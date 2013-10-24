/*
 * Protocol spec: https://github.com/makerbot/s3g/blob/master/doc/s3gProtocol.md
 */
#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <unistd.h>
#include "MakerbotDriver.h"

using std::cout;
using std::endl;
using std::string;
using std::vector;

//NOTE: see Server.cpp for comments on this macro
#define LOG(lvl, fmt, ...) log_.log(lvl, "[MBD] " fmt, ##__VA_ARGS__)

//const int MakerbotDriver::UPDATE_INTERVAL = 200;

MakerbotDriver::MakerbotDriver(Server& server, const std::string& serialPortPath, const uint32_t& baudrate)
: AbstractDriver(server, serialPortPath, baudrate), bufferSpace_(512), headTemperature_(0)
{}

int MakerbotDriver::update() {
	if (!isConnected()) return -1;

	processQueue();

	//HIER:
	//- eerste pakketjes krijgen geen reactie, wss omdat de arduino nog aan het booten is (dit kan vrij lang duren _en_ vari‘ren (5 ~ 30 sec) (mss langer voor toolhead temp?) <- duurt ook lang in RepG)
	//- af en toe (iig tijdens het start-timeout-moment) komt er een 0x60 terug en daarna '0x60 0x80 0xE0' en daarna steeds een byte daaraf (?)
	getHeadTemperature();
	LOG(Logger::VERBOSE, "  head temp: %i", headTemperature_);

	return 500;
}

void MakerbotDriver::setGCode(const std::string& gcode) {
//	AbstractDriver::setGCode(gcode);
//	extractGCodeInfo(gcode);
}

void MakerbotDriver::appendGCode(const std::string& gcode) {
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
	if (!queue_.empty() && getBufferSpace()>480) {
		for (int i=0; i<15; i++) {
			if (queue_.empty()) break;
			string command = queue_.front();
			queue_.pop_front();
			int len = command.size();
			uint8_t *payload = (uint8_t*)command.c_str();
			sendPacket(payload,len);
		}
	}
}

