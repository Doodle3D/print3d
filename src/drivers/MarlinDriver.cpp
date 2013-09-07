#include "MarlinDriver.h"

using std::string;

MarlinDriver::MarlinDriver(const std::string& serialPortPath)
: AbstractDriver(serialPortPath)
{
  //updateDelay = 0;
  //timer_.start();
}

int MarlinDriver::update() {

  if(timer_.getElapsedTimeInMilliSec() > 1000) {
    Logger& log = Logger::getInstance();
    log.log(Logger::BULK, "MarlinDriver::update");
    int rv = readData();
    log.log(Logger::BULK, "  rv: %i",rv);
    if (rv > 0) {
      string* line;
      while((line = serial_.extractLine()) != NULL) {
        readCode(*line);
        delete line;
      }
    }

    /*std::string serialMessage;
    while((serialMessage = readData()) != "") {
      log.log(Logger::VERBOSE, "  serialMessage: %s",serialMessage.c_str());
    }*/

    timer_.start(); // restart timer
	}

  return 0;
}

void MarlinDriver::readCode(string& code) {
  Logger& log = Logger::getInstance();
  log.log(Logger::BULK, "MarlinDriver::readCode: '%s' (length: %i)",code.c_str(), code.length());

  if(code == "start") {
    sendCode("M105");
  }
}
void MarlinDriver::sendCode(const std::string& code) {
  serial_.send(code.c_str());
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
AbstractDriver* MarlinDriver::create(const std::string& serialPortPath) {
  return new MarlinDriver(serialPortPath);
}
