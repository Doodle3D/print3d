#include "MarlinDriver.h"

MarlinDriver::MarlinDriver(const std::string& serialPortPath)
: AbstractDriver(serialPortPath)
{ /* empty */ }

int MarlinDriver::update() {
	return 0;
}

//STATIC
const AbstractDriver::DriverInfo& MarlinDriver::getDriverInfo() {
  static AbstractDriver::vec_FirmwareDescription supportedFirmware;
  static AbstractDriver::DriverInfo info;

  if (supportedFirmware.empty()) {
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("marlin_generic") );
		supportedFirmware.push_back( AbstractDriver::FirmwareDescription("marlin_ultimaker") );

    info.supportedFirmware = supportedFirmware;
    info.creator = &MarlinDriver::create;
  };

  return info;
}
AbstractDriver* MarlinDriver::create(const std::string& serialPortPath) {
  return new MarlinDriver(serialPortPath);
}
