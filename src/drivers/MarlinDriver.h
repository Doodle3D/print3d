#ifndef MARLIN_DRIVER_H_SEEN
#define MARLIN_DRIVER_H_SEEN

#include "AbstractDriver.h"
#include <string>
#include "../server/Logger.h"
#include "../Timer.h"

class MarlinDriver : public AbstractDriver {
public:
	explicit MarlinDriver(Server& server, const std::string& serialPortPath, const uint32_t& baudrate);

  static const AbstractDriver::DriverInfo& getDriverInfo();
	virtual int update();

  static AbstractDriver* create(Server& server, const std::string& serialPortPath, const uint32_t& baudrate);

protected:
  void readResponseCode(std::string& code);
  void parseTemperatures(std::string& code);
  void checkTemperature();
  void sendCode(const std::string& code);
  void updateGCodeInfo();
  int findValue(std::string& code,std::size_t startPos);

private:
  Timer timer_;
	Timer tempTimer_;
	int checkTemperatureInterval_;
  int checkTemperatureAttempt_;
  int maxCheckTemperatureAttempts_;
};

#endif /* ! MARLIN_DRIVER_H_SEEN */
