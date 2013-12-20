/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#ifndef DRIVER_FACTORY_H_SEEN
#define DRIVER_FACTORY_H_SEEN

#include <string>
#include <vector>
#include "AbstractDriver.h"
#include "../server/Logger.h"

class DriverFactory {
public:
	static AbstractDriver* createDriver(const std::string& driverName, Server& server, const std::string& serialPortPath, const uint32_t& baudrate);

private:
  typedef std::vector<const AbstractDriver::DriverInfo*> vec_DriverInfoP;

	/*
	 * TODO:
	 * - function to check whether a given driver instance is still correct for current config;
	 *   and return a new instance of correct type if not (use copy constructor of driver classes to clone)
	 * - provide function to return all driver names
	 * - convencience function to return all supported printer types for all drivers (query each driver and collect results)
	 */
};

#endif /* ! DRIVER_FACTORY_H_SEEN */
