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

#ifdef __APPLE__
	const string defaultDevice = "tty.usbmodem1a21";
	//const string defaultDevice = "tty.usbmodemfd131";
	//const string defaultDevice = "tty.usbserial-A10248X1";
#elif __linux
	const string defaultDevice = "ttyACM0";
#endif

static struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
//		{"quiet", no_argument, NULL, 'q'},
//		{"verbose", no_argument, NULL, 'v'},
		{"fork", no_argument, NULL, 'f'},
		{"no-fork", no_argument, NULL, 'F'},
		{"force", no_argument, NULL, 'S'},
		{"device", required_argument, NULL, 'd'},
		{NULL, 0, NULL, 0}
};


int main(int argc, char** argv) {
	string serialDevice = "";
	int doFork = 0; //-1: don't fork, 0: leave default, 1: do fork
	bool showHelp = false, forceStart = false;
	char ch;

	while ((ch = getopt_long(argc, argv, "hfFSd:", long_options, NULL)) != -1) {
		switch (ch) {
			case 'h': showHelp = true; break;
			case 'f': doFork = 1; break;
			case 'F': doFork = -1; break;
			case 'S': forceStart = true; break;
			case 'd': serialDevice = optarg; break;
		}
	}

	printf("Print 3D server\n");

	if (showHelp) {
		printf("The following options are accepted (forking is %s by default):\n", Server::FORK_BY_DEFAULT ? "on" : "off");
		printf("\t-h,--help\t\tDisplay this help message\n");
		printf("\t-f,--fork\t\tFork the server process\n");
		printf("\t-F,--no-fork\t\tDo not fork the server process\n");
		printf("\t-S,--force\t\tForce starting the server with a default device if none could be found\n");
		printf("\t-d,--device\t\tThe printer serial device to use (any prefix path will be cut off)\n");
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
			serialDevice = defaultDevice;
		} else if (devlist[1] != 0) { //more than one device
			cerr << "more than one device found (listed below), please specify one the following" << endl;
			cerr << "found devices:";
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
		int lastSlash = serialDevice.rfind('/');
		if (lastSlash != string::npos) serialDevice = serialDevice.substr(lastSlash + 1);
	}

	std::cout << "Using printer device: '" << serialDevice << "'" << endl;

	Logger& log = Logger::getInstance();
	log.open(stderr, Logger::VERBOSE);

  log.log(Logger::INFO,"serialDevice: %s",serialDevice.c_str());

	Server s(serialDevice, ipc_construct_socket_path(serialDevice.c_str()));

	int rv;

	//TODO: test
	if (doFork == 0) rv = s.start();
	else rv = s.start((doFork == 1) ? true : false);

	if (rv >= 0) exit(0);
	else exit(1);
}
