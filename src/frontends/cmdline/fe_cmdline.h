/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#ifndef FE_CMDLINE_H_SEEN
#define FE_CMDLINE_H_SEEN

#include <stdio.h>


/* from fe_cmdline.c */
typedef enum ACTION_TYPE {
		AT_NONE, AT_ERR_UNKNOWN, AT_ERR_MISSING, AT_SHOW_HELP,
		AT_GET_TEMPERATURE, AT_GET_TEST, AT_GET_TRX_TEST, AT_GET_PROGRESS, AT_GET_STATE, AT_GET_SUPPORTED,
		AT_HEATUP, AT_PRINT_FILE, AT_SEND_CODE, AT_SEND_STDIN, AT_STOP_PRINT
} ACTION_TYPE;

extern int verbosity;
extern int json_output;
extern char *deviceId;
extern char *printFile;
extern char *sendGcode;
extern char *endGcode;
extern int heatupTemperature;
extern int forceStart;

/* from fe_cmdline.c */
void printError(int output_as_json, const char* format, ...);
void printJsonOk(const char *format, ...);

/* from actions.c */
int handleAction(int argc, char **argv, ACTION_TYPE action);

#endif /* ! FE_CMDLINE_H_SEEN */
