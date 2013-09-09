#ifndef COMMUNICATOR_H_SEEN
#define COMMUNICATOR_H_SEEN

#include <inttypes.h>
#include <stdio.h>

int openSocketForDeviceId(const char* deviceId);
int closeSocket();
const char* getError();
int comm_testCommand(const char* deviceId, const char* question, char** answer);
int comm_getTemperature(const char* deviceId, int16_t* temperature);
int comm_sendGcodeFile(const char* deviceId, const char *file);

#endif /* COMMUNICATOR_H_SEEN */
