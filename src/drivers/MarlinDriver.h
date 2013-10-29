#ifndef MARLIN_DRIVER_H_SEEN
#define MARLIN_DRIVER_H_SEEN

#include <string>
#include "../Timer.h"
#include "AbstractDriver.h"
#include "../server/Logger.h"

class MarlinDriver : public AbstractDriver {
public:
	MarlinDriver(Server& server, const std::string& serialPortPath, const uint32_t& baudrate);

	static const AbstractDriver::DriverInfo& getDriverInfo();
	virtual int update();

	//overrides
	void setGCode(const std::string& gcode);
	void appendGCode(const std::string& gcode);

	static AbstractDriver* create(Server& server, const std::string& serialPortPath, const uint32_t& baudrate);

protected:
	void startPrint(STATE state);

	void readResponseCode(std::string& code);
	void parseTemperatures(std::string& code);
	void checkTemperature();
	void sendCode(const std::string& code);
	int findValue(const std::string& code, std::size_t startPos);

private:
	static const int UPDATE_INTERVAL;

	Timer timer_;
	Timer temperatureTimer_;
	int checkTemperatureInterval_;
	bool checkConnection_;
	int checkTemperatureAttempt_;
	int maxCheckTemperatureAttempts_;

	void extractGCodeInfo(const std::string& gcode);
};

#endif /* ! MARLIN_DRIVER_H_SEEN */
