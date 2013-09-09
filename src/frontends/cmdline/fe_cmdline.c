#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fe_cmdline.h"


typedef enum ACTION_TYPE {
		AT_NONE, AT_SHOW_HELP, AT_GET_TEMPERATURE, AT_GET_TEST, AT_GET_SUPPORTED, AT_PRINT_FILE, AT_SEND_CODE
} ACTION_TYPE;

static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"quiet", no_argument, NULL, 'q'},
		{"verbose", no_argument, NULL, 'v'},

		{"get", required_argument, NULL, 'g'},
		{"get-temperature", no_argument, NULL, 't'},
		{"supported", no_argument, NULL, 's'},

		{"device", required_argument, NULL, 'd'},
		{"gcode-file", required_argument, NULL, 'f'},
		{"gcode", required_argument, NULL, 'c'},

		{NULL, 0, NULL, 0}
};

char *deviceId = NULL;
char *print_file = NULL, *send_gcode = NULL;

static int deviceIdRequired = 0;
static int verbosity = 0; //-1 for quiet, 0 for normal, 1 for verbose
static ACTION_TYPE action = AT_NONE;


void parse_options(int argc, char **argv) {
	char ch;
	while ((ch = getopt_long(argc, argv, "hqvg:tsd:f:c:", long_options, NULL)) != -1) {
		switch (ch) {
		case 'h': action = AT_SHOW_HELP; break;
		case 'q': verbosity = -1; break;
		case 'v': verbosity = 1; break;
		case 'g':
			if (strcmp(optarg, "temperature") == 0) {
				action = AT_GET_TEMPERATURE;
				deviceIdRequired = 1;
			} else if (strcmp(optarg, "test") == 0) {
				action = AT_GET_TEST;
			}
			break;
		case 't':
			action = AT_GET_TEMPERATURE;
			deviceIdRequired = 1;
			break;
		case 's': action = AT_GET_SUPPORTED; break;
		case 'd':
			deviceId = optarg;
			break;
		case 'f':
			print_file = optarg;
			action = AT_PRINT_FILE;
			deviceIdRequired = 1;
			break;
		case 'c':
			send_gcode = optarg;
			action = AT_SEND_CODE;
			deviceIdRequired = 1;
			break;
		}
	}
}


int main(int argc, char **argv) {
	parse_options(argc, argv);

	if (verbosity >= 0) printf("Print3D command-line client%s\n", (verbosity > 0) ? " (verbose mode)" : "");

	if (deviceIdRequired && !deviceId) {
		deviceId = "xyz"; //default tho this for now
		fprintf(stderr, "error: missing device-id\n");
		exit(1);
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
		break;
	case AT_GET_TEMPERATURE:
		printTemperatureAction();
		break;
	case AT_GET_TEST:
		printTestResponseAction();
		break;
	case AT_GET_SUPPORTED:
		printf("[dummy] get supported\n");
		break;
	case AT_PRINT_FILE:
		if (!print_file) {
			fprintf(stderr, "error: missing filename to print\n");
			exit(1);
		}

		sendGcodeFileAction(print_file);
		break;
	case AT_SEND_CODE:
		if (!send_gcode) {
			fprintf(stderr, "error: missing g-code to print\n");
			exit(1);
		}
		printf("[dummy] send gcode: '%s'\n", send_gcode);
		break;
	}

	exit(0);
}
