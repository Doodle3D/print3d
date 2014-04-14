/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#include <errno.h>
#include <getopt.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include "../ipc_shared.h"
#include "Logger.h"
#include "Server.h"

using std::string;
using std::cerr;
using std::endl;

static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"quieter", no_argument, NULL, 'Q'},
		{"quiet", no_argument, NULL, 'q'},
		{"verbose", no_argument, NULL, 'v'},
		{"verboser", no_argument, NULL, 'V'},
		{"fork", no_argument, NULL, 'f'},
		{"no-fork", no_argument, NULL, 'F'},
		{"force", no_argument, NULL, 'S'},
		{"device", required_argument, NULL, 'd'},
		{"printer", required_argument, NULL, 'p'},
		{NULL, 0, NULL, 0}
};


int main(int argc, char** argv) {
	string serialDevice = "";
	string printerName = "";
	int doFork = 0; //-1: don't fork, 0: leave default, 1: do fork
	bool showHelp = false, forceStart = false;
	Logger::ELOG_LEVEL logLevel = Logger::WARNING;
	char ch;

	while ((ch = getopt_long(argc, argv, "hQqvVfFSd:p:", long_options, NULL)) != -1) {
		switch (ch) {
			case 'h': showHelp = true; break;
			case 'Q': logLevel = Logger::QUIET; break;
			case 'q': logLevel = Logger::ERROR; break;
			case 'v': logLevel = Logger::VERBOSE; break;
			case 'V': logLevel = Logger::BULK; break;
			case 'f': doFork = 1; break;
			case 'F': doFork = -1; break;
			case 'S': forceStart = true; break;
			case 'd': serialDevice = optarg; break;
			case 'p': printerName = optarg; break;
		}
	}

	printf("Print 3D server\n");

	/* show some basic information on the platform type */
	printf("Platform information: ");
#ifdef __BIG_ENDIAN__
	printf("big endian");
#elif __LITTLE_ENDIAN__
	printf("little endian");
#else
	printf("unknown endianness (assuming big)");
#endif
	printf(", sizeof(short)=%zu, sizeof(int)=%zu, sizeof(long)=%zu, sizeof(float)=%zu\n", sizeof(short), sizeof(int), sizeof(long), sizeof(float));

	if (showHelp) {
		printf("The following options are accepted (forking is %s by default):\n", Server::FORK_BY_DEFAULT ? "on" : "off");
		printf("\t-h,--help\t\tDisplay this help message\n");
		printf("\t-Q,--quieter\t\tLog nothing\n");
		printf("\t-q,--quiet\t\tLog only errors\n");
		printf("\t-v,--verbose\t\tLog verbose\n");
		printf("\t-V,--verboser\t\tLog as much as possible\n");
		printf("\t-f,--fork\t\tFork the server process\n");
		printf("\t-F,--no-fork\t\tDo not fork the server process\n");
		printf("\t-S,--force\t\tForce starting the server with a default device if none could be found\n");
		printf("\t-d,--device\t\tThe printer serial device to use (any prefix path will be cut off)\n");
		printf("\t-p,--printer\t\tThe 3D printer driver to use (use help to get more information)\n");
		::exit(0);
	}

	if (serialDevice.empty()) {
		char **devlist = ipc_find_devices();

		if (!devlist) { //no list
			cerr << "error retrieving device list (" << strerror(errno) << ")" << endl;
			exit(1);
		}

		if (!devlist[0] && !forceStart) { //no devices and no force-start
			cerr << "no devices found, please connect a printer or re-run with '-S'" << endl;
			::exit(1);
		} else if (!devlist[0]) { //no devices but force-start requested
			serialDevice = IPC_DEFAULT_DEVICE_ID;
		} else if (devlist[1] != 0) { //more than one device
			cerr << "more than one device found (listed below), please specify one of the following:" << endl;
			for (int i = 0; devlist[i] != 0; i++) {
				const char *item = devlist[i];
				cerr << " '" << item << "'";
			}
			cerr << endl;
			::exit(1);
		} else {
			serialDevice = devlist[0];
		}

		ipc_free_device_list(devlist);
	} else {
		size_t lastSlash = serialDevice.rfind('/');
		if (lastSlash != string::npos) serialDevice = serialDevice.substr(lastSlash + 1);
	}
	std::cout << "Using printer device: '" << serialDevice << "'" << endl;

	std::cout << "printer type: '" << printerName << "'" << endl;
	if (printerName.compare("help") == 0) {
		// TODO: print list of available printers.
	}

	Logger& log = Logger::getInstance();
	log.open(stderr, logLevel);

	Server s(printerName, "/dev/" + serialDevice, ipc_construct_socket_path(serialDevice.c_str()));

	int rv;

	if (doFork == 0) rv = s.start();
	else rv = s.start((doFork == 1) ? true : false);

	if (rv >= 0) exit(0);
	else exit(1);
}
