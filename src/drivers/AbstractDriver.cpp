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
		// TODO: get default baudrate from constructor
    // TODO: when this baudrate is incorrect another should be tried
    //  and it should be reported so that the one that does works gets
    //  saved as preference.
    serial_.setSpeed(115200);
	}

	return rv;
}

int AbstractDriver::closeConnection() {
	return serial_.close();
}

void AbstractDriver::readSerial() {

}

int AbstractDriver::getTemperature() const {
	return temperature_;
}
