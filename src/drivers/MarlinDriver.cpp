/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include <stdlib.h>
#include <string.h>
#include "MarlinDriver.h"

using std::string;
using std::size_t;

//NOTE: see Server.cpp for comments on this macro
#define LOG(lvl, fmt, ...) log_.log(lvl, "[MLD] " fmt, ##__VA_ARGS__)

const int MarlinDriver::UPDATE_INTERVAL = 200;

MarlinDriver::MarlinDriver(Server& server, const string& serialPortPath, const uint32_t& baudrate)
: AbstractDriver(server, serialPortPath, baudrate),
  checkTemperatureInterval_(2000),
  checkConnection_(true),
  checkTemperatureAttempt_(0),
  maxCheckTemperatureAttempts_(2) {

	/*//temperature parsing tests
	readCode(*new string("T:19.1 /0.0 B:0.0 /0.0 @:0 B@:0"));
	readCode(*new string("ok T:19.1 /0.0 B:0.0 /0.0 @:0 B@:0"));
	readCode(*new string("ok T:19.5 /0.0 B:0.0 /0.0 @:0 B@:0"));
	readCode(*new string("ok T:19.6 /0.0 B:0.0 /0.0 @:0 B@:0"));
	readCode(*new string("ok T:19.9 /0.0 B:0.0 /0.0 @:0 B@:0"));
	readCode(*new string("ok T:19 /0.0 B:0.0 /0.0 @:0 B@:0"));
	readCode(*new string("ok T:19.9 /180.5 B:0.0 /0.0 @:0 B@:0"));
	readCode(*new string("ok T:19.9 /180.5 B:90.0 /0.0 @:0 B@:0"));
	readCode(*new string("ok T:19.9 /180.5 B:90.6 /0.0 @:0 B@:0"));
	readCode(*new string("ok T:19.9 /180.5 B:90 /0.0 @:0 B@:0"));
	readCode(*new string("ok T:19.9 /180.5 B:90.0 /180.0 @:0 B@:0"));
	readCode(*new string("ok T:19.9 /180.5 B:90.0 /180.9 @:0 B@:0"));
	readCode(*new string("ok T:19.1 @:0"));
	readCode(*new string("T:19.1 @:0"));
	readCode(*new string("ok T:19.1 /0.0 @:0 B:29.0 /0.0 "));
	readCode(*new string("ok T:19.1 @:0 B:29.0 "));*/

	/*// updateGCodeInfo parsing tests
	startPrint("M109 S220");
	startPrint("M109 S180");
	startPrint("M109 S70");
	startPrint("M109 S60\nM90\nM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");
	startPrint("M90\nM91\nM109 S60\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");
	startPrint("M90\nM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000\nM109 S60");
	startPrint("M90\nM109 S60\nM91\nM92\nM109 S60\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");*/

	/* //filterGCode parsing tests
	LOG(Logger::BULK, "A");
	setGCode("M90\nM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");
	LOG(Logger::BULK, "B");
	setGCode(";comment bla bla\nM90\nM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");
	LOG(Logger::BULK, "C");
	setGCode("M90\n;comment bla bla\nM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");
	LOG(Logger::BULK, "D");
	setGCode("M90\n;comment bla bla\n;comment2 bla bla\nM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");
	LOG(Logger::BULK, "E");
	setGCode("M90\n;comment bla bla\r;comment2 bla bla\rM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");
	LOG(Logger::BULK, "F");
	setGCode("M90\n;comment bla bla\r;comment2 bla bla\rM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000\n;comment bla bla");
	*/
}

int MarlinDriver::update() {
	if (!isConnected()) return -1;

	if (checkTemperatureInterval_ != -1 && temperatureTimer_.getElapsedTimeInMilliSec() > checkTemperatureInterval_) {
		LOG(Logger::BULK, "update temp()");
		temperatureTimer_.start(); // restart timer

		// We check the temperature
		// during startup we use this to check for a valid connection, when it's established we stop checking
		if (checkConnection_) {
			if (checkTemperatureAttempt_ < maxCheckTemperatureAttempts_) {
				LOG(Logger::BULK, "  check temp %i/%i",checkTemperatureAttempt_,maxCheckTemperatureAttempts_);
				checkTemperature();
				checkTemperatureAttempt_++;
			} else {
				switchBaudrate();
				checkTemperatureAttempt_ = 0;
			}
		} else {
			LOG(Logger::BULK, "  check temp");
			checkTemperature();
		}
	}

	if (state_ == PRINTING || state_ == STOPPING || timer_.getElapsedTimeInMilliSec() > UPDATE_INTERVAL) {
		//LOG(Logger::BULK, "update()");
		int rv = readData();
		if (rv > 0) {
			string* line;
			while((line = serial_.extractLine()) != NULL) {
				readResponseCode(*line);
				delete line;
			}
		}
		timer_.start(); //restart timer
	}

	//request to be called again after the time that's left of the update interval
	return UPDATE_INTERVAL - timer_.getElapsedTimeInMilliSec();
}

