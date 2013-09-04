#include "UltimakerDriver.h"

UltimakerDriver::UltimakerDriver(const std::string& serialPortPath)
: AbstractDriver(serialPortPath)
{ /* empty */ }

const AbstractDriver::vec_PrinterDescription& UltimakerDriver::getSupportedPrinters() const {
	static vec_PrinterDescription types;

	if (types.empty()) {
		types.push_back( (PrinterDescription){ .name = "dummy" } );
		types.push_back( (PrinterDescription){ .name = "generic" } );
	}

	return types;
}

int UltimakerDriver::update() {
	return 0;
}
