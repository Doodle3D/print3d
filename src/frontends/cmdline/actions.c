/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
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

	rv = comm_getTemperature(&hotendTemperature, IPC_TEMP_HOTEND);
	rv = rv || comm_getTemperature(&hotendTargetTemperature, IPC_TEMP_HOTEND_TGT);
	rv = rv || comm_getTemperature(&bedTemperature, IPC_TEMP_BED);
	rv = rv || comm_getTemperature(&bedTargetTemperature, IPC_TEMP_BED_TGT);
	rv = rv || comm_getTemperature(&heating, IPC_TEMP_HEATING);

	comm_closeSocket();

	if (rv == 0) {
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
	} else {
		printError(json_output, "could not read one or more target temperatures (%s)", comm_getError());
	}

	return rv ? 1 : 0;
}

static int act_printTestResponse() {
	char *question = "what?";
	char *answer;
	int rv;

	rv = comm_testCommand(0, &answer);
	if (rv > -1 && !json_output) printf("first test command returned: '%s'\n", answer);
	//free(answer);

	rv = rv || comm_testCommand(question, &answer);
	if (rv > -1 && !json_output) printf("second test command returned: '%s'\n", answer);
	//free(answer);

	comm_closeSocket();

	if (rv) printError(json_output, "could not complete second test command (%s)", comm_getError());
	else if (json_output) printJsonOk(NULL);

	return rv ? 1 : 0;
}

static int act_sendGcodeFile(const char *file) {
	int rv = 0;

	if ((rv = comm_clearGcode()) < 0) {
		printError(json_output, "could not clear gcode (%s)", comm_getError());
	} else if ((rv = comm_sendGcodeFile(file)) < 0) {
		printError(json_output, "could not send gcode file (%s)", comm_getError());
	} else if ((rv = comm_startPrintGcode()) < 0) {
		printError(json_output, "sent gcode file, but could not start print (%s)", comm_getError());
	}

	comm_closeSocket();

	if (!rv) {
		if (!json_output) printf("sent gcode file and started print\n");
		else printJsonOk(NULL);
	}

	return rv ? 1 : 0;
}

static int act_sendGcodeFromStdin() {
	int rv = 0;
	char *gcode = NULL;

	if ((rv = comm_clearGcode()) < 0) {
		printError(json_output, "could not clear gcode (%s)", comm_getError());
	}

	if (rv == 0) {
		if (!json_output) printf("Please enter gcode and finish with C-d...\n");

		ssize_t gcode_len = 0, read_len = 0;
		while (1) {
			gcode_len += 128;
			gcode = (char*)realloc(gcode, gcode_len);
			ssize_t rv = read(STDIN_FILENO, gcode + read_len, gcode_len - read_len);

			if (rv < 0) {
				printError(json_output, "error reading from stdin (%s)", strerror(errno));
				free(gcode);
			}

			if (rv <= 0) break;

			read_len += rv;
		}
	}

	if (rv == 0) {
		if ((rv = comm_sendGcodeData(gcode)) < 0) {
			printError(json_output, "could not send gcode data (%s)", comm_getError());
		}
		free(gcode);
	}

	if (rv == 0) {
		if ((rv = comm_startPrintGcode()) < 0) {
			printError(json_output, "sent gcode data, but could not start print (%s)", comm_getError());
		}
	}

	comm_closeSocket();

	if (!rv) {
		if (!json_output) printf("sent gcode data and started print\n");
		else printJsonOk(NULL);
	}

	return rv ? 1 : 0;
}

static int act_sendGcode(const char *gcode) {
	int rv = 0;

	if ((rv = comm_clearGcode()) < 0) {
		printError(json_output, "could not clear gcode (%s)", comm_getError());
	} else if ((rv = comm_sendGcodeData(gcode)) < 0) {
		printError(json_output, "could not send gcode (%s)", comm_getError());
	} else if ((rv = comm_startPrintGcode()) < 0) {
		printError(json_output, "sent gcode, but could not start print (%s)", comm_getError());
	}

	comm_closeSocket();

	if (!rv) {
		if (!json_output) printf("sent gcode and started print\n");
		else printJsonOk(NULL);
	}

	return rv ? 1 : 0;
}

