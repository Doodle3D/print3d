/*
 * TODO:
 * - find out why extruder is inverted (gcode from repG, in which the same gcode works)
 *   -> this is fixed after unticking 'invert A' in repG in the board settings
 * - the 135 wait command seems to wait until toolhead is 100 degrees, where it also stays during printing
 *   -> perhaps head and bed are confused? (e.g. because M109 is used to set bed temp...)
 *   -> gpx does indeed translate M109 to head temp & wait; M140 is the bed equiv for M104 and M190 is the bed equiv for M109
 * - the t-o-m takes about 12 seconds to start responding, we need a way to detect this moment (rq firmware version until sane value?)
 * - change gpx to prevent it from emitting start/end commands (unless we actually want them)
 * - override clear gcode to reset command counters
 * - sometimes readings are swapped (e.g. hTgt as hAct, or hAct as bAct, or hAct as bufFree)
 *   -> seems like this always happens together with an error, perhaps the response arrives anyway and gets assigned incorrectly?
 * - implement packet send retrying (up to 5 times)
 * - integrate with AbstractDriver (keep vars updated, override gcodebuffer things (used for states and progress))
 * - read uci config in server to create the correct type of driver
 * - allow to sepcify fixed baud rate?
 * Protocol spec: https://github.com/makerbot/s3g/blob/master/doc/s3gProtocol.md
 */
#include <iostream>
#include <sstream>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "MakerbotDriver.h"

using std::cout;
using std::endl;
using std::string;
using std::vector;

//NOTE: see Server.cpp for comments on this macro
#ifndef LOG
# define LOG(lvl, fmt, ...) log_.log(lvl, "[MBD] " fmt, ##__VA_ARGS__)
#endif

//const int MakerbotDriver::UPDATE_INTERVAL = 200;

MakerbotDriver::MakerbotDriver(Server& server, const std::string& serialPortPath, const uint32_t& baudrate)
: AbstractDriver(server, serialPortPath, baudrate), gpxBuffer_(0), gpxBufferSize_(0), bufferSpace_(512), currentCmd_(0), totalCmds_(0)
{}

static int lastCode = -1;

int MakerbotDriver::update() {
	static int counter = 0;//TEMP

	if (!isConnected()) return -1;

	parser_.setBuffer((char*)gpxBuffer_, gpxBufferSize_);
	while(parser_.parseNextCommand());
	clearGpxBuffer();
	sendCommands(parser_.commands);
	parser_.commands.clear();
	processQueue();

	if (counter == 30) {
		updateTemperatures();
		LOG(Logger::VERBOSE, "  hTemps: %i/%i, bTemps: %i/%i, cmdbuf: %i/%i/%i, prbuf space: %i",
				temperature_, targetTemperature_,
				bedTemperature_, targetBedTemperature_,
				currentCmd_, queue_.size(), totalCmds_, bufferSpace_);

		counter = 0;
	}
	counter++;

	return 1000 / 30; //the of test also runs at 30fps...
}

void MakerbotDriver::clearGpxBuffer() {
	free(gpxBuffer_); gpxBuffer_ = 0; gpxBufferSize_ = 0;
}

