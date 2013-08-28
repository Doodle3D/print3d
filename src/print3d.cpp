#include <stdlib.h>
#include "Logger.h"
#include "Serial.h"

int main(int argc, char** argv) {
	const char* serialDevice = "/dev/ttyACM0";

	//handle cmdline args
	//open logger
	//open fifo/file for IPC
	//fork (as late as possible but before any possibly lengthy operations like serial port things)
	//open port
	//set (detect?) speed

	//wait for commands
	//  if no printer object yet, or type config changed, create printer object
	//  dispatch request to printer object

	Logger l;
	l.open(stderr, Logger::BULK);

	Serial s;
	s.open(serialDevice);

	s.close();
	exit(0);
}
