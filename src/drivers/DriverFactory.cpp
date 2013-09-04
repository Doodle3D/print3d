#include "DriverFactory.h"
#include "UltimakerDriver.h"

AbstractDriver* DriverFactory::createDriver(const std::string& driverName, const std::string& serialPortPath) {
	if (driverName == "ultimaker") return new UltimakerDriver(serialPortPath);
	else return 0;
}
