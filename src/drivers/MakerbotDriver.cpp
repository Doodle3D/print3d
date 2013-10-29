/*
 * TODO:
 * - only build aux/uci.git on osx (with BUILD_LUA disabled, and possibly patch the makefile to disable building the executable?). for openwrt, add a dependency on libuci instead
 * - read uci config in server to create the correct type of driver (see lua frontend for reference)
 *   -> also read baud rate to allow setting it to a fixed value? (i.e., disable auto-switching in marlindriver)
 * - test on wifibox
 *
 * - sometimes readings are swapped (e.g. hTgt as hAct, or hAct as bAct, or hAct as bufFree)
 *   -> seems like this always happens together with an error, perhaps the response arrives anyway and gets assigned incorrectly?
 *
 * - the t-o-m takes about 12 seconds to start responding, do we need an extra state CONNECTING?
 * - set/append/clear gcode functions are a bit weird now. refactor this
 *
 * Protocol spec: https://github.com/makerbot/s3g/blob/master/doc/s3gProtocol.md
 * online iButton CRC calculator: http://www.datastat.com/sysadminjournal/maximcrc.cgi
 */
#include <iostream>
#include <sstream>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "MakerbotDriver.h"
#include "../server/Server.h"
#include "../aux/GPX.git/libgpx.h"

using std::cout;
using std::endl;
using std::string;
using std::vector;

//NOTE: see Server.cpp for comments on this macro
#ifndef LOG
# define LOG(lvl, fmt, ...) log_.log(lvl, "[MBD] " fmt, ##__VA_ARGS__)
#endif

//NOTE: this should an amount that fits in one GcodeBuffer's bucket because getNextLine() cannot cross bucket boundaries
//const int MakerbotDriver::WAIT_GCODE_LINES = 1000; //approximation: 1000 lines * 35 chars < 1024 bytes * 50
const int MakerbotDriver::MAX_GPX_BUFFER_SIZE = 1024 * 50;


MakerbotDriver::MakerbotDriver(Server& server, const std::string& serialPortPath, const uint32_t& baudrate)
: AbstractDriver(server, serialPortPath, baudrate), gpxBuffer_(0), gpxBufferSize_(0),
  bufferSpace_(512), currentCmd_(0), totalCmds_(0), validResponseReceived_(false)
{}

static int lastCode = -1;

int MakerbotDriver::update() {
	static int counter = 0;//TEMP

	if (!isConnected()) return -1;

	//Notes on using the gcodeBuffer (and why it is currently unused in this driver):
	//1) it would require getNextLine and eraseLine to process as many lines as possible of the requested amount
	//2) similarly, WAIT_GCODE_LINES should not be strict (or the end of the gcode would never be processed)
	//3) a trick would have to be devised to report progress accurately (instead of 'progressing' WAIT_GCODE_LINES lines at a time)
//	if (gcodeBuffer_.getBufferedLines() >= WAIT_GCODE_LINES && gpxBufferSize_ < MAX_GPX_BUFFER_SIZE) {
//		string gcode;
//		if (!gcodeBuffer_.getNextLine(gcode, WAIT_GCODE_LINES)) {
//			LOG(Logger::ERROR, "could not extract %i lines from gcode buffer while it claims to have them", WAIT_GCODE_LINES);
//		} else {
//			gcodeBuffer_.eraseLine(WAIT_GCODE_LINES);
//			convertGcode(gcode);
//		}
//	}

	//use parser as one-shot converter from command stream to separate commands and add them to our queue
	parser_.setBuffer((char*)gpxBuffer_, gpxBufferSize_);
	while(parser_.parseNextCommand());
	clearGpxBuffer();
	queueCommands(parser_.commands);
	parser_.commands.clear();

	STATE s = getState();
	if (s == PRINTING || s == STOPPING) {
		processQueue();
		//TODO: setState(IDLE) if all code has been processed
	}

	if (counter == 30) {
		updateTemperatures();
		LOG(Logger::VERBOSE, "  hTemps: %i/%i, bTemps: %i/%i, cmdbuf: %i/%i/%i, prbuf space: %i",
				temperature_, targetTemperature_,
				bedTemperature_, targetBedTemperature_,
				currentCmd_, queue_.size(), totalCmds_, bufferSpace_);

		requestBufferSpace();

		counter = 0;
	}
	counter++;

	return 1000 / 30; //the of test also runs at 30fps...
}

void MakerbotDriver::clearGpxBuffer() {
	free(gpxBuffer_); gpxBuffer_ = 0; gpxBufferSize_ = 0;
}

