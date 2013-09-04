#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logger.h"
#include "ipc_shared.h"

//NOTE: this construct is here to provide plain C access to command names for frontends/communicator.c
#ifdef __cplusplus
extern "C" {
#endif

const char* IPC_COMMAND_NAMES[] = {
		"test", "getTemperature"
};

const char* IPC_SOCKET_PATH_PREFIX = "/tmp/print3d-";


char* number_to_string(int n) {
	int asclen = (int)((ceil(log10(fabs(n))) + 2) * sizeof(char));
	char* arglenbuf = (char*)malloc(asclen);

	snprintf(arglenbuf, asclen, "%i", n); //TODO: check return value?

	return arglenbuf;
}


char* ipc_construct_socket_path(const char* deviceId) {
	char* buf = (char*)malloc(strlen(IPC_SOCKET_PATH_PREFIX) + strlen(deviceId) + 1);
	strcpy(buf, IPC_SOCKET_PATH_PREFIX);
	strcat(buf, deviceId);
	return buf;
}

//returns newly allocated buffer (size is written to *len), arg is ignored atm
char* ipc_construct_command(const char* name, const char* arg, int* len) {
	static const char* colon = ":";
	if (len) *len = strlen(name) + 2;
	char* buf = (char*)malloc(*len);

	if(arg) {
		int arglen = strlen(arg);
		char* arglenbuf = number_to_string(arglen);

		int totalcmdlen = *len + 1 + strlen(arglenbuf) + 1 + strlen(arg); //includes NUL but still misses semicolon!
		log_message(LLVL_WARNING, "CMD '%s:%s:%s', cmdlen=%i (arglen=%i)", name, arglenbuf, arg, totalcmdlen, strlen(arg));//TEMP

		//TODO: HIER (update length etc)

		//increment *len with strlen(arg) + arglenbuf + 2 //2 colons...and still allowing room for the final ';\0'
		//strcat colon; then strcat arglenbuf; then strcat another colon

		free(arglenbuf);
	}

	strcpy(buf, name);
	buf[*len - 2] = ';';
	buf[*len - 1] = '\0';

	return buf;
}

#ifdef __cplusplus
} //extern "C"
#endif