size_t MakerbotDriver::convertGcode(const string &gcode) {
	int rv;
	errno = 0; //'clear' error?
	FILE *io = popen("gpx -m t6 -s > /tmp/gpx-3rw58.out", "w");
	if (!io) { LOG(Logger::ERROR, "could not popen gpx (possible errno: %s)", strerror(errno)); exit(1); }

	rv = fwrite(gcode.c_str(), gcode.size(), 1, io);
	if (rv < 0) { LOG(Logger::ERROR, "gpx write error (%s)", strerror(errno)); exit(1); }
	if (rv < 1) { LOG(Logger::ERROR, "gpx short write (%i bytes written)", rv); exit(1); }

	rv = pclose(io);
	LOG(Logger::VERBOSE, "converted %i bytes of gcode (gpx exit status: %i)", gcode.size(), rv);

	FILE *in = fopen("/tmp/gpx-3rw58.out", "r");
	if (!in) { LOG(Logger::ERROR, "could not open gpx output file (%s)", strerror(errno)); exit(1); }

	fseek(in, 0, SEEK_END);
	size_t fsize = ftell(in);
	rewind(in);

	gpxBuffer_ = (unsigned char*)realloc(gpxBuffer_, gpxBufferSize_ + fsize);
	rv = fread(gpxBuffer_ + gpxBufferSize_, fsize, 1, in);
	if (rv < 0) { LOG(Logger::ERROR, "gpx output file read error (%s)", strerror(errno)); exit(1); }
	if (rv < 1) { LOG(Logger::ERROR, "gpx output file short read (%i bytes written)", rv); exit(1); }

	gpxBufferSize_ += fsize;

	fclose(in);

	return fsize;
}

void MakerbotDriver::setGCode(const string &gcode) {
	currentCmd_ = totalCmds_ = 0;
	clearGpxBuffer();
	int rv = convertGcode(gcode);
	LOG(Logger::VERBOSE, "set %i bytes of gpx data (bufsize now %i)", rv, gpxBufferSize_);
//	AbstractDriver::setGCode(gcode);
//	extractGCodeInfo(gcode);
}

