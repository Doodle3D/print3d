#ifndef COMMUNICATOR_H_SEEN
#define COMMUNICATOR_H_SEEN

#include <inttypes.h>
#include <stdio.h>

int comm_openSocketForDeviceId(const char *deviceId);
int comm_closeSocket();
const char *comm_getError();

int comm_testCommand(const char *deviceId, const char *question, char **answer);
int comm_getTemperature(const char *deviceId, int16_t *temperature);

int comm_clearGcode(const char *deviceId);
int comm_startPrintGcode(const char *deviceId);
int comm_stopPrintGcode(const char *deviceId);
int comm_sendGcodeFile(const char *deviceId, const char *file);
int comm_sendGcodeData(const char *deviceId, const char *gcode);

#endif /* COMMUNICATOR_H_SEEN */
