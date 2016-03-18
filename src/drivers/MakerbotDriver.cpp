/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

/*
 * TODO:
 * - Implement progress approximation (see cmdToLineRatio_, currently commented out because they contain bugs).
 * - In convertGCode: instead of separating the conversion buffer into separate commands again (with the parser), it would be better to adapt gpx to emit() these commands one by one (or something similar)...
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
#include <algorithm>
#include <iomanip>
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
# define LOG(lvl, fmt, ...) log_.log(lvl, "MBTD", fmt, ##__VA_ARGS__)
#endif

const int MakerbotDriver::PRINTER_BUFFER_SIZE = 512;
const int MakerbotDriver::GCODE_CVT_LINES = 25;

//Note: these values are quite small in order to reduce accuracy errors in progress reports.
const size_t MakerbotDriver::QUEUE_MIN_SIZE = 10;
const size_t MakerbotDriver::QUEUE_FILL_SIZE = 30;


MakerbotDriver::MakerbotDriver(Server& server, const std::string& serialPortPath, const uint32_t& baudrate)
: AbstractDriver(server, serialPortPath, baudrate), bufferSpace_(PRINTER_BUFFER_SIZE),
  cmdToLineRatio_(0.0f), validResponseReceived_(false), firmwareVersion_(0)
{
	gpx_clear_state();
	gpx_setSuppressEpilogue(1); // prevent commands like build is complete. only necessary once
	gpx_setBuildName("    Doodle3D"); //NOTE: 4 spaces seem to fix some display offset issue on at least one r2x
	gcodeBuffer_.setKeepGpxMacroComments(true);
}

static int lastCode = -1;
static int counter = 0;

int MakerbotDriver::update() {
	if (!isConnected()) return -1;

	if ((state_ == PRINTING || state_ == STOPPING) && queue_.size() < QUEUE_MIN_SIZE) {
		int32_t amt = -1;
		while (amt != 0 && queue_.size() < QUEUE_FILL_SIZE) {
			string lines;
			amt = gcodeBuffer_.getNextLine(lines, GCODE_CVT_LINES);
			int cmds = convertGCode(lines);
			if (!lines.empty()) LOG(Logger::BULK, "converted %i lines into %i commands: '%s'", amt, cmds, lines.c_str()); //TEMP

			if (amt > 0) {
				gcodeBuffer_.eraseLine(amt);
				gcodeBuffer_.setCurrentLine(getCurrentLine() + amt);

				//update approximation of s3g commands per line
				float newRatio = (float)amt / cmds;
				float weight = cmds > 0 ? (float)cmds / queue_.size() : 0.0f;
				cmdToLineRatio_ = cmdToLineRatio_ * (1.0f - weight) + newRatio * weight;
			}
		}
	}

	if (state_ == PRINTING || state_ == STOPPING) {
		processQueue();
		//TODO: setState(IDLE) if all code has been processed
	}

	if (counter == 30) { // TODO: replace with time based timer?
		if (!validResponseReceived_) {
			unsigned int ver = getFirmwareVersion();
			LOG(Logger::INFO, "Makerbot firmware version %.2f", ver / 100.0f);
		}
		updateTemperatures();
		requestBufferSpace();

		LOG(Logger::VERBOSE, "  hTemps: %i/%i, bTemps: %i/%i, queue: %i, c2l ratio: %.2f, prbuf space (actual): %i",
				temperature_, targetTemperature_, bedTemperature_, targetBedTemperature_,
				queue_.size(), cmdToLineRatio_, bufferSpace_);

		counter = 0;
	}
	counter++;

	return 1000 / 30; //the of test also runs at 30fps...
}

size_t MakerbotDriver::convertGCode(const string &gcode) {
	size_t oldQueueSize = queue_.size();

	if (gcode.size() == 0) return 0;

	unsigned char *cvtBuf = 0;
	long cvtBufLen = 0;
	gpx_convert(gcode.c_str(), gcode.size(), &cvtBuf, &cvtBufLen);

	parser_.setBuffer((char*)cvtBuf, cvtBufLen);
	while(parser_.parseNextCommand());
	queueCommands(parser_.commands);
	parser_.commands.clear();

	free(cvtBuf);
	return queue_.size() - oldQueueSize;
}

GCodeBuffer::GCODE_SET_RESULT MakerbotDriver::setGCode(const string &gcode, int32_t totalLines, GCodeBuffer::MetaData *metaData) {
	GCodeBuffer::GCODE_SET_RESULT gsr = AbstractDriver::setGCode(gcode, totalLines, metaData);
	fullStop();
	return gsr;
}

void MakerbotDriver::clearGCode() {
	AbstractDriver::clearGCode();
	fullStop();
}


int32_t MakerbotDriver::getCurrentLine() const {
	int32_t cl = gcodeBuffer_.getCurrentLine();
	//cl -= std::min(cl, (int32_t)((queue_.size() + PRINTER_BUFFER_SIZE - bufferSpace_) * cmdToLineRatio_));
	return cl;
}

int32_t MakerbotDriver::getBufferedLines() const {
	int32_t cl = gcodeBuffer_.getBufferedLines();
	//cl += (queue_.size() + PRINTER_BUFFER_SIZE - bufferSpace_) * cmdToLineRatio_;
	return cl;
}


//STATIC
const AbstractDriver::DriverInfo& MakerbotDriver::getDriverInfo() {
	static AbstractDriver::vec_FirmwareDescription supportedFirmware;
	static AbstractDriver::DriverInfo info;

	if (supportedFirmware.empty()) {
		info.name = "Makerbot";

		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("_3Dison_plus") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("makerbot_replicator2") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("makerbot_replicator2x") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("makerbot_thingomatic") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("makerbot_generic") );
        supportedFirmware.push_back( AbstractDriver::FirmwareDescription("wanhao_duplicator4") );

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
	return true;
}

void MakerbotDriver::sendCode(const std::string& code, bool logAsInfo) {
//	LOG(logAsInfo ? Logger::INFO : Logger::BULK, "sendCode(): %s",code.c_str());
//	if (isConnected()) {
//		extractGCodeInfo(code);
//		serial_.send((code+"\n").c_str());
//	}
}

//TODO: actually implement this to interpret response packets instead of the local function which does that now?
void MakerbotDriver::readResponseCode(std::string& code) {
}

//clean out all buffers and try to abort currently active printing
void MakerbotDriver::fullStop() {
	queue_.clear();
	gpx_clear_state();

	if(isConnected()) {
		resetPrinterBuffer();
		abort();
	}
}


/*********************
 * PRIVATE FUNCTIONS *
 *********************/