void MakerbotDriver::sendPacket(uint8_t *payload, int len) {
	serial_.write(0xD5);
	serial_.write(len);
	uint8_t crc = 0;
	for (int i = 0; i < len; i++) {
		crc = _crc_ibutton_update(crc, payload[i]);
		serial_.write(payload[i]);
	}
	serial_.write(crc);
	bufferSpace_ -= len; //approximation of space left in buffer
	//lastCommand = payload[0]; //is this safe?
	if (!parseResponse(payload[0])) {
		cout << "Timeout occured waiting on makerbot response" << endl;
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

bool MakerbotDriver::parseResponse(int cmd) {
	//CHECK: does readDataWithLen() work correcly?
	//TODO: add error checks to readDataWithLen() calls
	//TODO: in the list below, 00 (get version) host query command seems to be missing?
	//FIXME: ok readDataWithLen() might still read more than requested because the inner readAllAvailableData still reads MAX_BUFFER_SIZE... (in practice, packets from the t-o-m seem to be read at once)
	serial_.readDataWithLen(1, 500); //timeout after 100ms (officialy 40ms but makerbots sometimes need longer occording to docs
	if (serial_.getBufferSize() > 0) {
		unsigned char *buf = (unsigned char*)serial_.getBuffer();
		for (int i = 0; i < serial_.getBufferSize(); i++) {
			printf(" 0x%02X", buf[i]);
		}
		printf("\n");
	}
	if (serial_.extractByte()==0xD5) {
//		cout << "0xD5 found" << endl;
		//::usleep(5000); //prevent OF_SERIAL_NO_DATA
		serial_.readDataWithLen(1, 5);
		int len = serial_.extractByte();
		unsigned char buf[len+1];
		serial_.readDataWithLen(len+1, 5);
		serial_.extractBytes(buf, len+1);
		int crc = buf[len];
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
			case 10: headTemperature_ = *reinterpret_cast<unsigned*>(buf+1); break; //Tool query: Query a tool for information
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
		return true;
	}
	return false;
}

//bool MakerbotDriver::parseResponse(int cmd) {
//	long t0 = timer_.getElapsedTimeInMilliSec();
//	while (timer_.getElapsedTimeInMilliSec()-t0 < 100) { //timeout after 100ms (officialy 40ms but makerbots sometimes need longer occording to docs
//		if (serial_.readByte()==0xD5) {
//			//cout << "0xD5" << endl;
//			::usleep(5000); //prevent OF_SERIAL_NO_DATA
//			int len = serial_.readByte();
//			unsigned char buf[len];
//			serial_.readBytes(buf, len);
//			int crc = serial_.readByte();
//			int code = buf[0];
//			if (code!=0x81) cout << getResponseMessage(code) << endl;
//			//cout << dec << "cmd: " << cmd << " -> 0x" << hex << code << endl;
//			switch (cmd) {
//				////////////// Host Query Commands
//				case 1: break; //01 - init: Initialize firmware to boot state
//				case 2: bufferSpace_ = *reinterpret_cast<unsigned*>(buf+1); break; //Get available buffer size: Determine how much free memory is available for buffering commands
//				case 3: break; //03 - Clear buffer: Empty the command buffer
//				case 7: break; //07 - Abort immediately: Stop machine, shut down job permanently
//				case 8: break; //08 - pause/resume: Halt execution temporarily
//				case 10: headTemperature_ = *reinterpret_cast<unsigned*>(buf+1); break; //Tool query: Query a tool for information
//				case 11: break; //11 - Is finished: See if the machine is currently busy
//				case 12: break; //12 - Read from EEPROM
//				case 13: break; //13 - Write to EEPROM
//				case 14: break; //14 - Capture to file
//				case 15: break; //15 - End capture to file
//				case 16: break; //16 - Play back capture
//				case 17: break; //17 - reset
//				case 18: break; //18 - Get next filename
//				case 20: break; //20 - Get build name
//				case 21: break; //21 - Get extended position: Get the current
//				case 22: break; //22 - Extended stop: Stop a subset of systems
//				case 23: break; //23 - Get motherboard status
//				case 24: break; //24 - Get build statistics
//				case 25: break; //25 - Get communication statistics
//				case 27: break; //27 - Get advanced version number
//				///////////// Host Buffered Commands
//				case 131: break; //131 - Find axes minimums: Move specified axes in the negative direction until limit switch is triggered.
//				case 132: break; //132 - Find axes maximums: Move specified axes in the positive direction until limit switch is triggered.
//				case 133: break; //133 - delay: pause all motion for the specified time
//				case 134: break; //134 - Change Tool
//				case 135: break; //135 - Wait for tool ready: Wait until a tool is ready before proceeding
//				case 136: break; //136 - Tool action command: Send an action command to a tool for execution
//				case 137: break; //137 - Enable/disable axes: Explicitly enable or disable stepper motor controllers
//				case 139: break; //139 - Queue extended point
//				case 140: break; //140 - Set extended position
//				case 141: break; //141 - Wait for platform ready: Wait until a build platform is ready before proceeding
//				case 142: break; //142 - Queue extended point, new style
//				case 143: break; //143 - Store home positions
//				case 144: break; //144 - Recall home positions
//				case 145: break; //145 - Set digital potentiometer value
//				case 146: break; //146 - Set RGB LED value
//				case 147: break; //147 - Set Beep
//				case 148: break; //148 - Wait for button
//				case 149: break; //149 - Display message to LCD
//				case 150: break; //150 - Set Build Percentage
//				case 151: break; //151 - Queue Song
//				case 152: break; //152 - reset to Factory
//				case 153: break; //153 - Build start notification
//				case 154: break; //154 - Build end notification
//				case 155: break; //155 - Queue extended point x3g
//				case 157: break; //157 - Stream Version
//				//////////////////// Tool Query Commands
//				//case 00: break; // Get version: Query firmware for version information
//				//case 02: break; // Get toolhead temperature
//				//case 17: break; // Get motor speed (RPM)
//				//case 22: break; // Is tool ready?
//				//case 25: break; // Read from EEPROM
//				//case 26: break; // Write to EEPROM
//				//case 30: break; // Get build platform temperature
//				//case 32: break; // Get toolhead target temperature
//				//case 33: break; // Get build platform target temperature
//				//case 35: break; // Is build platform ready?
//				//case 36: break; // Get tool status
//				//case 37: break; // Get PID state
//				//
//				////////////////////// Tool Action Commands
//				//case 01: break; // init: Initialize firmware to boot state
//				//case 03: break; // Set toolhead target temperature
//				//case 06: break; // Set motor speed (RPM)
//				//case 10: break; // Enable/disable motor
//				//case 12: break; // Enable/disable fan
//				//case 13: break; // Enable/disable extra output
//				//case 14: break; // Set servo 1 position
//				//case 23: break; // pause/resume: Halt execution temporarily
//				//case 24: break; // Abort immediately: Terminate all operations and reset
//				//case 31: break; // Set build platform target temperature
//			}
//			return true;
//		}
//	}
//	return false;
//}

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
	out << "Unknown response code: " << code; //CHECK
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

int MakerbotDriver::getHeadTemperature() {
	uint8_t payload[] = { 10, 0, 2 };
	sendPacket(payload,sizeof(payload));
	return headTemperature_;
}

int MakerbotDriver::getFirmwareVersion() {
	//00 - Get version: Query firmware for version information
	uint8_t payload[] = { 0 };
	sendPacket(payload,sizeof(payload));
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
