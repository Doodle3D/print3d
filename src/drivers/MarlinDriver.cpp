#include "MarlinDriver.h"

using std::string;

MarlinDriver::MarlinDriver(const std::string& serialPortPath)
: AbstractDriver(serialPortPath)
{
  updateDelay = 0;
}

void MarlinDriver::update() {

  if(updateDelay <= 0) {

    Logger& log = Logger::getInstance();
    //log.log(Logger::BULK, "MarlinDriver::update");

    int rv = readData();
    if (rv > 0) {
      string* line = serial_.extractLine();
      readCode(*line);
      delete line;
    }

    /*std::string serialMessage;
    while((serialMessage = readData()) != "") {
      log.log(Logger::VERBOSE, "  serialMessage: %s",serialMessage.c_str());
    }*/
    updateDelay = 10000;
	}
  updateDelay--;
}

void MarlinDriver::readCode(string& code) {
  Logger& log = Logger::getInstance();
  log.log(Logger::BULK, "MarlinDriver::readCode: %s",code.c_str());
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