bool MarlinDriver::startPrint(STATE state) {
	if (!AbstractDriver::startPrint(state)) return false;
	printNextLine();
	return true;
}

void MarlinDriver::setGCode(const string& gcode) {
	AbstractDriver::setGCode(gcode);
	extractGCodeInfo(gcode);
}

void MarlinDriver::appendGCode(const string& gcode) {
	AbstractDriver::appendGCode(gcode);
	extractGCodeInfo(gcode);
}

void MarlinDriver::readResponseCode(string& code) {
	/*
	 * Printing data from printer with wrong baudrate sometimes garbles the
	 * terminal, enable this to filter a number of control characters.
	 * See: https://bbs.archlinux.org/viewtopic.php?pid=423358#p423358.
	 */
	//filterText(code, "\x0D\x0E\x1B"); //not sure if replacing these is enough

	LOG(Logger::BULK, "readCode(): '%s'", code.c_str());

	bool tempMessage = (code.find("ok T:") == 0);
	bool heatingMessage = (code.find("T:") == 0);

	if (tempMessage || heatingMessage) { // temperature or heating
		parseTemperatures(code);
		//checkTemperatureAttempt_ = -1; //set to -1 to disable baud rate switching mechanism
		if (checkConnection_) {
			checkConnection_ = false; // stop checking connection (and switching baud rate)
			setState(IDLE);
		}
		//maxCheckTemperatureAttempts_ = 1;

		// determine checkTempInterval
		//if (heatingMessage) checkTemperatureInterval_ = -1; // if it's heating we don't really need to ask
		if (state_ == PRINTING || state_ == STOPPING) checkTemperatureInterval_ = 5000; // if it's printing we ask it less frequently
		else checkTemperatureInterval_ = 1500; // normal

		LOG(Logger::BULK, "  checkTemperatureInterval_: '%i'", checkTemperatureInterval_);

	} else if (code.find("ok") == 0) { // confirmation that code is received okay
		//sendCode("M105"); // temp
		if (state_ == PRINTING || state_ == STOPPING) {
			gcodeBuffer_.eraseLine();
			printNextLine();
		}

	} else if (code.find("start") != string::npos) {
		//sendCode("M105"); // temp
		//startPrint("M90\nM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000"); // temp

	} else if (code.find("Resend:") != string::npos) { // please resend line
		//currentLine_ = atoi(strstr(code.c_str(), "Resend:") + 7); // correct current line
		gcodeBuffer_.setCurrentLine(getCurrentLine() - 1);
		LOG(Logger::BULK, "  Resend: %i", gcodeBuffer_.getCurrentLine());
		printNextLine();

	}
}

void MarlinDriver::parseTemperatures(string& code) {
	// Examples:
	//   ok T:19.1 /0.0 B:0.0 /0.0 @:0 B@:0
	//   T:19.51 B:-1.00 @:0
	//   T:19.5 E:0 W:?

	LOG(Logger::BULK, "parseTemperatures(): '%s'", code.c_str());
	// temperature hotend
	size_t posT = code.find("T:");

	//status variant _not_ prefixed with 'ok ' indicates the printer is heating
	heating_ = (posT == 0);

	temperature_ = findValue(code, posT + 2);
	//LOG(Logger::BULK, "  temp '%i'", temperature_);

	// target temperature hotend
	size_t posTT = code.find('/', posT);
	if (posTT != string::npos) {
		targetTemperature_ = findValue(code, posTT + 1);
		LOG(Logger::VERBOSE, "  (parseTemperatures) targetTemp '%i'", targetTemperature_); //TEMP was commented
	}

	// bed temperature
	size_t posB = code.find("B:");
	if (posB != string::npos) {
		bedTemperature_ = findValue(code, posB + 2);
		//LOG(Logger::BULK, "  bedTemp '%i'", bedTemperature_);

		// target bed temperature
		size_t posTBT = code.find('/', posB);
		if (posTBT != string::npos) {
			targetBedTemperature_ = findValue(code, posTBT + 1);
			//LOG(Logger::BULK, "  targetBedTemp '%i'", targetBedTemperature_);
		}
	}
}