//#define USE_EXTERNAL_GPX //TEMP
size_t MakerbotDriver::convertGcode(const string &gcode) {
	if (gcode.size()==0) return 0;

#ifndef USE_EXTERNAL_GPX

	unsigned char *cvtBuf = 0;
	long cvtBufLen = 0;
	gpx_setSuppressEpilogue(1); //only necessary once
	gpx_convert(gcode.c_str(), gcode.size(), &cvtBuf, &cvtBufLen);
	LOG(Logger::VERBOSE, "got back %i bytes, cvtBuf=%p", cvtBufLen, cvtBuf);//TEMP

	gpxBuffer_ = (unsigned char*)realloc(gpxBuffer_, gpxBufferSize_ + cvtBufLen);
	memcpy(gpxBuffer_ + gpxBufferSize_, cvtBuf, cvtBufLen);
	free(cvtBuf); //cvtBuf = 0; cvtBufLen = 0;
	gpxBufferSize_ += cvtBufLen;

	return cvtBufLen;

#else

	int rv;
	errno = 0; //'clear' error?
	FILE *io = popen("gpx -s -n > /tmp/gpx-3rw58.out", "w");
	if (!io) { LOG(Logger::ERROR, "could not popen gpx (possible errno: %s)", strerror(errno)); exit(1); }

	clearerr(io);
	rv = fwrite(gcode.c_str(), gcode.size(), 1, io);
	if (rv < 0) { LOG(Logger::ERROR, "gpx write error (%s)", strerror(errno)); exit(1); }
	if (rv == 0 ) { LOG(Logger::ERROR, "gpx short write (%i bytes written)", rv); exit(1); }

	rv = pclose(io);
	LOG(Logger::VERBOSE, "converted %i bytes of gcode (gpx exit status: %i)", gcode.size(), rv);

	FILE *in = fopen("/tmp/gpx-3rw58.out", "r");
	if (!in) { LOG(Logger::ERROR, "could not open gpx output file (%s)", strerror(errno)); exit(1); }

	fseek(in, 0, SEEK_END);
	size_t fsize = ftell(in);
	rewind(in);

	gpxBuffer_ = (unsigned char*)realloc(gpxBuffer_, gpxBufferSize_ + fsize);

	//NOTE: this only works when all gcode is being added at once, otherwise gpx will lose its internal state with each block
	clearerr(in);
	rv = fread(gpxBuffer_ + gpxBufferSize_, fsize, 1, in);
	if (rv < 0) { LOG(Logger::ERROR, "gpx output file read error (%s)", strerror(errno)); exit(1); }

	if (rv == 0 && ferror(in)) { LOG(Logger::ERROR, "gpx output file short read (%i bytes read)", rv); exit(1); }

	gpxBufferSize_ += fsize;

	fclose(in);

	return fsize;
#endif
}

void MakerbotDriver::setGCode(const string &gcode) {
	currentCmd_ = totalCmds_ = 0;
	clearGpxBuffer();
	int rv = convertGcode(gcode);
	if (getState() == IDLE) setState(BUFFERING);
	LOG(Logger::VERBOSE, "set %i bytes of gpx data (bufsize now %i)", rv, gpxBufferSize_);
}

void MakerbotDriver::appendGCode(const string &gcode) {
	int rv = convertGcode(gcode);
	if (getState() == IDLE) setState(BUFFERING);
	LOG(Logger::VERBOSE, "appended %i bytes of gpx data (bufsize now %i)", rv, gpxBufferSize_);
}

//FIXME: clear/set/append gcode and start/stop print functions need a big overhaul. it's unreadable and unmaintainable atm
void MakerbotDriver::clearGCode() {
	STATE s = getState();
	if (s == BUFFERING || s == PRINTING || s == STOPPING) setState(IDLE);

	currentCmd_ = totalCmds_ = 0;
	clearGpxBuffer();
	queue_.clear();
	gpx_clear_state(); //FIXME: this should probably also be called on init
	resetPrinterBuffer();
	abort();
}


//FIXME: having the makerbot printer buffer is very nice, but it does make this value slightly inaccurate (esp. when starting/stopping)
int32_t MakerbotDriver::getCurrentLine() const {
	return currentCmd_;
}

int32_t MakerbotDriver::getBufferedLines() const {
	return queue_.size();
}

int32_t MakerbotDriver::getTotalLines() const {
	return totalCmds_;
}


//STATIC
const AbstractDriver::DriverInfo& MakerbotDriver::getDriverInfo() {
	static AbstractDriver::vec_FirmwareDescription supportedFirmware;
	static AbstractDriver::DriverInfo info;

	if (supportedFirmware.empty()) {
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("makerbot_generic") );

		info.supportedFirmware = supportedFirmware;
		info.creator = &MakerbotDriver::create;
	};

	return info;
}

