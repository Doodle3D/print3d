#include <inttypes.h>
#include <stdio.h>
#include "../communicator.h"
#include "../../logger.h"
#include "../../utils.h"
#include "fe_cmdline.h"

static void act_printTemperature() {
	int16_t temperature;

	comm_openSocketForDeviceId(deviceId);
	int rv = comm_getTemperature(&temperature);
	comm_closeSocket();

	if (rv > -1) printf("temperature: %i\n", temperature);
	else fprintf(stderr, "could not read temperature (%s)\n", comm_getError());
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

static int action_sendGcodeFile(const char *file) {
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
	case AT_NONE: case AT_SHOW_HELP:
		printf("Basic usage: '%s [<options>]'.\n", argv[0]);
		printf("The following options are available:\n");
		printf("\t-h,--help\t\tShow this help message\n");
		printf("\t-q,--quiet\t\tDo not print any output\n");
		printf("\t-v,--verbose\t\tPrint verbose output\t\n");
		printf("\t-g,--get <parameter>\tRetrieve the given parameter (currently 'temperature' or 'test')\n");
		printf("\t-t,--get-temperature\tRetrieve the printer temperature\n");
		printf("\t-s,--supported\t\tRetrieve a list of supported printers\n");
		printf("\t-d,--device <device-id>\tPrint to the given device-id\n");
		printf("\t-f,--gcode-file <file>\tPrint the given g-code file\n");
		printf("\t-c,--gcode <gcode>\tPrint the specified line of g-code\n");
		return 0;
	case AT_GET_TEMPERATURE:
		act_printTemperature();
		return 0;
	case AT_GET_TEST:
		act_printTestResponse();
		return 0;
	case AT_GET_SUPPORTED:
		printf("[dummy] get supported\n");
		return 0;
	case AT_PRINT_FILE:
		if (!print_file) {
			fprintf(stderr, "error: missing filename to print\n");
			return 1;
		}

		return action_sendGcodeFile(print_file);
	case AT_SEND_CODE:
		if (!send_gcode) {
			fprintf(stderr, "error: missing g-code to print\n");
			return 1;
		}
		printf("[dummy] send gcode: '%s'\n", send_gcode);
		return 0;
	}

	return 2;
}
