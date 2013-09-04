#include <string>
#include "../ipc_shared.h"
#include "Logger.h"
#include "Server.h"

using std::string;

int main(int argc, char** argv) {
	const string serialDevice = "/dev/ttyACM0";

	//TODO: handle cmdline args

	Logger& log = Logger::getInstance();
	log.open(stderr, Logger::BULK);

	Server s(serialDevice, ipc_construct_socket_path("xyz"));

	if (s.start() >= 0) exit(0);
	else exit(1);
}
