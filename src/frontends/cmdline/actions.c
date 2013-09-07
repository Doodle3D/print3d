#include <inttypes.h>
#include <stdio.h>
#include "../communicator.h"
#include "fe_cmdline.h"

void printTemperatureAction() {
	int16_t temperature;

	openSocketForDeviceId(deviceId);
	int rv = getTemperature(deviceId, &temperature);
	closeSocket();

	if (rv > -1) printf("temperature: %i\n", temperature);
	else fprintf(stderr, "could not read temperature (%s)\n", getError());
}

void printTestResponseAction() {
	char *question = "what?";
	char *answer;
	int rv;

	openSocketForDeviceId(deviceId);

	rv = testCommand(deviceId, 0, &answer);
	if (rv > -1) printf("first test command returned: '%s'\n", answer);
	else fprintf(stderr, "could not complete first test command (%s)\n", getError());

	//free(answer);

	rv = testCommand(deviceId, question, &answer);
	if (rv > -1) printf("second test command returned: '%s'\n", answer);
	else fprintf(stderr, "could not complete second test command (%s)\n", getError());

	//free(answer);

	closeSocket();
}

void sendGcodeFileAction(int fd) {
	int rv = sendGcodeFile(deviceId, fd);

	if (rv > -1) printf("sent gcode file\n");
	else fprintf(stderr, "could not send gcode file (%s)\n", getError());
}
