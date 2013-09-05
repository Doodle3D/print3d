#ifndef MARLIN_DRIVER_H_SEEN
#define MARLIN_DRIVER_H_SEEN

#include "AbstractDriver.h"

class MarlinDriver : public AbstractDriver {
public:
	explicit MarlinDriver(const std::string& serialPortPath);

  static const AbstractDriver::DriverInfo& getDriverInfo();
	virtual int update();

  static AbstractDriver* create(const std::string& serialPortPath);

private:

};

#endif /* ! MARLIN_DRIVER_H_SEEN */
