/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "../communicator.h"
#include "../../logger.h"
#include "../../utils.h"
#include "fe_cmdline.h"

static int act_printTemperature() {
	int rv = 0;
	int16_t hotendTemperature = INT16_MIN, hotendTargetTemperature = INT16_MIN;
	int16_t bedTemperature = INT16_MIN, bedTargetTemperature = INT16_MIN;

	comm_openSocket(deviceId);

	if (comm_getTemperature(&hotendTemperature, IPC_TEMP_HOTEND) < 0) {
		fprintf(stderr, "could not read hotend temperature (%s)\n", comm_getError());
		rv = 1;
	}

	if (comm_getTemperature(&hotendTargetTemperature, IPC_TEMP_HOTEND_TGT) < 0) {
		fprintf(stderr, "could not read hotend target temperature (%s)\n", comm_getError());
		rv = 1;
	}

	if (comm_getTemperature(&bedTemperature, IPC_TEMP_BED) < 0) {
		fprintf(stderr, "could not read bed temperature (%s)\n", comm_getError());
		rv = 1;
	}

	if (comm_getTemperature(&bedTargetTemperature, IPC_TEMP_BED_TGT) < 0) {
		fprintf(stderr, "could not read bed target temperature (%s)\n", comm_getError());
		rv = 1;
	}

	comm_closeSocket();

	printf("actual/target hotend temperature: %i/%i; actual/target bed temperature: %i/%i\n",
			hotendTemperature, hotendTargetTemperature, bedTemperature, bedTargetTemperature);

	return rv;
}

static int act_printTestResponse() {
	char *question = "what?";
	char *answer;
	int rv;
	int result = 0;

	comm_openSocket(deviceId);

	rv = comm_testCommand(0, &answer);
	if (rv > -1) {
		printf("first test command returned: '%s'\n", answer);
	} else {
		fprintf(stderr, "could not complete first test command (%s)\n", comm_getError());
		result = 1;
	}

	//free(answer);

	rv = comm_testCommand(question, &answer);
	if (rv > -1) {
		printf("second test command returned: '%s'\n", answer);
	} else {
		fprintf(stderr, "could not complete second test command (%s)\n", comm_getError());
		result = 1;
	}

	//free(answer);

	comm_closeSocket();

	return result;
}

//Note: this function is special in that the actual test function opens multiple sockets itself, so we don't do that here.
static int act_testTransactions() {
	char *resultText = NULL;
	int rv = comm_testTransactions(deviceId, &resultText);
	printf("transaction tester returned: %s\n", resultText);
	free(resultText);
	return rv == 0 ? 0 : 1;
}

static int act_sendGcodeFile(const char *file) {
	if (!isAbsolutePath(file)) {
		fprintf(stderr, "please supply an absolute path for the file to print\n");
		return 1;
	}

	comm_openSocket(deviceId);

	if (comm_clearGCode() < 0) {
		fprintf(stderr, "could not clear gcode (%s)\n", comm_getError());
		return 1;
	}

	if (comm_sendGCodeFile(file) < 0) {
		fprintf(stderr, "could not send gcode file (%s)\n", comm_getError());
		return 1;
	}

	if (comm_startPrintGCode() < 0) {
		fprintf(stderr, "sent gcode file, but could not start print (%s)\n", comm_getError());
		return 1;
	}

	comm_closeSocket();

	printf("sent gcode file and started print\n");
	return 0;
}

static int act_sendGcodeFromStdin() {
	comm_openSocket(deviceId);

	if (comm_clearGCode() < 0) {
		fprintf(stderr, "could not clear gcode (%s)\n", comm_getError());
		return 1;
	}

	printf("Please enter gcode and finish with C-d...\n");

	char *gcode = NULL;
	ssize_t gcode_len = 0, read_len = 0;
	while (1) {
		gcode_len += 128;
		gcode = (char*)realloc(gcode, gcode_len);
		ssize_t rv = read(STDIN_FILENO, gcode + read_len, gcode_len - read_len);

		if (rv < 0) {
			fprintf(stderr, "error reading from stdin (%s)\n", strerror(errno));
			free(gcode);
			return 1;
		}

		if (rv == 0) break;

		read_len += rv;
	}

	if (comm_sendGCodeData(gcode, NULL) < 0) {
		fprintf(stderr, "could not send gcode data (%s)\n", comm_getError());
		return 1;
	}
	free(gcode);

	if (comm_startPrintGCode() < 0) {
		fprintf(stderr, "sent gcode data, but could not start print (%s)\n", comm_getError());
		return 1;
	}

	comm_closeSocket();

	printf("sent gcode data and started print\n");
	return 0;
}

static int act_sendGcode(const char *gcode) {
	comm_openSocket(deviceId);

	if (comm_clearGCode() < 0) {
		fprintf(stderr, "could not clear gcode (%s)\n", comm_getError());
		return 1;
	}

	if (comm_sendGCodeData(gcode, NULL) < 0) {
		fprintf(stderr, "could not send gcode (%s)\n", comm_getError());
		return 1;
	}

	if (comm_startPrintGCode() < 0) {
		fprintf(stderr, "sent gcode, but could not start print (%s)\n", comm_getError());
		return 1;
	}

	comm_closeSocket();

	printf("sent gcode and started print\n");
	return 0;
}

