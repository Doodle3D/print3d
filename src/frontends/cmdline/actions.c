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

	comm_openSocketForDeviceId(deviceId);

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

	comm_openSocketForDeviceId(deviceId);

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

static int act_sendGcodeFile(const char *file) {
	if (!isAbsolutePath(file)) {
		fprintf(stderr, "please supply an absolute path for the file to print\n");
		return 1;
	}

	comm_openSocketForDeviceId(deviceId);

	if (comm_clearGcode() < 0) {
		fprintf(stderr, "could not clear gcode (%s)\n", comm_getError());
		return 1;
	}

	if (comm_sendGcodeFile(file) < 0) {
		fprintf(stderr, "could not send gcode file (%s)\n", comm_getError());
		return 1;
	}

	if (comm_startPrintGcode() < 0) {
		fprintf(stderr, "sent gcode file, but could not start print (%s)\n", comm_getError());
		return 1;
	}

	comm_closeSocket();

	printf("sent gcode file and started print\n");
	return 0;
}

static int act_sendGcodeFromStdin() {
	comm_openSocketForDeviceId(deviceId);

	if (comm_clearGcode() < 0) {
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

	if (comm_sendGcodeData(gcode) < 0) {
		fprintf(stderr, "could not send gcode data (%s)\n", comm_getError());
		return 1;
	}
	free(gcode);

	if (comm_startPrintGcode() < 0) {
		fprintf(stderr, "sent gcode data, but could not start print (%s)\n", comm_getError());
		return 1;
	}

	comm_closeSocket();

	printf("sent gcode data and started print\n");
	return 0;
}

static int act_sendGcode(const char *gcode) {
	comm_openSocketForDeviceId(deviceId);

	if (comm_clearGcode() < 0) {
		fprintf(stderr, "could not clear gcode (%s)\n", comm_getError());
		return 1;
	}

	if (comm_sendGcodeData(gcode) < 0) {
		fprintf(stderr, "could not send gcode (%s)\n", comm_getError());
		return 1;
	}

	if (comm_startPrintGcode() < 0) {
		fprintf(stderr, "sent gcode, but could not start print (%s)\n", comm_getError());
		return 1;
	}

	comm_closeSocket();

	printf("sent gcode and started print\n");
	return 0;
}

static int act_printProgress() {
	int32_t currentLine = INT32_MIN, bufferedLines = INT32_MIN, totalLines = INT32_MIN;

	comm_openSocketForDeviceId(deviceId);

	if (comm_getProgress(&currentLine, &bufferedLines, &totalLines) < 0) {
		fprintf(stderr, "could not get printing progress (%s)\n", comm_getError());
		return 1;
	}

	comm_closeSocket();

	printf("print progress: %zi of %zi lines (%zi buffered)", currentLine, totalLines, bufferedLines);
	if (totalLines != 0) printf(" (%.1f%%)", (float)currentLine / totalLines * 100);
	printf("\n");

	return 0;
}

static int act_printState() {
	char *state;

	comm_openSocketForDeviceId(deviceId);

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
	comm_openSocketForDeviceId(deviceId);
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
	comm_openSocketForDeviceId(deviceId);
	int rv = comm_stopPrintGcode(endCode);
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
	switch(verbosity) {
	case -1:
		log_open_stream(stderr, LLVL_QUIET); break;
	case 1:
		log_open_stream(stderr, LLVL_VERBOSE); break;
	case 0: default:
		log_open_stream(stderr, LLVL_WARNING); break;
	}

	switch (action) {
	case AT_ERROR:
		fprintf(stderr, "Action handler called with 'error' action, this should not happen.\n");
		return 2;
		break;
	case AT_NONE: case AT_SHOW_HELP:
		printf("Basic usage: '%s [<options>]'.\n", argv[0]);
		printf("The following options are available:\n");
		printf("\t-h,--help\t\tShow this help message\n");
		printf("\t-q,--quiet\t\tDo not print any output\n");
		printf("\t-v,--verbose\t\tPrint verbose output\t\n");
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
	case AT_GET_SUPPORTED:
		printf("[dummy] get supported\n");
		return 5;
	case AT_HEATUP:
		return act_doHeatup(heatupTemperature);
	case AT_PRINT_FILE:
		if (!printFile) {
			fprintf(stderr, "error: missing filename to print\n");
			return 1;
		}

		return act_sendGcodeFile(printFile);
	case AT_SEND_CODE:
		if (!sendGcode) {
			fprintf(stderr, "error: missing g-code to print\n");
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
