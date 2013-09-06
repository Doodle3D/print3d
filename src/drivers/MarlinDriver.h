#ifndef MARLIN_DRIVER_H_SEEN
#define MARLIN_DRIVER_H_SEEN

#include "AbstractDriver.h"
#include <string>
#include "../server/Logger.h"

class MarlinDriver : public AbstractDriver {
public:
	explicit MarlinDriver(const std::string& serialPortPath);

  static const AbstractDriver::DriverInfo& getDriverInfo();
	virtual void update();

  static AbstractDriver* create(const std::string& serialPortPath);

protected:
  void readCode(std::string& code);

private:
  int updateDelay;
};

#endif /* ! MARLIN_DRIVER_H_SEEN */
