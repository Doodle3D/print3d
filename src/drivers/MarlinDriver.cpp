#include "MarlinDriver.h"

using std::string;

MarlinDriver::MarlinDriver(const std::string& serialPortPath, const uint32_t& baudrate)
: AbstractDriver(serialPortPath, baudrate) { }

int MarlinDriver::update() {

	if(tempTimer_.getElapsedTimeInMilliSec() > 5000) { // TODO: find good place for these constants
		tempTimer_.start(); // restart timer
		checkTemperature();
	}

  if(timer_.getElapsedTimeInMilliSec() > 1000) {
    //Logger& log = Logger::getInstance();
    //log.log(Logger::BULK, "MarlinDriver::update");
    int rv = readData();
    //log.log(Logger::BULK, "  rv: %i",rv);
    if (rv > 0) {
      string* line;
      while((line = serial_.extractLine()) != NULL) {
        readCode(*line);
        delete line;
      }
    }
    timer_.start(); // restart timer
  }

  return 0;
}

void MarlinDriver::readCode(std::string& code) {
	log_.log(Logger::BULK, "MarlinDriver::readCode: '%s' (length: %i)",code.c_str(), code.length());

  if(code.find("ok T:")==0) { // while heating
	  parseTemperatures(code);
  } else if(code.find("ok")==0) { // confirmation code is received okay
    //sendCode("M105"); // temp
  	if(state_ == PRINTING) printNextLine();
  } else if(code.find("T:")==0) { // temperature
    parseTemperatures(code);
  } else if(code.find("start") != string::npos) {
    //sendCode("M105"); // temp
  	startPrint("M90\nM91\nM92\nG0 X10.600 Y10.050 Z0.200 F2100.000 E0.000"); // temp
  } else if(code.find("Resend:") != string::npos) { // please resent line
    currentLine_ = atoi(strstr(code.c_str(), "Resend:") + 7); // correct current line
    log_.log(Logger::BULK, "  Resend: %i",currentLine_);
  }
}
void MarlinDriver::parseTemperatures(std::string& code) {
  // Examples:
  //   T:19.51 B:-1.00 @:0
  //   ok T:19.1 /0.0 B:0.0 /0.0 @:0 B@:0
  //   T:19.5 E:0 W:?

	log_.log(Logger::BULK, "MarlinDriver::parseTemperatures: '%s'",code.c_str());
  std::size_t posT = code.find("T:");
  std::size_t posSpace1 = code.find(" ");
  string tempStr = code.substr(posT+2, posSpace1-posT-2);
  temperature_ = ::atof(tempStr.c_str());
  log_.log(Logger::BULK, "  temp '%i'",temperature_);

  std::size_t posB = code.find("B:");
  if(posB != std::string::npos) {
	  std::size_t posSpace2 = code.find(" ",posB);
	  string bedTempStr = code.substr(posB+2, posSpace2-posB-2);
	  bedTemperature_ = ::atof(bedTempStr.c_str());
	  log_.log(Logger::BULK, "  bedTemp '%i'",bedTemperature_);
  }
}

void MarlinDriver::sendCode(const std::string& code) {
	// TODO: parse code to find target temperature ? check if M105 properly returns target temp.

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
AbstractDriver* MarlinDriver::create(const std::string& serialPortPath, const uint32_t& baudrate) {
  return new MarlinDriver(serialPortPath, baudrate);
}


