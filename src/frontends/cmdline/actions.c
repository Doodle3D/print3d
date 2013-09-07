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
	char *question = "what?";
	char *answer;
	int rv;

	rv = testCommand(deviceId, 0, &answer);
	if (rv > -1) printf("first test command returned: '%s'\n", answer);
	else fprintf(stderr, "could not complete first test command (%s)\n", getError());

	//free(answer);

	rv = testCommand(deviceId, question, &answer);
	if (rv > -1) printf("second test command returned: '%s'\n", answer);
	else fprintf(stderr, "could not complete second test command (%s)\n", getError());

	//free(answer);
}
