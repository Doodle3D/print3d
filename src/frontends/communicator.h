/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

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
int comm_stopPrintGcode(const char *endCode);
int comm_sendGcodeFile(const char *file);
int comm_sendGcodeData(const char *gcode);

int comm_heatup(int temperature);
int comm_getProgress(int32_t *currentLine, int32_t *bufferedLines, int32_t *numLines);
int comm_getState(char **state);

#endif /* COMMUNICATOR_H_SEEN */
