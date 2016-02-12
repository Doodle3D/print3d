/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#ifndef COMMUNICATOR_H_SEEN
#define COMMUNICATOR_H_SEEN

#include <inttypes.h>
#include <stdio.h>
#include "../ipc_shared.h"

int comm_openSocket(const char *deviceId);
int comm_closeSocket();
int comm_is_socket_open();
const char *comm_getError();

int comm_testCommand(const char *question, char **answer);
int comm_testTransactions(const char *deviceId, char **outputText);

int comm_clearGCode();
int comm_startPrintGCode();
int comm_stopPrintGCode(const char *endCode);
int comm_sendGCodeFile(const char *file);
int comm_sendGCodeData(const char *gcode, ipc_gcode_metadata_s *metadata);

int comm_getTemperature(int16_t *temperature, IPC_TEMPERATURE_PARAMETER which);
int comm_heatup(int temperature);
int comm_getProgress(int32_t *currentLine, int32_t *bufferedLines, int32_t *numLines, int32_t *bufferSize, int32_t *maxBufferSize);
int comm_getState(char **state);

#endif /* COMMUNICATOR_H_SEEN */
