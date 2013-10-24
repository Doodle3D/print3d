#ifndef MAKERBOT_DRIVER_H_SEEN
#define MAKERBOT_DRIVER_H_SEEN

#include <deque>
#include <string>
#include <vector>
#include "../Timer.h"
#include "AbstractDriver.h"
#include "../server/Logger.h"

class MakerbotDriver : public AbstractDriver {
public:
	MakerbotDriver(Server& server, const std::string& serialPortPath, const uint32_t& baudrate);

	static const AbstractDriver::DriverInfo& getDriverInfo();
	virtual int update();

	//overrides
	void setGCode(const std::string& gcode);
	void appendGCode(const std::string& gcode);

	static AbstractDriver* create(Server& server, const std::string& serialPortPath, const uint32_t& baudrate);

protected:
	void sendCode(const std::string& code);
	void readResponseCode(std::string& code);

private:
	Timer timer_;

	uint32_t bufferSpace_;
	std::deque<std::string> queue_;

	uint16_t headTemperature_;

	void processQueue();
	void sendPacket(uint8_t *payload, int len);
	uint8_t _crc_ibutton_update(uint8_t crc, uint8_t data);
	bool parseResponse(int cmd);
	std::string getResponseMessage(int code);
	void sendCommands(std::vector<std::string> commands);
	int getHeadTemperature();
	int getFirmwareVersion();
	int getBufferSpace();
};

#endif /* ! MAKERBOT_DRIVER_H_SEEN */