void MakerbotDriver::processQueue() {
	if (!queue_.empty()) {
		int oldQSize = queue_.size(), oldBSpace = bufferSpace_; //TEMP
		// refill the printer buffer when there is enough space
		if (requestBufferSpace() > 480) { //TODO: rewrite 480 into a factor of PRINTER_BUFFER_SIZE
			while (true) {
				if (queue_.empty()) break;
				string command = queue_.front();
				// check if there is space for this command (which vary in length)
				if (command.size() > bufferSpace_ - 5) break;

				queue_.pop_front();
				int len = command.size();
				uint8_t *payload = (uint8_t*)command.c_str();
				sendPacket(payload,len);
			}
		}
		if (oldQSize - queue_.size()) {
			LOG(Logger::VERBOSE, "processed %i cmds (size=%i), printbuf: %i => %i", oldQSize - queue_.size(), queue_.size(), oldBSpace, bufferSpace_);
		}
	} else {
		if (requestBufferSpace() < PRINTER_BUFFER_SIZE) {
			if (counter == 0) LOG(Logger::INFO, "Print queue empty, waiting for printer to finish...");
		} else {
			setState(IDLE);
			LOG(Logger::INFO, "Print queue and printer buffer empty. Done!");
		}
	}
}



uint8_t MakerbotDriver::_crc_ibutton_update(uint8_t crc, uint8_t data) {
	uint8_t i;
	crc = crc ^ data;
	for (i = 0; i < 8; i++) {
		if (crc & 0x01) crc = (crc >> 1) ^ 0x8C;
		else crc >>= 1;
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
	LOG(Logger::VERBOSE, "not sending reset_print_buffer command to prevent serial port from being closed");
//	uint8_t payload[] = { 3 };
//	sendPacket(payload,sizeof(payload));
}

void MakerbotDriver::abort() {
	LOG(Logger::VERBOSE, "not sending abort command to prevent serial port from being closed");
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
		LOG(Logger::INFO, "response message to cmd 0x%x/0x%x: 0x%x (=%s)", cmd, toolcmd, code, getResponseMessage(code).c_str());
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
			if(getState() == CONNECTING) setState(IDLE); // we have communication
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
			LOG(Logger::WARNING, "ignored response for cmd: %i (toolcmd: %i, len: %i)", cmd, toolcmd, len);
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
		if (Logger::getInstance().getLevel() == Logger::BULK) {
			bool tempQuery = pktBuf[2] == 10 && (pktBuf[4] == 2 || pktBuf[4] == 30 || pktBuf[4] == 32 || pktBuf[4] == 33);
			if (pktBuf[2] != 2 && !tempQuery) {
				stringstream strstr;
				for (int i = 0; i < len + 3; ++i) {
					strstr << " " << std::noshowbase << std::hex << std::setw(2) << std::setfill('0') << (int)pktBuf[i];
				}
				LOG(Logger::BULK, "|%i| writing packet with len %i:%s", 5 - retriesLeft, len + 3, strstr.str().c_str());
			}
		}

		serial_.write(pktBuf, len + 3);
		//NOTE: in case of a tool action command (10), also pass the tool command code (payload[2])
		rv = parseResponse(cmd, cmd == 10 ? payload[2] : -1);
		switch (rv) {
			case 0:
				if (!validResponseReceived_) LOG(Logger::INFO, "hello makerbot! (received valid response packet)");
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
	log_.checkError(rv, "MBTD", "cannot read from device");
	if (rv < 0) handleReadError(rv);
	return rv;
}

int MakerbotDriver::readAndCheckError(unsigned char *buf, size_t buflen, int timeout) {
	int rv = serial_.readBytesDirect(buf, buflen, timeout);
	log_.checkError(rv, "MBTD", "cannot read from device");
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
