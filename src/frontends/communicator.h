#ifndef COMMUNICATOR_H_SEEN
#define COMMUNICATOR_H_SEEN

#include <inttypes.h>
#include <stdio.h>

int openSocketForDeviceId(const char* deviceId);
int closeSocket();
const char* getError();
int testCommand(const char* deviceId, const char* question, char** answer);
int getTemperature(const char* deviceId, int16_t* temperature);
int sendGcodeFile(const char* deviceId, const char *file);

#endif /* COMMUNICATOR_H_SEEN */
