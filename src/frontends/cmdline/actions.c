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
	int16_t heating;

	if (comm_getTemperature(&hotendTemperature, IPC_TEMP_HOTEND) < 0) {
		printError(json_output, "could not read hotend temperature (%s)", comm_getError());
		rv = 1;
	}

	if (comm_getTemperature(&hotendTargetTemperature, IPC_TEMP_HOTEND_TGT) < 0) {
		printError(json_output, "could not read hotend target temperature (%s)", comm_getError());
		rv = 1;
	}

	if (comm_getTemperature(&bedTemperature, IPC_TEMP_BED) < 0) {
		printError(json_output, "could not read bed temperature (%s)", comm_getError());
		rv = 1;
	}

	if (comm_getTemperature(&bedTargetTemperature, IPC_TEMP_BED_TGT) < 0) {
		printError(json_output, "could not read bed target temperature (%s)", comm_getError());
		rv = 1;
	}

	if (comm_getTemperature(&heating, IPC_TEMP_HEATING) < 0) {
		printError(json_output, "could not read heating status (%s)", comm_getError());
		rv = 1;
	}

	comm_closeSocket();

	if (!json_output) {
		printf("actual/target hotend temperature: %i/%i; actual/target bed temperature: %i/%i, heating: %s\n",
				hotendTemperature, hotendTargetTemperature, bedTemperature, bedTargetTemperature,
				heating ? "yes" : "no");
	} else {
		printJsonOk("\"temperatures\": {\"hotend\": "
				"{\"current\": %i, \"target\": %i}, \"bed\": {\"current\": %i, \"target\": %i}, \"heating\": %s}",
				hotendTemperature, hotendTargetTemperature, bedTemperature, bedTargetTemperature,
				heating ? "true" : "false");
	}

	return rv;
}

static int act_printTestResponse() {
	char *question = "what?";
	char *answer;
	int rv;
	int result = 0;

	rv = comm_testCommand(0, &answer);
	if (rv > -1) {
		if (!json_output) printf("first test command returned: '%s'\n", answer);
	} else {
		printError(json_output, "could not complete first test command (%s)", comm_getError());
		result = 1;
	}

	//free(answer);

	rv = comm_testCommand(question, &answer);
	if (rv > -1) {
		if (!json_output) printf("second test command returned: '%s'\n", answer);
	} else {
		printError(json_output, "could not complete second test command (%s)", comm_getError());
		result = 1;
	}

	//free(answer);

	comm_closeSocket();

	if (rv > -1 && json_output) printJsonOk(NULL);
	return result;
}

/*
 * Note 1: this test cancels a current print in progress, if any.
 * Note 2: this function is special in that the actual test function opens multiple sockets itself, so we don't do that here.
 */
static int act_testTransactions() {
	char *resultText = NULL;
	int rv = comm_testTransactions(deviceId, &resultText);

	if (rv == 0) {
		if (!json_output) printf("ok, transaction tester returned: %s\n", resultText);
		else printJsonOk(NULL);
	} else {
		printError(json_output, "test failed (code: %i, text: '%i'", rv, resultText);
	}

	free(resultText);
	return rv == 0 ? 0 : 1;
}

static int act_sendGcodeFile(const char *file) {
	if (!isAbsolutePath(file)) {
		printError(json_output, "please supply an absolute path for the file to print");
		return 1;
	}

	if (comm_clearGCode() < 0) {
		printError(json_output, "could not clear gcode (%s)", comm_getError());
		return 1;
	}

	if (comm_sendGCodeFile(file) < 0) {
		printError(json_output, "could not send gcode file (%s)", comm_getError());
		return 1;
	}

	if (comm_startPrintGCode() < 0) {
		printError(json_output, "sent gcode file, but could not start print (%s)", comm_getError());
		return 1;
	}

	comm_closeSocket();

	if (!json_output) printf("sent gcode file and started print\n");
	else printJsonOk(NULL);

	return 0;
}

static int act_sendGcodeFromStdin() {
	if (comm_clearGCode() < 0) {
		printError(json_output, "could not clear gcode (%s)", comm_getError());
		return 1;
	}

	if (!json_output) printf("Please enter gcode and finish with C-d...\n");

	char *gcode = NULL;
	ssize_t gcode_len = 0, read_len = 0;
	while (1) {
		gcode_len += 128;
		gcode = (char*)realloc(gcode, gcode_len);
		ssize_t rv = read(STDIN_FILENO, gcode + read_len, gcode_len - read_len);

		if (rv < 0) {
			printError(json_output, "error reading from stdin (%s)", strerror(errno));
			free(gcode);
			return 1;
		}

		if (rv == 0) break;

		read_len += rv;
	}

	if (comm_sendGCodeData(gcode, -1, NULL) < 0) {
		printError(json_output, "could not send gcode data (%s)", comm_getError());
		return 1;
	}
	free(gcode);

	if (comm_startPrintGCode() < 0) {
		printError(json_output, "sent gcode data, but could not start print (%s)", comm_getError());
		return 1;
	}

	comm_closeSocket();

	if (!json_output) printf("sent gcode data and started print\n");
	else printJsonOk(NULL);

	return 0;
}

static int act_sendGcode(const char *gcode) {
	if (comm_clearGCode() < 0) {
		printError(json_output, "could not clear gcode (%s)", comm_getError());
		return 1;
	}

	if (comm_sendGCodeData(gcode, -1, NULL) < 0) {
		printError(json_output, "could not send gcode (%s)", comm_getError());
		return 1;
	}

	if (comm_startPrintGCode() < 0) {
		printError(json_output, "sent gcode, but could not start print (%s)", comm_getError());
		return 1;
	}

	comm_closeSocket();

	if (!json_output) printf("sent gcode and started print\n");
	else printJsonOk(NULL);

	return 0;
}

