#ifndef MARLIN_DRIVER_H_SEEN
#define MARLIN_DRIVER_H_SEEN

#include "AbstractDriver.h"
#include <string>
#include "../server/Logger.h"
#include "../Timer.h"

class MarlinDriver : public AbstractDriver {
public:
	explicit MarlinDriver(const std::string& serialPortPath);

  static const AbstractDriver::DriverInfo& getDriverInfo();
	virtual int update();

  static AbstractDriver* create(const std::string& serialPortPath);

protected:
  void readCode(std::string& code);
  void sendCode(const std::string& code);

private:
  //int updateDelay;
  Timer timer_;

};

#endif /* ! MARLIN_DRIVER_H_SEEN */
