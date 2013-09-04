#ifndef ABSTRACT_DRIVER_H_SEEN
#define ABSTRACT_DRIVER_H_SEEN

#include <string>
#include <vector>
#include "Serial.h"

class AbstractDriver {
public:
	struct PrinterDescription {
		std::string name;
	};

	typedef std::vector<PrinterDescription> vec_PrinterDescription;

	explicit AbstractDriver(const std::string& serialPortPath);
	virtual ~AbstractDriver();

	int openConnection();
	int closeConnection();

	int getTemperature() const;

	virtual const vec_PrinterDescription& getSupportedPrinters() const = 0;
	virtual int update() = 0;

private:
	const std::string serialPortPath_;
	Serial serial_;

	int temperature_;
};

#endif /* ! ABSTRACT_DRIVER_H_SEEN */