AbstractDriver* MakerbotDriver::create(Server& server, const std::string& serialPortPath, const uint32_t& baudrate) {
	return new MakerbotDriver(server, serialPortPath, baudrate);
}

void MakerbotDriver::startPrint(const std::string& gcode, AbstractDriver::STATE state) {
	setGCode(gcode);

	startPrint(state);
}

//FIXME: for set/append/clear gcode and start/stop print functions: buffer management is very hacky
//because of lack of distinction between abstract driver and implementations. The abstract stopprint
//clear the gcode buffer, which doesn't matter for us....but it is ugly and should be rewritten.
void MakerbotDriver::stopPrint(const std::string& endcode) {
	clearGCode();
	setGCode(endcode);
	AbstractDriver::stopPrint(endcode);
}

void MakerbotDriver::startPrint(STATE state) {
//	FILE *queueDump = fopen("/tmp/queuedump.log", "w");
//	LOG(Logger::VERBOSE, "dumping %i commands...", queue_.size());//TEMP
//	int bytes = 0;
//	while(true) {
//		if (queue_.size() == 0) break;
//		string cmd = queue_.front();
//		bytes += cmd.size();//TEMP
//		queue_.pop_front();
//		fwrite(cmd.c_str(), cmd.size(), 1, queueDump);
//	}
//	fclose(queueDump);
//	LOG(Logger::VERBOSE, "dumped %i bytes...", bytes);//TEMP
//	exit(11);

	AbstractDriver::startPrint(state);
}

void MakerbotDriver::stopPrint() {
	stopPrint("");
}

void MakerbotDriver::resetPrint() {
	AbstractDriver::resetPrint();
	currentCmd_ = 0;
}

void MakerbotDriver::sendCode(const std::string& code) {
//	LOG(Logger::BULK, "sendCode(): %s",code.c_str());
//	if (isConnected()) {
//		extractGCodeInfo(code);
//		serial_.send((code+"\n").c_str());
//	}
}

void MakerbotDriver::readResponseCode(std::string& code) {
}

/*********************
 * PRIVATE FUNCTIONS *
 *********************/

void MakerbotDriver::processQueue() {
	if (!queue_.empty()) {
		int oldQSize = queue_.size(), oldBSpace = bufferSpace_;//TEMP
		if (requestBufferSpace() > 480) {
			while (true) {
				if (queue_.empty()) break;
				string command = queue_.front();
				if (command.size() > bufferSpace_ - 5) break;

				queue_.pop_front();
				int len = command.size();
				uint8_t *payload = (uint8_t*)command.c_str();
				sendPacket(payload,len);
				currentCmd_++;
			}
		}
		if (oldQSize - queue_.size()) {
			LOG(Logger::VERBOSE, "processed %i cmds (size=%i), printbuf: %i => %i", oldQSize - queue_.size(), queue_.size(), oldBSpace, bufferSpace_);
	//		LOG(Logger::VERBOSE, "actual buffer space: %i", getBufferSpace());
		}
	} else {
		setState(IDLE);
		LOG(Logger::VERBOSE, "Print queue empty. Done!");
	}
}



uint8_t MakerbotDriver::_crc_ibutton_update(uint8_t crc, uint8_t data) {
	uint8_t i;
	crc = crc ^ data;
	for (i = 0; i < 8; i++)
	{
		if (crc & 0x01)
			crc = (crc >> 1) ^ 0x8C;
		else
			crc >>= 1;
	}
	return crc;
}



string MakerbotDriver::getResponseMessage(int code) {
	switch (code) {
		case 0x80: return "Generic Packet error, packet discarded";
		case 0x81: return "Success";
		case 0x82: return "Action buffer overflow, entire packet discarded";
		case 0x83: return "CRC mismatch, packet discarded";
		case 0x84: return "Query packet too big, packet discarded";
		case 0x85: return "Command not supported/recognized";
		case 0x87: return "Downstream timeout";
		case 0x88: return "Tool lock timeout";
		case 0x89: return "Cancel build";
		case 0x8A: return "Bot is Building from SD";
		case 0x8B: return "Bot is shutdown due to Overheat";
		case 0x8C: return "Packet timeout error, packet discarded";
	}

	std::ostringstream out;
	out << "Unknown response code: " << code;
	return out.str();
}

void MakerbotDriver::queueCommands(vector<string> commands) {
	for (int i = 0; i < commands.size(); i++) {
		queue_.push_back(commands.at(i));
	}
	totalCmds_ += commands.size();
}

