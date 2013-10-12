#include <stdlib.h>
#include <string.h>
#include "MarlinDriver.h"

using std::string;

//NOTE: see Server.cpp for comments on this macro
#define LOG(lvl, fmt, ...) log_.log(lvl, "[MLD] " fmt, ##__VA_ARGS__)

const int MarlinDriver::UPDATE_INTERVAL = 200;

MarlinDriver::MarlinDriver(Server& server, const std::string& serialPortPath, const uint32_t& baudrate)
: AbstractDriver(server, serialPortPath, baudrate),
  checkTemperatureInterval_(2000),
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

	/*//filterGCode parsing tests
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
	setGCode("M90\n;comment bla bla\r;comment2 bla bla\rM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000\n;comment bla bla");*/
}

int MarlinDriver::update() {
	if (!isConnected()) return -1;

	if(checkTemperatureInterval_ != -1 && temperatureTimer_.getElapsedTimeInMilliSec() > checkTemperatureInterval_) {
		LOG(Logger::BULK, "update temp()");
		temperatureTimer_.start(); // restart timer

		// We try receiving the temperature, until it's tried enough, and disable the mechanism after we have received temperature once
		if(checkTemperatureAttempt_ == -1 ||
			(checkTemperatureAttempt_ != -1 && checkTemperatureAttempt_ < maxCheckTemperatureAttempts_)) {
			LOG(Logger::BULK, "  check temp %i/%i",checkTemperatureAttempt_,maxCheckTemperatureAttempts_);
			checkTemperature();
			checkTemperatureAttempt_++;
		} else {
			switchBaudrate();
			checkTemperatureAttempt_ = 0;
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
		timer_.start(); // restart timer
	}

	//request to be called again after the time that's left of the update interval
	return UPDATE_INTERVAL - timer_.getElapsedTimeInMilliSec();
}

void MarlinDriver::setGCode(const std::string& gcode) {
	AbstractDriver::setGCode(gcode);
	extractGCodeInfo(gcode);
}

void MarlinDriver::appendGCode(const std::string& gcode) {
	AbstractDriver::appendGCode(gcode);
	extractGCodeInfo(gcode);
}

void MarlinDriver::readResponseCode(std::string& code) {
	LOG(Logger::BULK, "readCode(): '%s'",code.c_str());

	bool tempMessage = code.find("ok T:")==0;
	bool heatingMessage = code.find("T:")==0;
	if(tempMessage || heatingMessage) { // temperature or heating

		parseTemperatures(code);
		checkTemperatureAttempt_ = -1; //set to -1 to disable baud rate switching mechanism
		//maxCheckTemperatureAttempts_ = 1;

		// determine checkTempInterval
		if(heatingMessage) checkTemperatureInterval_ = -1; // if it's heating we don't really need to ask
		else if(state_ == PRINTING || state_ == STOPPING) checkTemperatureInterval_ = 5000; // if it's printing we ask it less frequently
		else checkTemperatureInterval_ = 1500; // normal

	} else if(code.find("ok") == 0) { // confirmation that code is received okay

		//sendCode("M105"); // temp
		if(state_ == PRINTING || state_ == STOPPING) {
			gcodeBuffer_.eraseLine();
			printNextLine();
		}

	} else if(code.find("start") != string::npos) {

		//sendCode("M105"); // temp
		//startPrint("M90\nM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000"); // temp

	} else if(code.find("Resend:") != string::npos) { // please resend line

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

	LOG(Logger::BULK, "parseTemperatures(): '%s'",code.c_str());
	// temperature hotend
	std::size_t posT = code.find("T:");
	temperature_ = findValue(code,posT+2);
	//LOG(Logger::BULK, "  temp '%i'",temperature_);

	// target temperature hotend
	std::size_t posTT = code.find('/',posT);
	if(posTT != std::string::npos) {
		targetTemperature_ = findValue(code,posTT+1);
		LOG(Logger::VERBOSE, "  (parseTemperatures)targetTemp '%i'",targetTemperature_);//TEMP was commented
	}

	// bed temperature
	std::size_t posB = code.find("B:");
	if(posB != std::string::npos) {
		bedTemperature_ = findValue(code,posB+2);
		//LOG(Logger::BULK, "  bedTemp '%i'",bedTemperature_);

		// target bed temperature
		std::size_t posTBT = code.find('/',posB);
		if(posTBT != std::string::npos) {
			targetBedTemperature_ = findValue(code,posTBT+1);
			//LOG(Logger::BULK, "  targetBedTemp '%i'",targetBedTemperature_);
		}
	}
}

int MarlinDriver::findValue(const string& code, size_t startPos) {
	//LOG(Logger::BULK, "  findValue()");
	std::size_t posEnd = code.find('\n',startPos);
	//LOG(Logger::BULK, "    posEnd: %i",posEnd);
	if(posEnd == string::npos) {
		posEnd = code.find(' ',startPos);
		//LOG(Logger::BULK, "    posEnd>: %i",posEnd);
	}
	string valueStr = code.substr(startPos, posEnd-startPos);
	//LOG(Logger::BULK, "    valueStr: %s",valueStr.c_str());
	return ::atof(valueStr.c_str());
}

void MarlinDriver::checkTemperature() {
	sendCode("M105");
}

void MarlinDriver::sendCode(const std::string& code) {
	//LOG(Logger::BULK, "sendCode(): %s",code.c_str());
	if (isConnected()) {
		extractGCodeInfo(code);
		serial_.send((code+"\n").c_str());
	}
}



//STATIC
const AbstractDriver::DriverInfo& MarlinDriver::getDriverInfo() {
	static AbstractDriver::vec_FirmwareDescription supportedFirmware;
	static AbstractDriver::DriverInfo info;

	if (supportedFirmware.empty()) {
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("marlin_generic") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("marlin_ultimaker") );

		info.supportedFirmware = supportedFirmware;
		info.creator = &MarlinDriver::create;
	};

	return info;
}

AbstractDriver* MarlinDriver::create(Server& server, const std::string& serialPortPath, const uint32_t& baudrate) {
	return new MarlinDriver(server, serialPortPath, baudrate);
}


/*********************
 * PRIVATE FUNCTIONS *
 *********************/

void MarlinDriver::extractGCodeInfo(const string& gcode) {
	LOG(Logger::BULK, "extractGCodeInfo()");
	//LOG(Logger::BULK, "  gcode: %s", gcode.c_str());

	// check for a heat command (M109 S... / M109 R...)
	std::size_t posHeat = gcode.find("M109");
	if(posHeat != std::string::npos) {
		targetTemperature_ = findValue(gcode, posHeat + 6);
		LOG(Logger::VERBOSE, "  (extractGCodeInfo)targetTemperature_: %i",targetTemperature_);//TEMP was bulk
	}

	// check for a bed heat command (M190 S... / M190 R...)
	std::size_t posBedHeat = gcode.find("M190");
	if(posHeat != std::string::npos) {
		targetBedTemperature_ = findValue(gcode, posBedHeat + 6);
		LOG(Logger::BULK, "  targetBedTemperature_: %i", targetBedTemperature_);
	}
}