void MakerbotDriver::appendGCode(const string &gcode) {
	int rv = convertGcode(gcode);
	LOG(Logger::VERBOSE, "appended %i bytes of gpx data (bufsize now %i)", rv, gpxBufferSize_);
//	AbstractDriver::appendGCode(gcode);
//	extractGCodeInfo(gcode);
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
	int oldQSize = queue_.size(), oldBSpace = bufferSpace_;//TEMP
	if (!queue_.empty() && getBufferSpace()>480) {
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
}

//updateBufferSpace defaults to true, set to false for non-buffered commands
bool MakerbotDriver::sendPacket(uint8_t *payload, int len, bool updateBufferSpace) {
	lastCode = payload[0];
	serial_.write(0xD5);
	serial_.write(len);
	uint8_t crc = 0;
	for (int i = 0; i < len; i++) {
		crc = _crc_ibutton_update(crc, payload[i]);
		serial_.write(payload[i]);
	}
	serial_.write(crc);
	if (updateBufferSpace) bufferSpace_ -= len; //approximation of space left in buffer
	//cout << " [sent: " << lastCode << ", l:" << len << "] ";

	int rv = parseResponse(payload[0], payload[0] == 10 ? payload[2] : -1);
	switch (rv) {
		case -1: cout << "makerbot response system error (" << strerror(errno) << ")" << endl; break;
		case -2: cout << "makerbot response timeout" << endl; break;
		case -3: cout << "makerbot response CRC error" << endl; break;
	}
	//cout << endl;
	return rv == 0 ? true : false;
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

//return values: 0 on success, -1 on system error, -2 on timeout, -3 on crc error
int MakerbotDriver::parseResponse(int cmd, int toolcmd) {
	//serial_.readDataWithLen(1, 100); //timeout after 100ms (officialy 40ms but makerbots sometimes need longer occording to docs
	//serial_.readData(500);
//	if (serial_.getBufferSize() > 0) {
//		printf("code %3i, buffer(%3i):", cmd, serial_.getBufferSize());
//		unsigned char *buf = (unsigned char*)serial_.getBuffer();
//		for (int i = 0; i < serial_.getBufferSize(); i++) {
//			printf(" 0x%02X", buf[i]);
//		}
//		printf("\n");
//	}
	int rv = serial_.readByteDirect(1000); //value 1000 is taken from s3g python script (StreamWriter.py)
	if (rv == 0xD5) {
//		cout << "0xD5 found" << endl;
		//::usleep(5000); //prevent OF_SERIAL_NO_DATA
		//serial_.readDataWithLen(1, 0);
		int len = serial_.readByteDirect(1000); //using 1000 again is not right of course
		if (len <= 0) return len;
		unsigned char buf[len+1];
		//serial_.readData();
		rv = serial_.readBytesDirect(buf, len+1, 1000); //using 1000 again is not right of course
		if (rv <= 0) return rv;
		//serial_.readDataWithLen(len+1, 0);
		//serial_.extractBytes(buf, len+1);

		int crc = buf[len];
		int realCrc = 0;
		for (int i = 0; i < len; i++) realCrc = _crc_ibutton_update(realCrc, buf[i]);
		if (crc != realCrc) return -3;

		int code = buf[0];
		if (code!=0x81) cout << getResponseMessage(code) << endl;
		//cout << dec << "cmd: " << cmd << " -> 0x" << hex << code << endl;
		switch (cmd) {
			////////////// Host Query Commands
			case 1: break; //01 - init: Initialize firmware to boot state
			case 2: bufferSpace_ = *reinterpret_cast<unsigned*>(buf+1); break; //Get available buffer size: Determine how much free memory is available for buffering commands
			case 3: break; //03 - Clear buffer: Empty the command buffer
			case 7: break; //07 - Abort immediately: Stop machine, shut down job permanently
			case 8: break; //08 - pause/resume: Halt execution temporarily
			case 10: { //Tool query: Query a tool for information
				uint16_t t = *reinterpret_cast<unsigned*>(buf+1);
				switch (toolcmd) {
					case 2: temperature_ = t; break;
					case 30: bedTemperature_ = t; break;
					case 32: targetTemperature_ = t; break;
					case 33: targetBedTemperature_ = t; break;
					default: LOG(Logger::WARNING, "parseResponse: unrecognized or missing tool command (%u)", toolcmd); break;
				}
			}; break;
			case 11: break; //11 - Is finished: See if the machine is currently busy
			case 12: break; //12 - Read from EEPROM
			case 13: break; //13 - Write to EEPROM
			case 14: break; //14 - Capture to file
			case 15: break; //15 - End capture to file
			case 16: break; //16 - Play back capture
			case 17: break; //17 - reset
			case 18: break; //18 - Get next filename
			case 20: break; //20 - Get build name
			case 21: break; //21 - Get extended position: Get the current
			case 22: break; //22 - Extended stop: Stop a subset of systems
			case 23: break; //23 - Get motherboard status
			case 24: break; //24 - Get build statistics
			case 25: break; //25 - Get communication statistics
			case 27: break; //27 - Get advanced version number
			///////////// Host Buffered Commands
			case 131: break; //131 - Find axes minimums: Move specified axes in the negative direction until limit switch is triggered.
			case 132: break; //132 - Find axes maximums: Move specified axes in the positive direction until limit switch is triggered.
			case 133: break; //133 - delay: pause all motion for the specified time
			case 134: break; //134 - Change Tool
			case 135: break; //135 - Wait for tool ready: Wait until a tool is ready before proceeding
			case 136: break; //136 - Tool action command: Send an action command to a tool for execution
			case 137: break; //137 - Enable/disable axes: Explicitly enable or disable stepper motor controllers
			case 139: break; //139 - Queue extended point
			case 140: break; //140 - Set extended position
			case 141: break; //141 - Wait for platform ready: Wait until a build platform is ready before proceeding
			case 142: break; //142 - Queue extended point, new style
			case 143: break; //143 - Store home positions
			case 144: break; //144 - Recall home positions
			case 145: break; //145 - Set digital potentiometer value
			case 146: break; //146 - Set RGB LED value
			case 147: break; //147 - Set Beep
			case 148: break; //148 - Wait for button
			case 149: break; //149 - Display message to LCD
			case 150: break; //150 - Set Build Percentage
			case 151: break; //151 - Queue Song
			case 152: break; //152 - reset to Factory
			case 153: break; //153 - Build start notification
			case 154: break; //154 - Build end notification
			case 155: break; //155 - Queue extended point x3g
			case 157: break; //157 - Stream Version
			//////////////////// Tool Query Commands
			//case 00: break; // Get version: Query firmware for version information
			//case 02: break; // Get toolhead temperature
			//case 17: break; // Get motor speed (RPM)
			//case 22: break; // Is tool ready?
			//case 25: break; // Read from EEPROM
			//case 26: break; // Write to EEPROM
			//case 30: break; // Get build platform temperature
			//case 32: break; // Get toolhead target temperature
			//case 33: break; // Get build platform target temperature
			//case 35: break; // Is build platform ready?
			//case 36: break; // Get tool status
			//case 37: break; // Get PID state
			//
			////////////////////// Tool Action Commands
			//case 01: break; // init: Initialize firmware to boot state
			//case 03: break; // Set toolhead target temperature
			//case 06: break; // Set motor speed (RPM)
			//case 10: break; // Enable/disable motor
			//case 12: break; // Enable/disable fan
			//case 13: break; // Enable/disable extra output
			//case 14: break; // Set servo 1 position
			//case 23: break; // pause/resume: Halt execution temporarily
			//case 24: break; // Abort immediately: Terminate all operations and reset
			//case 31: break; // Set build platform target temperature
		}
		return 0;
	}
	return rv;
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

//string pack_int16(uint16_t i) {
//	i = htonl(i);
//	string result = " ";
//	result[0] = i & 0xff;
//	result[1] = i >> 8;
//	return result;
//}

void MakerbotDriver::sendCommands(vector<string> commands) {
	for (int i=0; i<commands.size(); i++) {
		queue_.push_back(commands.at(i));
	}
	totalCmds_ += commands.size();
}

//void setFan(bool state) {
//	uint8_t payload[] = { 136, 0, 12, 1, state };
//	sendPacket(payload,sizeof(payload));
//}
//void setHeadTemperature(int temp) {
//	string str_temp_int16 = pack_int16(temp);
//	cout << "set temp: " << (int)str_temp_int16[0] << " " << (int)str_temp_int16[1] << endl;
//	uint8_t payload[] = { 136, 0, 3, 2, str_temp_int16[0], str_temp_int16[1] };
//	sendPacket(payload,sizeof(payload));
//}

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

int MakerbotDriver::getBufferSpace() {
	////02 - Get available buffer size: Determine how much free memory is available for buffering commands
	uint8_t payload[] = { 2 };
	sendPacket(payload,sizeof(payload));
	return bufferSpace_; //updated by sendPacket->parseResponse
}

//void playSong(uint8_t song=0) { ////151 - Queue Song
//	uint8_t payload[] = {151, song};
//	sendPacket(payload,sizeof(payload));
//}
//uint32_t unpack_int32(unsigned char *c) {
//	uint32_t result = c[0];
//	result += c[1] << 8;
//	result += c[2] << 16;
//	result += c[3] << 24;
//	return result;
//}
//void setText(string txt) { //149 - Display message to LCD
//	uint8_t payload[txt.size() + 6];
//	uint8_t header[] = { 149, 3, 1, 1, 1 };
//	memcpy(payload, header, sizeof(header));
//	memcpy(payload+sizeof(header), txt.c_str(), txt.size());
//	payload[sizeof(payload)-1] = '\0';
//	sendPacket(payload,sizeof(payload));
//}
//void abort() {
//	//07 - Abort immediately: Stop machine, shut down job permanently
//	uint8_t payload[] = { 7 };
//	sendPacket(payload,sizeof(payload));
//	queue.clear();
//}
//void clearDisplay() {
//	//149 - Display message to LCD
//	uint8_t payload[] = { 149, 3, 0, 0, 0, '\0'};
//	sendPacket(payload,sizeof(payload));
//}
//void clearBuffer() {
//	//03 - Clear buffer: Empty the command buffer
//	uint8_t payload[] = { 3 };
//	sendPacket(payload,sizeof(payload));
//}
