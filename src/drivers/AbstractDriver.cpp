#include "AbstractDriver.h"

using std::string;

AbstractDriver::AbstractDriver(const string& serialPortPath)
: serialPortPath_(serialPortPath), temperature_(-1)
{ /* empty */ }

AbstractDriver::~AbstractDriver() {
	serial_.close();
}

int AbstractDriver::openConnection() {
	int rv = serial_.open(serialPortPath_.c_str());

	if (rv > 0) {
		//TODO: set (detected?) speed
	}

	return rv;
}

int AbstractDriver::closeConnection() {
	return serial_.close();
}

int AbstractDriver::getTemperature() const {
	return temperature_;
}