static int act_printProgress() {
	int32_t currentLine = INT32_MIN, bufferedLines = INT32_MIN, totalLines = INT32_MIN;
	int32_t bufferSize = INT32_MIN, maxBufferSize = INT32_MIN;
	int32_t sequenceNumber = INT32_MIN, sequenceTotal = INT32_MIN;

	if (comm_getProgress(&currentLine, &bufferedLines, &totalLines,
			&bufferSize, &maxBufferSize, &sequenceNumber, &sequenceTotal) < 0) {
		printError(json_output, "could not get printing progress (%s)", comm_getError());
		return 1;
	}

	comm_closeSocket();

	if (!json_output) {
		printf("print progress: %d of %d lines (%d buffered)", currentLine, totalLines, bufferedLines);

		if (totalLines != 0) printf(" (%.1f%%)", (float)currentLine / totalLines * 100);

		printf(", at capacity %d of %d bytes max (%.1f%%); sequence number/total: %d/%d\n",
				bufferSize, maxBufferSize, (float)bufferSize / maxBufferSize * 100,
				sequenceNumber, sequenceTotal);
	} else {
		printJsonOk("\"progress\": {\"current\": %d, \"total\": %d, \"buffered\": %d,"
				" \"buffer_size\": %d, \"max_buffer_size\": %d, \"sequence_number\": %d, \"sequence_total\": %d}",
				currentLine, totalLines, bufferedLines, bufferSize, maxBufferSize, sequenceNumber, sequenceTotal);
	}

	return 0;
}

static int act_printState() {
	char *state;

	if (comm_getState(&state) < 0) {
		printError(json_output, "could not get printer state (%s)", comm_getError());
		return 1;
	}

	//free(state);
	comm_closeSocket();

	if (!json_output) printf("printer state: '%s'\n", state);
	else printJsonOk("\"printer_state\": \"%s\"", state);

	return 0;
}

static int act_doHeatup(int temperature) {
	int rv = comm_heatup(temperature);
	comm_closeSocket();

	if (rv > -1) {
		if (!json_output) printf("requested printer heatup to %i degrees celcius\n", temperature);
		else printJsonOk("\"target_temperatures\": {\"hotend\": %i}", temperature);
		return 0;
	} else {
		printError(json_output, "could not request printer heatup (%s)", comm_getError());
		return 1;
	}
}

static int act_stopPrint(const char *endCode) {
	int rv = comm_stopPrintGCode(endCode);
	comm_closeSocket();

	if (rv > -1) {
		if (!json_output) {
			if (!endCode) printf("requested printing stop\n");
			else printf("requested printing stop with %zu bytes of end code\n", strlen(endCode));
		} else {
			size_t ecSize = endCode ? strlen(endCode) : 0;
			printJsonOk("\"end_code_size\": %zu", ecSize);
		}
		return 0;
	} else {
		printError(json_output, "could not request printing stop (%s)", comm_getError());
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
	case AT_GET_TEMPERATURE: case AT_GET_TEST: case AT_GET_PROGRESS: case AT_GET_STATE:
	case AT_HEATUP: case AT_PRINT_FILE: case AT_SEND_CODE: case AT_SEND_STDIN: case AT_STOP_PRINT:
		if(comm_openSocket(deviceId) < 0) {
			printError(json_output, "could not open socket (run in verbose mode for details)");
			return 1;
		}
		break;
	default: break;
	}

	switch (action) {
	case AT_ERR_UNKNOWN:
	case AT_ERR_MISSING:
	case AT_ERR_INVALID_GET:
		printError(json_output, "Action handler called with 'unknown', 'missing' or 'invalid_get' action, this should not happen.");
		//leave rv at 2
		break;
	case AT_NONE: case AT_SHOW_HELP:
		if (json_output) {
			printJsonOk("\"message\": \"help requested, ignored in JSON mode\"");
			return 0;
		}

		printf("Basic usage: '%s [<options>]'.\n", argv[0]);
		printf("The following options are available:\n");
		printf("\t-h,--help\t\tShow this help message\n");
		printf("\t-q,--quiet\t\tDo not print any output\n");
		printf("\t-v,--verbose\t\tPrint verbose output (repeat to enable bulk mode; default: log only errors and warnings)\n");
		printf("\t-j,--json\t\tEmit only JSON output (verbosity will be set to quiet)\n");
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
	case AT_GET_SUPPORTED: {
		//TODO: implement (and remember to open/close socket)
		const char *msg = "unimplemented: get supported";

		if (!json_output) printf("[dummy] %s\n", msg);
		else printJsonOk("\"message\": \"%s\"", msg);

		rv = 5;
		break;
	}

	case AT_HEATUP: rv = act_doHeatup(heatupTemperature); break;
	case AT_PRINT_FILE:
		if (!printFile) {
			printError(json_output, "error: missing filename to print");
			rv = 1;
		} else if (!isAbsolutePath(printFile)) {
			printError(json_output, "please supply an absolute path for the file to print");
			rv = 1;
		}

		return act_sendGcodeFile(printFile);

	case AT_SEND_CODE:
		if (!sendGcode) {
			printError(json_output, "error: missing g-code to print");
			return 1;
		}
		return act_sendGcode(sendGcode);

	case AT_SEND_STDIN: rv = act_sendGcodeFromStdin(); break;
	case AT_STOP_PRINT: rv = act_stopPrint(endGcode); break;
	}

	return rv;
}
