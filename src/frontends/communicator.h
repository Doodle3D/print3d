#ifndef COMMUNICATOR_H_SEEN
#define COMMUNICATOR_H_SEEN

#include <stdlib.h>

int getTemperature(const char* deviceId);
int setTemperatureCheckInterval(const char* deviceId, int interval);

#endif /* COMMUNICATOR_H_SEEN */
