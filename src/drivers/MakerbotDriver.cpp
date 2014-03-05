/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

/*
 * TODO:
 * - only build aux/uci.git on osx (with BUILD_LUA disabled, and possibly patch the makefile to disable building the executable?). for openwrt, add a dependency on libuci instead
 * - read uci config in server to create the correct type of driver (see lua frontend for reference)
 *   -> also read baud rate to allow setting it to a fixed value? (i.e., disable auto-switching in marlindriver)
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
  bufferSpace_(512), currentCmd_(0), totalCmds_(0), validResponseReceived_(false), firmwareVersion_(0)
{
	gpx_clear_state();
	gpx_setSuppressEpilogue(1); // prevent commands like build is complete. only necessary once
	gpx_setBuildName("    Doodle3D"); //NOTE: 4 spaces seem to fix some display offset issue on at least one r2x
}

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

	// if data in gpx buffer, parse and seperate in packages
	//use parser as one-shot converter from command stream to separate commands and add them to our queue
	// TODO: move to appendGCode?
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

	if (counter == 30) { // TODO: replace for time based timer?
		if (!validResponseReceived_) {
			unsigned int ver = getFirmwareVersion();
			LOG(Logger::INFO, "Makerbot firmware version %.2f", ver / 100.0f);
		}
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

size_t MakerbotDriver::convertGcode(const string &gcode) {
	if (gcode.size()==0) return 0;

	// create convert buffer
	unsigned char *cvtBuf = 0;
	long cvtBufLen = 0;
	gpx_convert(gcode.c_str(), gcode.size(), &cvtBuf, &cvtBufLen);

//	//TEMP (give this file dump code a permanent place - it's useful. and move to startPrint so we can also debug chunked data transfers)
//	LOG(Logger::VERBOSE, "got back %i bytes, cvtBuf=%p", cvtBufLen, cvtBuf);
////	fprintf(stderr, "conversion result:");
////	for (int i = 0; i < cvtBufLen; i++) { //this loop is only useful when sending single or few commands
////		fprintf(stderr, " %02X", cvtBuf[i]);
////	}
//	fprintf(stderr, " (dumped to /tmp/gpxdump.log)\n");
//	FILE *dump = fopen("/tmp/gpxdump.log", "w");
//	if (fwrite(cvtBuf, cvtBufLen, 1, dump) == 0) perror("could not write data");
//	fclose(dump);
//	exit(13);

	// add converted gpx code to gpxbuffer
	// this is seperate into commands in update method
	gpxBuffer_ = (unsigned char*)realloc(gpxBuffer_, gpxBufferSize_ + cvtBufLen);
	memcpy(gpxBuffer_ + gpxBufferSize_, cvtBuf, cvtBufLen);
	free(cvtBuf); //cvtBuf = 0; cvtBufLen = 0;
	gpxBufferSize_ += cvtBufLen;

	return cvtBufLen;
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

	if(isConnected()) {
		resetPrinterBuffer();
		abort();
	}
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
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("makerbot_replicator2") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("makerbot_replicator2x") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("makerbot_thingomatic") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("makerbot_generic") );

		info.supportedFirmware = supportedFirmware;
		info.creator = &MakerbotDriver::create;
	};

	return info;
}

AbstractDriver* MakerbotDriver::create(Server& server, const std::string& serialPortPath, const uint32_t& baudrate) {
	return new MakerbotDriver(server, serialPortPath, baudrate);
}

bool MakerbotDriver::startPrint(const std::string& gcode, AbstractDriver::STATE state) {
	setGCode(gcode);

	return startPrint(state);
}

//FIXME: for set/append/clear gcode and start/stop print functions: buffer management is very hacky
//because of lack of distinction between abstract driver and implementations. The abstract stopprint
//clear the gcode buffer, which doesn't matter for us....but it is ugly and should be rewritten.
bool MakerbotDriver::stopPrint(const std::string& endcode) {
	clearGCode();
	setGCode(endcode);
	return AbstractDriver::stopPrint(endcode);
}

bool MakerbotDriver::startPrint(STATE state) {
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

	return AbstractDriver::startPrint(state);
}

bool MakerbotDriver::stopPrint() {
	return stopPrint("");
}

bool MakerbotDriver::resetPrint() {
	if (!AbstractDriver::resetPrint()) return false;
	currentCmd_ = 0;
	return true;
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
		// refill the printer buffer when there is enough space
		if (requestBufferSpace() > 480) {
			while (true) {
				if (queue_.empty()) break;
				string command = queue_.front();
				// check if their is space for this command (which vary in length)
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
	for (size_t i = 0; i < commands.size(); i++) {
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
	return firmwareVersion_;
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

//NOTE: according to http://replicat.org/sanguino3g, command 03 is not used
void MakerbotDriver::resetPrinterBuffer() { //03 - Clear buffer
	//LOG(Logger::BULK, "not sending reset_print_buffer command to prevent serial port from being closed");
//	uint8_t payload[] = { 3 };
//	sendPacket(payload,sizeof(payload));
}

void MakerbotDriver::abort() {
	//LOG(Logger::BULK, "not sending abort command to prevent serial port from being closed");
//	//07 - Abort immediately: Stop machine, shut down job permanently
//	uint8_t payload[] = { 7 };
//	sendPacket(payload,sizeof(payload));
//	queue_.clear();
}

//return values: 0 on success, -1 on system error, -2 on timeout, -3 on crc error
int MakerbotDriver::parseResponse(int cmd, int toolcmd) {
	static int timeout = 250;
	//read 0xD5
	int rv = readAndCheckError(timeout); //value 1000 is taken from s3g python script (StreamWriter.py)

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
	int len = readAndCheckError(timeout);
	if (len <= 0) {
		LOG(Logger::WARNING, "parseResponse: response get package length: len=%i", len);
		return len;
	}

	//read packet data from serial
	unsigned char buf[len];
	rv = readAndCheckError(buf, len, timeout);
	if (rv <= 0) {
		LOG(Logger::WARNING, "parseResponse: read packet serial response value=%i", rv);
		return rv;
	}

	//read crc from serial
	int crc = readAndCheckError(timeout);
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
		LOG(Logger::INFO, "response message 0x%x (=%s)", code, getResponseMessage(code).c_str());
	}

	//depending on previously send cmd interpret the packet
	//s3g cmd info: https://github.com/makerbot/s3g/blob/master/doc/s3gProtocol.md
	switch (cmd) {
		case 0: firmwareVersion_ = read16(buf+1); break;
		case 2: { bufferSpace_ = read32(buf+1); break; }
		case 10: { //Tool query: Query a tool for information
			uint16_t t = read16(buf+1);
			if (toolcmd==2) temperature_ = t;
			else if (toolcmd==30) bedTemperature_ = t;
			else if (toolcmd==32) targetTemperature_ = t;
			else if (toolcmd==33) targetBedTemperature_ = t;
			else LOG(Logger::WARNING, "parseResponse: unrecognized or missing tool command (%u)", toolcmd);
			break; }
		case 3: break; //clear buffer
		case 7: break; //abort
		case 136: break; //tool action command
		case 155: break; //queue extended point
		case 153: case 150: case 154: break; //ignore start/set/end build commands
		case 149: case 151: break; //ignore display message and queue song commands
		case 144: case 145: break; //ignore recall home pos. and set stepper pots
		//en: set-ext.pos(140) / ena/dis steppers(137) / find axes mins(131) / find axes maxes(132)
		//en: change tool(134) / wait for tool ready(135)
		default:
			LOG(Logger::WARNING, "ingored response for cmd: %i (toolcmd: %i, len: %i)", cmd, toolcmd, len);
			break;
	}

	return 0; //everything should be fine now
}


//FIXME: retrying should be used for retry-able errors only
//updateBufferSpace defaults to true, set to false for non-buffered commands
bool MakerbotDriver::sendPacket(uint8_t *payload, int len, bool updateBufferSpace) {
	//TODO: see if flushing also works (unfortunately this issue could not be reproduced consistently)
	//TODO: also flush on retries? (see s3g/conveyor)
	//serial_.flushReadBuffer();

	//check for unexpected bytes in the serial read buffer and eat them.
	unsigned char buf[100];
	int rv2 = readAndCheckError(buf, len, 0);
	if (rv2 > 0) {
		LOG(Logger::WARNING, "sendPacket: There where %i unexpected bytes in the serial read buffer", rv2);
	}

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
			case -1: LOG(Logger::ERROR, "makerbot response system error (%s)", strerror(errno)); break;
			case -2: LOG(Logger::ERROR, "makerbot response timeout"); break;
			case -3: LOG(Logger::ERROR, "makerbot response CRC error"); break;
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

//NOTE: somehow it looks like we don't need to swap int16 as opposed to int32
uint16_t MakerbotDriver::read16(unsigned char *buf) {
#ifndef __LITTLE_ENDIAN__
	//LOG(Logger::INFO, "swapping bytes %02X %02X", *(buf), *(buf+1));
	//unsigned char swap;
	//swap = *(buf); *(buf) = *(buf+1); *(buf+1) = swap;
	//LOG(Logger::INFO, "swapped bytes %02X %02X", *(buf), *(buf+1));
#endif
	return *(buf) + (*(buf+1)<<8);
	//return *reinterpret_cast<unsigned*>(buf);
}

uint32_t MakerbotDriver::read32(unsigned char *buf) {
#ifndef __LITTLE_ENDIAN__
//	LOG(Logger::INFO, "swapping bytes %02X %02X %02X %02X", *(buf), *(buf+1), *(buf+2), *(buf+3));
	unsigned char swap;
	swap = *(buf); *(buf) = *(buf+3); *(buf+3) = swap;
	swap = *(buf+1); *(buf+1) = *(buf+2); *(buf+2) = swap;
#endif
	return *reinterpret_cast<unsigned*>(buf);
}
