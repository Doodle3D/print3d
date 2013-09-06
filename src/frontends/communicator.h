#ifndef COMMUNICATOR_H_SEEN
#define COMMUNICATOR_H_SEEN

#include <inttypes.h>

const char* getError();
int getTemperature(const char* deviceId, int16_t* temperature);
int setTemperatureCheckInterval(const char* deviceId, int interval);

#endif /* COMMUNICATOR_H_SEEN */
