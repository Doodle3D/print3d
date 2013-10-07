#ifndef ABSTRACT_DRIVER_H_SEEN
#define ABSTRACT_DRIVER_H_SEEN

#include <inttypes.h>
#include <string>
#include <vector>
#include "GCodeBuffer.h"
#include "Serial.h"

class Server;

class AbstractDriver {
public:

	// description of firmware a driver supports. TODO: add human readable names
	struct FirmwareDescription {
		std::string name;
		FirmwareDescription(const std::string& n)
		: name(n)
		{}
	};
	// typedef (shorthand) for list of firmware descriptions
	typedef std::vector<FirmwareDescription> vec_FirmwareDescription;
	// typedef (shorthand) for create instance function of driver
	typedef AbstractDriver* (*creatorFunc)(Server& server, const std::string& serialPortPath, const uint32_t& baudrate);
	// driver info per driver (used in DriverFactory)
	struct DriverInfo {
		vec_FirmwareDescription supportedFirmware;
		creatorFunc creator;                                //pointer to driver's create function
	};

	// drivers should provide a function which returns a reference to a DriverInfo object
	//   static const AbstractDriver::DriverInfo& getDriverInfo();

	typedef enum STATE{
		UNKNOWN = 0,
		DISCONNECTED = 1,
		IDLE,
		PRINTING, /* executing commands */
		STOPPING
	} STATE;

	explicit AbstractDriver(Server& server, const std::string& serialPortPath, const uint32_t& baudrate);
	virtual ~AbstractDriver();

	//apparently gcc is quite good at avoiding unncessary copies (see NRVO)
	const std::vector<std::string> findDeviceIds();

	int openConnection();
	int closeConnection();
	bool isConnected() const;

	// should return in how much milliseconds it wants to be called again
	virtual int update() = 0;

	virtual void setGCode(const std::string& gcode);
	virtual void appendGCode(const std::string& gcode);
	virtual void clearGCode();

	void startPrint(const std::string& gcode, STATE state = PRINTING);
	void startPrint(STATE state = PRINTING);
	void stopPrint();
	void stopPrint(const std::string& endcode);

	void heatup(int temperature);

	/*
	 * Get cached extruder temperature
	 */
	int getTemperature() const;

	/*
	 * Get cached extruder target temperature
	 */
	int getTargetTemperature() const;

	/*
	 * Get cached bed temperature
	 */
	int getBedTemperature() const;

	/*
	 * Get cached bed target temperature
	 */
	int getTargetBedTemperature() const;

	/*
	 * Get line number of codeBuffer that the printer should be printing
	 */
	int getCurrentLine() const;

	/*
	 * Get line number of codeBuffer that the printer should be printing
	 */
	int getNumLines() const;

	STATE getState() const;
	const std::string &getStateString(STATE state);

protected:
	int temperature_;
	int targetTemperature_;
	int bedTemperature_;
	int targetBedTemperature_;
	STATE state_;

	GCodeBuffer gcodeBuffer_;

	Serial serial_;
	Logger& log_;
	Server& server_;

	void printNextLine();
	void resetPrint();

	virtual void sendCode(const std::string& code) = 0;
	virtual void readResponseCode(std::string& code) = 0;

	void setState(STATE state);

	int readData();
	void setBaudrate(uint32_t baudrate);
	void switchBaudrate();

private:
	static const std::string STATE_NAMES[];

	typedef enum BAUDRATE{
		B115200 = 115200,
		B250000 = 250000
	} BAUDRATE;

	const std::string serialPortPath_;
	uint32_t baudrate_;
};

#endif /* ! ABSTRACT_DRIVER_H_SEEN */
