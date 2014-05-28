/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fe_cmdline.h"
#include "../../ipc_shared.h"


static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"quiet", no_argument, NULL, 'q'},
		{"verbose", no_argument, NULL, 'v'},

		{"get", required_argument, NULL, 'g'},
		{"temperature", no_argument, NULL, 't'},
		{"progress", no_argument, NULL, 'p'},
		{"state", no_argument, NULL, 's'},
		{"supported", no_argument, NULL, 'S'},

		{"device", required_argument, NULL, 'd'},
		{"force-run", no_argument, NULL, 'F'},
		{"heatup", required_argument, NULL, 'w'},
		{"gcode-file", required_argument, NULL, 'f'},
		{"gcode", required_argument, NULL, 'c'},
		{"stdin", no_argument, NULL, 'r'},
		{"stop", no_argument, NULL, 'k'},
		{"stop-with-code", required_argument, NULL, 'K'},

		{NULL, 0, NULL, 0}
};

int verbosity = 0; //-1 for quiet, 0 for normal, 1 for verbose
char *deviceId = NULL;
char *printFile = NULL, *sendGcode = NULL, *endGcode = NULL;
int heatupTemperature = -1;
int forceStart = 0;

static int deviceIdRequired = 0;
static ACTION_TYPE action = AT_NONE;


void parseOptions(int argc, char **argv) {
	int ch;
	while ((ch = getopt_long(argc, argv, "hqvg:tpsd:Fw:f:c:rkK:", long_options, NULL)) != -1) {
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
				deviceIdRequired = 1;
			} else if (strcmp(optarg, "progress") == 0) {
				action = AT_GET_PROGRESS;
				deviceIdRequired = 1;
			}
			break;
		case 't':
			action = AT_GET_TEMPERATURE;
			deviceIdRequired = 1;
			break;
		case 'p':
			action = AT_GET_PROGRESS;
			deviceIdRequired = 1;
			break;
		case 's':
			action = AT_GET_STATE;
			deviceIdRequired = 1;
			break;
		case 'S':
			action = AT_GET_SUPPORTED;
			break;
		case 'd':
			deviceId = optarg;
			break;
		case 'F':
			forceStart = 1;
			break;
		case 'w':
			action = AT_HEATUP;
			heatupTemperature = atoi(optarg);
			deviceIdRequired = 1;
			break;
		case 'f':
			printFile = optarg;
			action = AT_PRINT_FILE;
			deviceIdRequired = 1;
			break;
		case 'c':
			sendGcode = optarg;
			action = AT_SEND_CODE;
			deviceIdRequired = 1;
			break;
		case 'r':
			action = AT_SEND_STDIN;
			deviceIdRequired = 1;
			break;
		case 'k':
			action = AT_STOP_PRINT;
			deviceIdRequired = 1;
			break;
		case 'K':
			endGcode = optarg;
			action = AT_STOP_PRINT;
			deviceIdRequired = 1;
			break;
		case '?': /* signifies an error (like option with missing argument) */
			action = AT_ERROR;
			break;
		}
	}
}


int main(int argc, char **argv) {
	parseOptions(argc, argv);

	if (action == AT_ERROR) {
		exit(2);
	}

	if (verbosity >= 0) printf("Print3D command-line client%s\n", (verbosity > 0) ? " (verbose mode)" : "");

	char **devlist = NULL;
	if (deviceIdRequired && !deviceId) {
		devlist = ipc_find_devices();

		if (!devlist) { //no device list
			fprintf(stderr, "error: could not obtain device list\n");
			exit(1);
		}

		if (devlist[0] == NULL && forceStart == 0) { //no devices found, and no force-run
			fprintf(stderr, "no devices found, please connect a printer and start a server for it, or re-rerun with '-F'\n");
			exit(1);
		} else if (devlist[0] == NULL) { //no devices, but force-run requested
			deviceId = strdup(IPC_DEFAULT_DEVICE_ID); //NOTE: WARNING: technically this is a memory leak because deviceId is never freed
			printf("no device specified and none found, using default (%s)\n", deviceId);
		} else if (devlist[1] != NULL) { //multiple devices found
			fprintf(stderr, "more than one device found (listed below), please specify one the following:\n");
			for (int i = 0; devlist[i] != 0; i++) {
				const char *item = devlist[i];
				fprintf(stderr, " '%s'", item);
			}
			fprintf(stderr, "\n");
			exit(1);
		} else { //one device found
			deviceId = devlist[0];
		}
	}

	if (deviceId) {
		char *slashPtr = strrchr(deviceId, '/');
		if (slashPtr) memmove(deviceId, slashPtr + 1, strlen(deviceId) - (slashPtr - deviceId));
	}

	//NOTE: this may print (null) as device, which is okay for commands not requiring a server connection
	printf("Using device ID '%s'\n", deviceId);
	int rv = handleAction(argc, argv, action);
	ipc_free_device_list(devlist);

	exit(rv);
}