static int act_printProgress() {
	int32_t currentLine = INT32_MIN, bufferedLines = INT32_MIN, totalLines = INT32_MIN;

	int rv = comm_getProgress(&currentLine, &bufferedLines, &totalLines);
	comm_closeSocket();

	if (rv > -1) {
		if (!json_output) {
			printf("print progress: %d of %d lines (%d buffered)", currentLine, totalLines, bufferedLines);
			if (totalLines != 0) printf(" (%.1f%%)", (float)currentLine / totalLines * 100);
			printf("\n");
		} else {
			printJsonOk("\"progress\": {\"current\": %d, \"total\": %d, \"buffered\": %d}",
					currentLine, totalLines, bufferedLines);
		}

		return 0;
	} else {
		printError(json_output, "could not get printing progress (%s)", comm_getError());
		return 1;
	}
}

static int act_printState() {
	char *state;

	int rv = comm_getState(&state);
	comm_closeSocket();

	if (rv > -1) {
		if (!json_output) printf("printer state: '%s'\n", state);
		else printJsonOk("\"printer_state\": \"%s\"", state);

		//free(state);
		return 0;
	} else {
		printError(json_output, "could not get printer state (%s)", comm_getError());
		return 1;
	}
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
	int rv = comm_stopPrintGcode(endCode);
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
	switch(verbosity) {
	case -1:
		log_open_stream(stderr, LLVL_QUIET); break;
	case 1:
		log_open_stream(stderr, LLVL_VERBOSE); break;
	case 0: default:
		log_open_stream(stderr, LLVL_WARNING); break;
	}

	switch (action) {
	case AT_GET_TEMPERATURE: case AT_GET_TEST: case AT_GET_PROGRESS: case AT_GET_STATE:
	case AT_HEATUP: case AT_PRINT_FILE: case AT_SEND_CODE: case AT_SEND_STDIN: case AT_STOP_PRINT:
		if(comm_openSocketForDeviceId(deviceId) < 0) {
			printError(json_output, "could not open socket (run in verbose mode for details)");
			return 1;
		}
	default: break;
	}

	switch (action) {
	case AT_ERR_UNKNOWN:
	case AT_ERR_MISSING:
		printError(json_output, "Action handler called with 'error' action, this should not happen.");
		return 2;
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
		printf("\t-v,--verbose\t\tPrint verbose output\t\n");
		printf("\t-j,--json\t\tEmit only JSON output (verbosity will be set to quiet)\n");
		printf("\t-g,--get <parm>\t\tRetrieve the given parameter(s) (temperature|test|progress)\n");
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
		return 0;
	case AT_GET_TEMPERATURE:
		return act_printTemperature();
	case AT_GET_TEST:
		return act_printTestResponse();
	case AT_GET_PROGRESS:
		return act_printProgress();
	case AT_GET_STATE:
		return act_printState();
	case AT_GET_SUPPORTED: {
		//TODO: implement (and remember to open/close socket)
		const char *msg = "unimplemented: get supported";

		if (!json_output) printf("[dummy] %s\n", msg);
		else printJsonOk("\"message\": \"%s\"", msg);

		return 5;
	}
	case AT_HEATUP:
		return act_doHeatup(heatupTemperature);
	case AT_PRINT_FILE:
		if (!printFile) {
			printError(json_output, "error: missing filename to print");
			return 1;
		}
		if (!isAbsolutePath(printFile)) {
			printError(json_output, "please supply an absolute path for the file to print");
			return 1;
		}

		return act_sendGcodeFile(printFile);
	case AT_SEND_CODE:
		if (!sendGcode) {
			printError(json_output, "error: missing g-code to print");
			return 1;
		}
		return act_sendGcode(sendGcode);
	case AT_SEND_STDIN:
		return act_sendGcodeFromStdin();
	case AT_STOP_PRINT:
		return act_stopPrint(endGcode);
	}

	return 2;
}