int MarlinDriver::findValue(const string& code, size_t startPos) {
	//LOG(Logger::BULK, "  findValue()");
	size_t posEnd = code.find('\n', startPos);
	//LOG(Logger::BULK, "    posEnd: %i", posEnd);
	if (posEnd == string::npos) {
		posEnd = code.find(' ', startPos);
		//LOG(Logger::BULK, "    posEnd>: %i", posEnd);
	}
	string valueStr = code.substr(startPos, posEnd-startPos);
	//LOG(Logger::BULK, "    valueStr: %s", valueStr.c_str());
	return ::atof(valueStr.c_str());
}

void MarlinDriver::checkTemperature() {
	sendCode("M105");
}

void MarlinDriver::sendCode(const string& code) {
	LOG(Logger::BULK, "sendCode(): %s", code.c_str());
	if (isConnected()) {
		extractGCodeInfo(code);
		serial_.send((code + "\n").c_str());
	}
}



//STATIC
const AbstractDriver::DriverInfo& MarlinDriver::getDriverInfo() {
	static AbstractDriver::vec_FirmwareDescription supportedFirmware;
	static AbstractDriver::DriverInfo info;

	if (supportedFirmware.empty()) {
		info.name = "Marlin";

		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("rigidbot") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("ultimaker") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("ultimaker2") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("printrbot") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("bukobot") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("cartesio") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("cyrus") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("delta_rostockmax") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("deltamaker") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("eventorbot") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("felix") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("gigabot") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("kossel") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("leapfrog_creatr") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("lulzbot_aO_101") );
    	supportedFirmware.push_back( AbstractDriver::FirmwareDescription("lulzbot_taz_4") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("makergear_m2") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("makergear_prusa") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("makibox") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("orca_0_3") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("ord_bot_hadron") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("printxel_3d") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("prusa_i3") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("prusa_iteration_2") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("rapman") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("reprappro_huxley") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("reprappro_mendel") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("robo_3d_printer") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("shapercube") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("tantillus") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("minifactory") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("vision_3d_printer") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("builder3d") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("bigbuilder3d") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("mamba3d") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("marlin_generic") );
        supportedFirmware.push_back( AbstractDriver::FirmwareDescription("doodle_dream") );

		info.supportedFirmware = supportedFirmware;
		info.creator = &MarlinDriver::create;
	};

	return info;
}

AbstractDriver* MarlinDriver::create(Server& server, const string& serialPortPath, const uint32_t& baudrate) {
	return new MarlinDriver(server, serialPortPath, baudrate);
}


/*********************
 * PRIVATE FUNCTIONS *
 *********************/

void MarlinDriver::extractGCodeInfo(const string& gcode) {
	int rv;
	LOG(Logger::BULK, "extractGCodeInfo()");
	//LOG(Logger::BULK, "  gcode: %s", gcode.c_str());

	// check for an extruder heat command (M109 S... / M109 R... / M104 ...)
	const string extruderCmds[] = {"M109", "M104"};
	rv = extractTemperatureFromMCode(gcode, extruderCmds, 2);
	if (rv >= 0) {
		targetTemperature_ = rv;
		LOG(Logger::VERBOSE, "  (extractGCodeInfo) targetTemperature_: %i", targetTemperature_);
	}

	// check for a bed heat command (M190 S... / M190 R... / M140 ...)
	const string bedCmds[] = {"M190", "M140"};
	rv = extractTemperatureFromMCode(gcode, bedCmds, 2);
	if (rv >= 0) {
		targetBedTemperature_ = rv;
		LOG(Logger::VERBOSE, "  (extractGCodeInfo) targetBedTemperature_: %i", targetBedTemperature_);
	}
}

//int MarlinDriver::extractTemperatureFromMCode(const string& gcode, const vector<const string>& codes) {
int MarlinDriver::extractTemperatureFromMCode(const string& gcode, const string *codes, int num_codes) {
	size_t cmdPos = string::npos, offset = 0;
	int result = -1;

	for (int i = 0; i < num_codes; ++i) {
		cmdPos = gcode.find(codes[i]);
		if (cmdPos != string::npos) {
			offset = codes[i].size() + 1; //assume space after command
			break;
		}
	}

	if(cmdPos != string::npos) {
		if (gcode.at(cmdPos + offset) == 'T') offset += 3; //skip toolhead
		result = findValue(gcode, cmdPos + offset + 1); //skip S or R
	}

	return result;
}

void MarlinDriver::filterText(string& text, const string& replace) {
	for (string::iterator it = text.begin(); it < text.end(); ++it) {
		if (replace.find(*it) != string::npos) *it = '.';
	}
}