static int act_printProgress() {
	int32_t currentLine = INT32_MIN, bufferedLines = INT32_MIN, totalLines = INT32_MIN;

	comm_openSocket(deviceId);

	if (comm_getProgress(&currentLine, &bufferedLines, &totalLines) < 0) {
		fprintf(stderr, "could not get printing progress (%s)\n", comm_getError());
		return 1;
	}

	comm_closeSocket();

	printf("print progress: %d of %d lines (%d buffered)", currentLine, totalLines, bufferedLines);
	if (totalLines != 0) printf(" (%.1f%%)", (float)currentLine / totalLines * 100);
	printf("\n");

	return 0;
}

static int act_printState() {
	char *state;

	comm_openSocket(deviceId);

	if (comm_getState(&state) < 0) {
		fprintf(stderr, "could not get printer state (%s)\n", comm_getError());
		return 1;
	}

	//free(state);
	comm_closeSocket();

	printf("printer state: '%s'\n", state);

	return 0;
}

static int act_doHeatup(int temperature) {
	comm_openSocket(deviceId);
	int rv = comm_heatup(temperature);
	comm_closeSocket();

	if (rv > -1) {
		printf("requested printer heatup to %i degrees celcius\n", temperature);
		return 0;
	} else {
		fprintf(stderr, "could not request printer heatup (%s)\n", comm_getError());
		return 1;
	}
}

static int act_stopPrint(const char *endCode) {
	comm_openSocket(deviceId);
	int rv = comm_stopPrintGCode(endCode);
	comm_closeSocket();

	if (rv > -1) {
		if (!endCode) printf("requested printing stop\n");
		else printf("requested printing stop with %zu bytes of end code\n", strlen(endCode));
		return 0;
	} else {
		fprintf(stderr, "could not request printing stop (%s)\n", comm_getError());
		return 1;
	}
}


int handleAction(int argc, char **argv, ACTION_TYPE action) {
	int rv = 2;

	if (verbosity < -1) verbosity = -1;
	else if (verbosity > 2) verbosity = 2;

	switch(verbosity) {
	case -1: log_open_stream(stderr, LLVL_QUIET); break;
	case 1: log_open_stream(stderr, LLVL_VERBOSE); break;
	case 2: log_open_stream(stderr, LLVL_BULK); break;
	case 0: default: log_open_stream(stderr, LLVL_WARNING); break;
	}

	switch (action) {
	case AT_ERROR:
		fprintf(stderr, "Action handler called with 'error' action, this should not happen.\n");
		//leave rv at 2
		break;

	case AT_NONE:
		printf("Error: nothing to do, please specify an operation to run\n\n");
		/* fall-through to help */
	case AT_SHOW_HELP:
		printf("Basic usage: '%s [<options>]'.\n", argv[0]);
		printf("The following options are available:\n");
		printf("\t-h,--help\t\tShow this help message\n");
		printf("\t-q,--quiet\t\tDo not print any output\n");
		printf("\t-v,--verbose\t\tPrint verbose output (repeat to increase verbosity)\n");
		printf("\t-g,--get <parm>\t\tRetrieve the given parameter(s) (temperature|test|trxtest|progress)\n");
		printf("\t-t,--temperature\tRetrieve the printer temperature\n");
		printf("\t-p,--progress\t\tRetrieve printing progress\n");
		printf("\t-s,--state\t\tQuery the printer state\n");
		printf("\t-S,--supported\t\tRetrieve a list of supported printers\n");
		printf("\t-d,--device <dev-id>\tPrint to the given device-id\n");
		printf("\t-F,--force-run\t\tForce running with a default device if none could be found\n");
		printf("\t-w,--heatup <temp>\tAsk the printer to heatup to the given temperature (degrees celcius)\n");
		printf("\t-f,--gcode-file <file>\tPrint the given g-code file\n");
		printf("\t-c,--gcode <gcode>\tPrint the specified line(s) of g-code\n");
		printf("\t-r,--stdin\t\tPrint g-code read from stdin (end interactive input with C-d)\n");
		printf("\t-k,--stop\t\tStop printing\n");
		printf("\t-K,--stop-with-code <gcode> Stop printing, with specified end code\n");
		rv = 2;
		break;

	case AT_GET_TEMPERATURE: rv = act_printTemperature(); break;
	case AT_GET_TEST: rv = act_printTestResponse(); break;
	case AT_GET_TRX_TEST: rv = act_testTransactions(); break;
	case AT_GET_PROGRESS: rv = act_printProgress(); break;
	case AT_GET_STATE: rv = act_printState(); break;
	case AT_GET_SUPPORTED:
		printf("[dummy] get supported\n");
		rv = 5;
		break;

	case AT_HEATUP: rv = act_doHeatup(heatupTemperature); break;
	case AT_PRINT_FILE:
		if (!printFile) {
			fprintf(stderr, "error: missing filename to print\n");
			rv = 1;
		}

		return act_sendGcodeFile(printFile);

	case AT_SEND_CODE:
		if (!sendGcode) {
			fprintf(stderr, "error: missing g-code to print\n");
			return 1;
		}
		return act_sendGcode(sendGcode);

	case AT_SEND_STDIN: rv = act_sendGcodeFromStdin(); break;
	case AT_STOP_PRINT: rv = act_stopPrint(endGcode); break;
	}

	return rv;
}
