#ifndef PRINTER_H_SEEN
#define PRINTER_H_SEEN

#include <string>
#include "Serial.h"

class Printer {
public:
	explicit Printer(const std::string& serialPortPath);
	~Printer();

	int openConnection();
	int closeConnection();

	int getTemperature() const;

private:
	const std::string serialPortPath_;
	Serial serial_;

	int temperature_;
};

#endif /* ! PRINTER_H_SEEN */
