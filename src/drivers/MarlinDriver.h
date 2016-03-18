/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

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

	static AbstractDriver* create(Server& server, const std::string& serialPortPath, const uint32_t& baudrate);

protected:
	bool startPrint(STATE state);

	void readResponseCode(std::string& code);
	void parseTemperatures(std::string& code);
	void checkTemperature(bool logAsInfo = false);
	void sendCode(const std::string& code, bool logAsInfo = false);

private:
	static const int UPDATE_INTERVAL;

	Timer timer_;
	Timer temperatureTimer_;
	int checkTemperatureInterval_;
	bool checkConnection_;
	int checkTemperatureAttempt_;
	int maxCheckTemperatureAttempts_;
	int extractTemperatureFromMCode(const std::string& gcode, const std::string *codes, int num_codes);

	void filterText(std::string& text, const std::string& replace);
};

#endif /* ! MARLIN_DRIVER_H_SEEN */
