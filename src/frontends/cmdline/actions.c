#include <inttypes.h>
#include <stdio.h>
#include "../communicator.h"
#include "../../logger.h"
#include "../../utils.h"
#include "fe_cmdline.h"

static void act_printTemperature() {
	int16_t hotendTemperature = INT16_MIN, hotendTargetTemperature = INT16_MIN;
	int16_t bedTemperature = INT16_MIN, bedTargetTemperature = INT16_MIN;

	comm_openSocketForDeviceId(deviceId);

	if (comm_getTemperature(&hotendTemperature, IPC_TEMP_HOTEND) < 0)
		fprintf(stderr, "could not read hotend temperature (%s)\n", comm_getError());

	if (comm_getTemperature(&hotendTargetTemperature, IPC_TEMP_HOTEND_TGT) < 0)
		fprintf(stderr, "could not read hotend target temperature (%s)\n", comm_getError());

	if (comm_getTemperature(&bedTemperature, IPC_TEMP_BED) < 0)
		fprintf(stderr, "could not read bed temperature (%s)\n", comm_getError());

	if (comm_getTemperature(&bedTargetTemperature, IPC_TEMP_BED_TGT) < 0)
		fprintf(stderr, "could not read bed target temperature (%s)\n", comm_getError());

	comm_closeSocket();

	printf("actual/target hotend temperature: %i/%i; actual/target bed temperature: %i/%i\n",
			hotendTemperature, hotendTargetTemperature, bedTemperature, bedTargetTemperature);
}

static void act_printTestResponse() {
	char *question = "what?";
	char *answer;
	int rv;

	comm_openSocketForDeviceId(deviceId);

	rv = comm_testCommand(0, &answer);
	if (rv > -1) printf("first test command returned: '%s'\n", answer);
	else fprintf(stderr, "could not complete first test command (%s)\n", comm_getError());

	//free(answer);

	rv = comm_testCommand(question, &answer);
	if (rv > -1) printf("second test command returned: '%s'\n", answer);
	else fprintf(stderr, "could not complete second test command (%s)\n", comm_getError());

	//free(answer);

	comm_closeSocket();
}

static int act_sendGcodeFile(const char *file) {
	if (!isAbsolutePath(file)) {
		fprintf(stderr, "please supply an absolute path for the file to print\n");
		return 1;
	}

	comm_openSocketForDeviceId(deviceId);
	int rv = comm_sendGcodeFile(file);
	comm_closeSocket();

	if (rv > -1) {
		printf("sent gcode file\n");
		return 0;
	} else {
		fprintf(stderr, "could not send gcode file (%s)\n", comm_getError());
		return 1;
	}
}

static int act_sendGcode(const char *gcode) {
	comm_openSocketForDeviceId(deviceId);
	int rv = comm_sendGcodeData(gcode);
	comm_closeSocket();

	if (rv > -1) {
		printf("sent gcode\n");
		return 0;
	} else {
		fprintf(stderr, "could not send gcode (%s)\n", comm_getError());
		return 1;
	}
}

static int act_printProgress() {
	int16_t currentLine = INT16_MIN, numLines = INT16_MIN;

	comm_openSocketForDeviceId(deviceId);

	if (comm_getProgress(&currentLine, &numLines) < 0) {
		fprintf(stderr, "could not get printing progress (%s)\n", comm_getError());
		return 1;
	}

	comm_closeSocket();

	printf("print progress: %i of %i lines", currentLine, numLines);
	if (numLines != 0) printf(" (%.1f%%)\n", (float)currentLine / numLines * 100);

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


int handle_action(int argc, char **argv, ACTION_TYPE action) {
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
		printf("\t-w,--heatup <temp>\tAsk the printer to heatup to the given temperature (degrees celcius)\n");
		printf("\t-f,--gcode-file <file>\tPrint the given g-code file\n");
		printf("\t-c,--gcode <gcode>\tPrint the specified line of g-code\n");
		return 0;
	case AT_GET_TEMPERATURE:
		act_printTemperature();
		return 0;
	case AT_GET_TEST:
		act_printTestResponse();
		return 0;
	case AT_GET_PROGRESS:
		return act_printProgress();
	case AT_GET_STATE:
		return act_printState();
	case AT_GET_SUPPORTED:
		printf("[dummy] get supported\n");
		return 0;
	case AT_HEATUP:
		return act_doHeatup(heatup_temperature);
	case AT_PRINT_FILE:
		if (!print_file) {
			fprintf(stderr, "error: missing filename to print\n");
			return 1;
		}

		return act_sendGcodeFile(print_file);
	case AT_SEND_CODE:
		if (!send_gcode) {
			fprintf(stderr, "error: missing g-code to print\n");
			return 1;
		}
		act_sendGcode(send_gcode);
		return 0;
	}

	return 2;
}
