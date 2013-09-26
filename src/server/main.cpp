#include <stdlib.h>
#include <string>
#include "../ipc_shared.h"
#include "Logger.h"
#include "Server.h"

using std::string;

int main(int argc, char** argv) {
#ifdef __APPLE__
	//const string serialDevice = "/dev/tty.usbmodem1a21";
	const string serialDevice = "/dev/tty.usbmodemfd131";
	//const string serialDevice = "/dev/tty.usbserial-A10248X1";
#elif __linux
	const string serialDevice = "/dev/ttyACM0";
#endif

	//TODO: handle cmdline args

	Logger& log = Logger::getInstance();
	log.open(stderr, Logger::VERBOSE);

  log.log(Logger::INFO,"serialDevice: %s",serialDevice.c_str());

	Server s(serialDevice, ipc_construct_socket_path("xyz"));

	if (s.start() >= 0) exit(0);
	else exit(1);
}
