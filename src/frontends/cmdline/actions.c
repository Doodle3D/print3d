#include <inttypes.h>
#include <stdio.h>
#include "../communicator.h"
#include "fe_cmdline.h"

void printTemperature() {
	int16_t temperature;
	int rv = getTemperature(deviceId, &temperature);

	if (rv > -1) printf("temperature: %i\n", temperature);
	else fprintf(stderr, "could not read temperature (%s)\n", getError());
}

void printTestResponse() {
	char *question = print_file;
	char *answer;
	int rv = testCommand(deviceId, question, &answer);

	if (rv > -1) printf("test command returned: '%s'\n", answer);
	else fprintf(stderr, "could not complete test command (%s)\n", getError());
}
