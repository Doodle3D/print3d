#include "MarlinDriver.h"
#include "DriverFactory.h"

AbstractDriver* DriverFactory::createDriver(const std::string& driverName, Server& server, const std::string& serialPortPath, const uint32_t& baudrate) {
  static vec_DriverInfoP driverInfos;


  Logger& log = Logger::getInstance();
  //log.log(Logger::VERBOSE, "DriverFactory::createDriver");

  // list all printer drivers (their driver info)
  if(driverInfos.empty()) {
    driverInfos.push_back( &MarlinDriver::getDriverInfo() );
    log.log(Logger::VERBOSE, "  num drivers: %i",driverInfos.size());
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

      //log.log(Logger::VERBOSE, "    firmware name: %s",(*f).name.c_str());
      // if match create driver instance
      if((*f).name == driverName) {
        log.log(Logger::INFO, "Created firmware: %s",(*f).name.c_str());
        return di.creator(server, serialPortPath,baudrate);
      }
    }
  }
 return 0;
}
