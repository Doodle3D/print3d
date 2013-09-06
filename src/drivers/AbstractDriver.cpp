#include "AbstractDriver.h"

using std::string;

AbstractDriver::AbstractDriver(const string& serialPortPath)
: temperature_(-1), serialPortPath_(serialPortPath)
{ /* empty */ }

AbstractDriver::~AbstractDriver() {
	serial_.close();
}

int AbstractDriver::openConnection() {

  Logger& log = Logger::getInstance();
	log.log(Logger::VERBOSE,"AbstractDriver::openConnection");
  log.log(Logger::VERBOSE,"  serialPortPath_: %s",serialPortPath_.c_str());
	int rv = serial_.open(serialPortPath_.c_str());
	log.log(Logger::VERBOSE,"  serial opened");
	if (rv > 0) {
		// TODO: get default baudrate from constructor
    // TODO: when this baudrate is incorrect another should be tried
    //  and it should be reported so that the one that does works gets
    //  saved as preference.
    serial_.setSpeed(115200);
	}

	return rv;
}

int AbstractDriver::readData() {
  Logger& log = Logger::getInstance();
  //log.log(Logger::BULK, "AbstractDriver::readData");

  int rv = serial_.readData();
  log.checkError(rv, "cannot read from device");
  if (rv == 0) {
    log.log(Logger::INFO, "connection closed with device");
  } else {
    log.log(Logger::BULK, "read %i bytes from device", rv);
  }
  return rv;
}

int AbstractDriver::closeConnection() {
	return serial_.close();
}

int AbstractDriver::getTemperature() const {
	return temperature_;
}
