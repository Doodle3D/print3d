#ifndef COMMUNICATOR_H_SEEN
#define COMMUNICATOR_H_SEEN

#include <inttypes.h>
#include <stdio.h>
#include "../ipc_shared.h"

int comm_openSocketForDeviceId(const char *deviceId);
int comm_closeSocket();
const char *comm_getError();

int comm_testCommand(const char *question, char **answer);
int comm_getTemperature(int16_t *temperature, IPC_TEMPERATURE_PATAMETER which);

int comm_clearGcode();
int comm_startPrintGcode();
int comm_stopPrintGcode();
int comm_sendGcodeFile(const char *file);
int comm_sendGcodeData(const char *gcode);

int comm_heatup(int temperature);
int comm_getProgress(int16_t *currentLine, int16_t *numLines);
//TODO: int comm_getState(string? enum?);

#endif /* COMMUNICATOR_H_SEEN */
