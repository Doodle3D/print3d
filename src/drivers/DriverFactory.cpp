/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include "MarlinDriver.h"
#include "MakerbotDriver.h"
#include "DriverFactory.h"

//NOTE: see Server.cpp for comments on this macro
#define LOG(lvl, fmt, ...) Logger::getInstance().log(lvl, "DFCT", fmt, ##__VA_ARGS__)


AbstractDriver* DriverFactory::createDriver(const std::string& driverName, Server& server, const std::string& serialPortPath, const uint32_t& baudrate) {
  static vec_DriverInfoP driverInfos;

  //LOG(Logger::INFO, "createDriver()");

  // list all printer drivers (their driver info)
  if(driverInfos.empty()) {
    driverInfos.push_back( &MarlinDriver::getDriverInfo() );
    driverInfos.push_back(&MakerbotDriver::getDriverInfo());
    LOG(Logger::INFO, "  num drivers: %i",driverInfos.size());
  }

  // loop trough driver info's
  vec_DriverInfoP::iterator d;
  for (d = driverInfos.begin();
       d != driverInfos.end();
       ++d) {
    const AbstractDriver::DriverInfo& di = **d;

    // loop trough driver info's supported firmware
    AbstractDriver::vec_FirmwareDescription::const_iterator f;
    for (f = di.supportedFirmware.begin();
         f != di.supportedFirmware.end();
         ++f) {

      //LOG(Logger::VERBOSE, "    firmware name: %s",(*f).name.c_str());
      // if match create driver instance
      if((*f).name == driverName) {
        LOG(Logger::INFO, "Created firmware: %s",(*f).name.c_str());
        return di.creator(server, serialPortPath,baudrate);
      }
    }
  }
 return 0;
}
