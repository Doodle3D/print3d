#include <stdlib.h>
#include <string.h>
#include "MarlinDriver.h"

using std::string;

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
	log_.log(Logger::BULK, "A");
	setGCode("M90\nM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");
	log_.log(Logger::BULK, "B");
	setGCode(";comment bla bla\nM90\nM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");
	log_.log(Logger::BULK, "C");
	setGCode("M90\n;comment bla bla\nM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");
	log_.log(Logger::BULK, "D");
	setGCode("M90\n;comment bla bla\n;comment2 bla bla\nM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");
	log_.log(Logger::BULK, "E");
	setGCode("M90\n;comment bla bla\r;comment2 bla bla\rM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000");
	log_.log(Logger::BULK, "F");
	setGCode("M90\n;comment bla bla\r;comment2 bla bla\rM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000\n;comment bla bla");*/
}

int MarlinDriver::update() {
	if(checkTemperatureInterval_ != -1 && tempTimer_.getElapsedTimeInMilliSec() > checkTemperatureInterval_) {
		log_.log(Logger::BULK, "MarlinDriver::update temp");
		tempTimer_.start(); // restart timer

		// We try receiving the temperature, until it's tried enough
		if(checkTemperatureAttempt_ < maxCheckTemperatureAttempts_) {
			log_.log(Logger::BULK, "  check temp %i/%i",checkTemperatureAttempt_,maxCheckTemperatureAttempts_);
			checkTemperature();
			checkTemperatureAttempt_++;
		} else {
			switchBaudrate();
			checkTemperatureAttempt_ = 0;
		}
	}
  if(state_ == PRINTING || timer_.getElapsedTimeInMilliSec() > 200) {
    //log_.log(Logger::BULK, "MarlinDriver::update");
    int rv = readData();
			//log_.log(Logger::BULK, "  rv: %i: '%s'",rv,serial_.getBuffer());//TEMP
    if (rv > 0) {
      string* line;
      while((line = serial_.extractLine()) != NULL) {
					readResponseCode(*line);
        delete line;
      }
    }
    timer_.start(); // restart timer
  }

  return 0;
}

void MarlinDriver::readResponseCode(std::string& code) {
	log_.log(Logger::BULK, "MarlinDriver::readCode: '%s'",code.c_str());

	bool tempMessage = code.find("ok T:")==0;
	bool heatingMessage = code.find("T:")==0;
	if(tempMessage || heatingMessage) { // temperature or heating

		parseTemperatures(code);
		checkTemperatureAttempt_ = 0;
		maxCheckTemperatureAttempts_ = 1;

		// determine checkTempInterval
		if(heatingMessage) checkTemperatureInterval_ = -1; // if it's heating we don't really need to ask
		else if(state_ == PRINTING) checkTemperatureInterval_ = 5000; // if it's printing we ask it less frequently
		else checkTemperatureInterval_ = 1500; // normal

	} else if(code.find("ok") == 0) { // confirmation that code is received okay

		//sendCode("M105"); // temp
		if(state_ == PRINTING) {
			erasePrevLine();
			printNextLine();
		}

	} else if(code.find("start") != string::npos) {

  	//sendCode("M105"); // temp
  	//startPrint("M90\nM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000"); // temp

  } else if(code.find("Resend:") != string::npos) { // please resend line

    //currentLine_ = atoi(strstr(code.c_str(), "Resend:") + 7); // correct current line
  	currentLine_--;
  	log_.log(Logger::BULK, "  Resend: %i",currentLine_);
    printNextLine();

  }
}

void MarlinDriver::parseTemperatures(string& code) {
  // Examples:
	//   ok T:19.1 /0.0 B:0.0 /0.0 @:0 B@:0
	//   T:19.51 B:-1.00 @:0
  //   T:19.5 E:0 W:?

	log_.log(Logger::BULK, "MarlinDriver::parseTemperatures: '%s'",code.c_str());
	// temperature hotend
  std::size_t posT = code.find("T:");
  temperature_ = findValue(code,posT+2);
  log_.log(Logger::BULK, "  temp '%i'",temperature_);

  // target temperature hotend
  std::size_t posTT = code.find("/",posT);
  if(posTT != std::string::npos) {
  	targetTemperature_ = findValue(code,posTT+1);
  	log_.log(Logger::BULK, "  targetTemp '%i'",targetTemperature_);
  }

  // bed temperature
  std::size_t posB = code.find("B:");
  if(posB != std::string::npos) {
  	bedTemperature_ = findValue(code,posB+2);
	  log_.log(Logger::BULK, "  bedTemp '%i'",bedTemperature_);

	  // target bed temperature
	  std::size_t posTBT = code.find("/",posB);
	  if(posTBT != std::string::npos) {
	  	targetBedTemperature_ = findValue(code,posTBT+1);
	  	log_.log(Logger::BULK, "  targetBedTemp '%i'",targetBedTemperature_);
	  }
  }
}

void MarlinDriver::updateGCodeInfo() {
	log_.log(Logger::BULK, "MarlinDriver::updateGCodeInfo");
	AbstractDriver::updateGCodeInfo();
	//log_.log(Logger::BULK, "  gcodeBuffer: %s",gcodeBuffer.c_str());

	// check for a heat command (M109 S... / M109 R...)
	std::size_t posHeat = gcodeBuffer_.find("M109");
	if(posHeat != std::string::npos) {
		targetTemperature_ = findValue(gcodeBuffer_,posHeat+6);
		log_.log(Logger::BULK, "  targetTemperature_: %i",targetTemperature_);
	}

	// check for a bed heat command (M190 S... / M190 R...)
	std::size_t posBedHeat = gcodeBuffer_.find("M190");
	if(posHeat != std::string::npos) {
		targetBedTemperature_ = findValue(gcodeBuffer_,posBedHeat+6);
		log_.log(Logger::BULK, "  targetBedTemperature_: %i",targetBedTemperature_);
	}
}

int MarlinDriver::findValue(string& code,size_t startPos) {
	//log_.log(Logger::BULK, "  MarlinDriver::findValue");
	std::size_t posEnd = code.find('\n',startPos);
	//log_.log(Logger::BULK, "    posEnd: %i",posEnd);
	if(posEnd == string::npos) {
		posEnd = code.find(' ',startPos);
		//log_.log(Logger::BULK, "    posEnd>: %i",posEnd);
	}
	string valueStr = code.substr(startPos, posEnd-startPos);
	//log_.log(Logger::BULK, "    valueStr: %s",valueStr.c_str());
	return ::atof(valueStr.c_str());
}

void MarlinDriver::checkTemperature() {
	sendCode("M105");
}

void MarlinDriver::sendCode(const std::string& code) {
	//log_.log(Logger::BULK, "MarlinDriver::sendCode: %s",code.c_str());
  serial_.send((code+"\n").c_str());
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