bool MakerbotDriver::updateTemperatures() {
	int rv = true;
	uint8_t payload[] = { 10, 0, 0 };

	payload[2] = 2; //tool #0 temp
	if (!sendPacket(payload,sizeof(payload), false)) rv = false;

	payload[2] = 30; //build platform temp
	if (!sendPacket(payload,sizeof(payload), false)) rv = false;

	payload[2] = 32; //tool #0 target temp
	if (!sendPacket(payload,sizeof(payload), false)) rv = false;

	payload[2] = 33; //build platform target temp
	if (!sendPacket(payload,sizeof(payload), false)) rv = false;

	return rv;
}

int MakerbotDriver::getFirmwareVersion() {
	//00 - Get version: Query firmware for version information
	uint8_t payload[] = { 0 };
	sendPacket(payload,sizeof(payload), false);
	return 0; //FIXME
}

int MakerbotDriver::requestBufferSpace() {
	////02 - Get available buffer size: Determine how much free memory is available for buffering commands
	uint8_t payload[] = { 2 };
	sendPacket(payload,sizeof(payload));
	return bufferSpace_; //updated by sendPacket->parseResponse
}

void MakerbotDriver::playSong(uint8_t song) { ////151 - Queue Song
	uint8_t payload[] = {151, song};
	sendPacket(payload,sizeof(payload));
}

void MakerbotDriver::resetPrinterBuffer() { //03 - Clear buffer
	uint8_t payload[] = { 3 };
	sendPacket(payload,sizeof(payload));
}

void MakerbotDriver::abort() {
	//07 - Abort immediately: Stop machine, shut down job permanently
	uint8_t payload[] = { 7 };
	sendPacket(payload,sizeof(payload));
	queue_.clear();
}

//return values: 0 on success, -1 on system error, -2 on timeout, -3 on crc error
int MakerbotDriver::parseResponse(int cmd, int toolcmd) {

	//read 0xD5
	int rv = readAndCheckError(100); //value 1000 is taken from s3g python script (StreamWriter.py)

	if (rv<0) {
		LOG(Logger::WARNING, "parseResponse: serial.readByte return value=%d", rv);
		return rv;
	}

	if (rv!=0xD5) {
		LOG(Logger::WARNING, "parseResponse: expected 0xD5 instead of 0x%.02x", rv);
		return -99;
	}

	usleep(5000); //5000 microseconds = 5 milliseconds

	//read length of packet data
	int len = readAndCheckError(100);
	if (len <= 0) {
		LOG(Logger::WARNING, "parseResponse: response get package length: len=%i", len);
		return len;
	}

	//read packet data from serial
	unsigned char buf[len];
	rv = readAndCheckError(buf, len, 100);
	if (rv <= 0) {
		LOG(Logger::WARNING, "parseResponse: read packet serial response value=%i", rv);
		return rv;
	}

	//read crc from serial
	int crc = readAndCheckError(100);
	if (crc < 0) { //can CRC be zero? i think so
		LOG(Logger::WARNING, "parseResponse: CRC=%i", crc);
		return len;
	}

	//calculate expected crc
	int realCrc = 0;
//	if (cmd!=2) printf("response packet for cmd %i (len=%i): ",cmd,len);
	for (int i = 0; i < len; i++) {
		//cout << hex << ((int)buf[i]) << " ";
//		if (cmd!=2) printf("%.02x ",buf[i]);
		realCrc = _crc_ibutton_update(realCrc, buf[i]);
	}
//	if (cmd!=2) printf("[crc: %i/%i]",crc, realCrc);
//	if (cmd!=2) cout << endl;

	//check received crc with expected 'real' crc
	if (crc != realCrc) {
		LOG(Logger::WARNING, "CRC check failed (cmd %i)",cmd);
		return -3;
	}

	//read response code from packet. 0x81 is success
	int code = buf[0];
	if (code!=0x81) {
		cout << getResponseMessage(code) << endl;
	}

	//depending on cmd interpret packet
	switch (cmd) {
		case 2: { bufferSpace_ = read32(buf+1); break; }
		case 10: { //Tool query: Query a tool for information
			uint16_t t = read16(buf+1);
			if (toolcmd==2) temperature_ = t;
			else if (toolcmd==30) bedTemperature_ = t;
			else if (toolcmd==32) targetTemperature_ = t;
			else if (toolcmd==33) targetBedTemperature_ = t;
			else LOG(Logger::WARNING, "parseResponse: unrecognized or missing tool command (%u)", toolcmd);
			break; }
		default:
			LOG(Logger::WARNING, "other command: %i", cmd);
			break;
	}

	return 0; //everything should be fine now
}


