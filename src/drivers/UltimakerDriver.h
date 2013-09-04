#ifndef ULTIMAKER_DRIVER_H_SEEN
#define ULTIMAKER_DRIVER_H_SEEN

#include "AbstractDriver.h"

class UltimakerDriver : public AbstractDriver {
public:
	explicit UltimakerDriver(const std::string& serialPortPath);

	virtual const vec_PrinterDescription& getSupportedPrinters() const;
	virtual int update();

private:
};

#endif /* ! ULTIMAKER_DRIVER_H_SEEN */
