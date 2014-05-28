/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

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
		std::string name;
		vec_FirmwareDescription supportedFirmware;
		creatorFunc creator;                                //pointer to driver's create function
	};

	// drivers should provide a function which returns a reference to a DriverInfo object
	//   static const AbstractDriver::DriverInfo& getDriverInfo();

	typedef enum STATE{
		UNKNOWN = 0,
		DISCONNECTED = 1,
		CONNECTING,
		IDLE,
		BUFFERING,
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

	virtual GCodeBuffer::GCODE_SET_RESULT setGCode(const std::string& gcode, GCodeBuffer::MetaData *metaData = 0);
	virtual GCodeBuffer::GCODE_SET_RESULT appendGCode(const std::string& gcode, GCodeBuffer::MetaData *metaData = 0);
	virtual void clearGCode();

	virtual bool startPrint(const std::string& gcode, STATE state = PRINTING);
	virtual bool stopPrint(const std::string& endcode);
	virtual bool startPrint(STATE state = PRINTING);
	virtual bool stopPrint();

	void heatup(int temperature);

	uint16_t getTemperature() const;
	uint16_t getTargetTemperature() const;
	uint16_t getBedTemperature() const;
	uint16_t getTargetBedTemperature() const;

	virtual int32_t getCurrentLine() const;
	virtual int32_t getBufferedLines() const;
	virtual int32_t getTotalLines() const;

	STATE getState() const;
	static const std::string &getStateString(STATE state);

protected:
	static const bool REQUEST_EXIT_ON_PORT_FAIL;

	uint16_t temperature_;
	uint16_t targetTemperature_;
	uint16_t bedTemperature_;
	uint16_t targetBedTemperature_;
	STATE state_;

	GCodeBuffer gcodeBuffer_;

	Serial serial_;
	Logger& log_;
	Server& server_;

	virtual void sendCode(const std::string& code) = 0;
	virtual void readResponseCode(std::string& code) = 0;

	void printNextLine();
	virtual bool resetPrint();

	void setState(STATE state);

	bool isPrinterOnline() const;

	int readData();
	void setBaudrate(uint32_t baudrate);
	void switchBaudrate();

	int findNumber(const std::string& code, std::size_t startPos) const;
	void extractGCodeInfo(const std::string& gcode);

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