//FIXME: retrying should be used for retry-able errors only
//updateBufferSpace defaults to true, set to false for non-buffered commands
bool MakerbotDriver::sendPacket(uint8_t *payload, int len, bool updateBufferSpace) {
	unsigned char *pktBuf = (unsigned char*)malloc(len + 3);
	pktBuf[0] = 0xD5;
	pktBuf[1] = len;
	uint8_t crc = 0;
	for (int i = 0; i < len; i++) {
		crc = _crc_ibutton_update(crc, payload[i]);
		pktBuf[2 + i] = payload[i];
	}
	pktBuf[2 + len] = crc;

	lastCode = payload[0];

	uint8_t cmd = payload[0];

	//LOG(Logger::VERBOSE, "send: cmd=%u, len=%u, crc=%u", cmd, len, crc);


	if (updateBufferSpace) bufferSpace_ -= len; //approximation of space left in buffer
	//cout << " [sent: " << lastCode << ", l:" << len << "] ";

	int retriesLeft = 5;
	int rv;
	while (retriesLeft > 0) {
		serial_.write(pktBuf, len + 3);
		//NOTE: in case of a tool action command (10), also pass the tool command code (payload[2])
		rv = parseResponse(cmd, cmd == 10 ? payload[2] : -1);
		switch (rv) {
			case 0:
				if (!validResponseReceived_) LOG(Logger::VERBOSE, "hello makerbot!");
				validResponseReceived_ = true;
				//LOG(Logger::VERBOSE, "ok");
				break;
			case -1: cout << "makerbot response system error (" << strerror(errno) << ")" << endl; break;
			case -2: cout << "makerbot response timeout" << endl; break;
			case -3: cout << "makerbot response CRC error" << endl; break;
		}
		if (rv == 0 || rv == -3) break; //do not retry if the _response_ crc was invalid, packet has probably been received successfully by printer

		retriesLeft--;
		if (retriesLeft > 0) LOG(Logger::WARNING, "resending packet (%i tries left) (rv=%i for cmd: %u)", retriesLeft, rv, cmd);
	}

	//cout << endl;
	free(pktBuf);
	return rv == 0 ? true : false;
}

int MakerbotDriver::readAndCheckError(int timeout) {
	int rv = serial_.readByteDirect(timeout);
	log_.checkError(rv, "cannot read from device");
	if (rv < 0) handleReadError(rv);
	return rv;
}

int MakerbotDriver::readAndCheckError(unsigned char *buf, size_t buflen, int timeout) {
	int rv = serial_.readBytesDirect(buf, buflen, timeout);
	log_.checkError(rv, "cannot read from device");
	if (rv < 0) handleReadError(rv);
	return rv;
}

void MakerbotDriver::handleReadError(int rv) {
	if (rv == -2) {
		LOG(Logger::ERROR, "remote end closed connection, closing port");
		closeConnection();
		if (AbstractDriver::REQUEST_EXIT_ON_PORT_FAIL) server_.requestExit(1);
	} else if (rv == -1 && errno == ENXIO) {
		LOG(Logger::ERROR, "port was disconnected, closing port");
		closeConnection();
		if (AbstractDriver::REQUEST_EXIT_ON_PORT_FAIL) server_.requestExit(1);
	} else if (rv == -1 && errno == EBADF) {
		LOG(Logger::ERROR, "port file descriptor became invalid, closing port");
		closeConnection();
		if (REQUEST_EXIT_ON_PORT_FAIL) server_.requestExit(1);
	}
}

uint16_t MakerbotDriver::read16(unsigned char *buf) {
#ifndef __LITTLE_ENDIAN__
	LOG(Logger::INFO, "swapping bytes %02X %02X", *(buf), *(buf+1));
	unsigned char swap;
	swap = *(buf); *(buf) = *(buf+1); *(buf+1) = swap;
#endif
	return *reinterpret_cast<unsigned*>(buf);
}

uint32_t MakerbotDriver::read32(unsigned char *buf) {
#ifndef __LITTLE_ENDIAN__
	LOG(Logger::INFO, "swapping bytes %02X %02X %02X %02X", *(buf), *(buf+1), *(buf+2), *(buf+3));
	unsigned char swap;
	swap = *(buf); *(buf) = *(buf+3); *(buf+3) = swap;
	swap = *(buf+1); *(buf+1) = *(buf+2); *(buf+2) = swap;
#endif
	return *reinterpret_cast<unsigned*>(buf);
}
