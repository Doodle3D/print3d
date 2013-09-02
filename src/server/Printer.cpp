#include "Printer.h"

using std::string;

Printer::Printer(const string& serialPortPath)
: serialPortPath_(serialPortPath), temperature_(-1)
{ /* empty */ }

Printer::~Printer() {
	serial_.close();
}

int Printer::getTemperature() const {
	return temperature_;
}

int Printer::openConnection() {
	int rv = serial_.open(serialPortPath_.c_str());

	if (rv > 0) {
		//TODO: set (detected?) speed
	}

	return rv;
}

int Printer::closeConnection() {
	return serial_.close();
}
