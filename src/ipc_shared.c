#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logger.h"
#include "utils.h"
#include "ipc_shared.h"

//NOTE: this construct is here to provide plain C access to command names for frontends/communicator.c
#ifdef __cplusplus
extern "C" {
#endif

const ipc_cmd_name_s IPC_COMMANDS[] = {
		{IPC_CMD_TEST, "test"},
		{IPC_CMD_GET_TEMPERATURE, "getTemperature"}
};

const char* IPC_SOCKET_PATH_PREFIX = "/tmp/print3d-";

char* ipc_construct_socket_path(const char* deviceId) {
	char* buf = (char*)malloc(strlen(IPC_SOCKET_PATH_PREFIX) + strlen(deviceId) + 1);
	strcpy(buf, IPC_SOCKET_PATH_PREFIX);
	strcat(buf, deviceId);
	return buf;
}


//reallocates buf to 4 bytes (2 for command code and 2 argument count)
int ipc_cmd_set(char** buf, int* buflen, IPC_COMMAND_CODE code) {
	char* t = (char*)realloc(*buf, 4);
	if (!t) return -1;

	*buf = t;
	store_ns(*buf, code);
	store_ns(*buf + 2, 0);
	*buflen = 4;

	return 0;
}

//reallocates buf to make room for arg (4 bytes for length + the argument itself)
//if arg is NULL, an empty argument is added
int ipc_cmd_add_arg(char** buf, int* buflen, const char* arg, uint32_t arglen) {
	if (!arg) arglen = 0; //ensure argument length is 0 for null argument
	char* t = (char*)realloc(*buf, *buflen + 4 + arglen);
	if (!t) return -1;

	*buf = t;
	store_nl(*buf + *buflen, arglen); //write argument length
	if (arg) memcpy(*buf + *buflen + 4, arg, arglen); //write argument if non-null
	*buflen += 4 + arglen;
	store_ns(*buf + 2, read_ns(*buf + 2) + 1); //increment argument count

	return 0;
}

//returns command length if complete, 0 otherwise
int ipc_cmd_is_complete(const char* buf, int buflen) {
	if (buflen < 4) return 0;

	const char* p = buf + 2;
	int argsleft = read_ns(p);

	p += 2;
	while(argsleft) {
		if (buf + buflen < p + 4) return 0;
		uint32_t al = read_nl(p);
		if (buf + buflen < p + 4 + al) return 0;

		argsleft--;
		p += 4 + al;
	}

	return p - buf;
}

//returns the number of arguments in the command
int ipc_cmd_num_args(const char* buf, int buflen) {
	if (buflen < 4) return -2;
	return read_ns(buf + 2);
}

//extracts command code
IPC_COMMAND_CODE ipc_cmd_get(const char* buf, int buflen) {
	if (buflen < 2) return -2;
	return read_ns(buf);
}

//copies requested argument into argbuf (after reallocating) and returns its length (or -1 if the index is invalid)
//appends a nul-byte to create ASCIIZ string if requested
int ipc_cmd_get_arg(const char* buf, int buflen, char** argbuf, int* argbuflen, int argidx, int addzero) {
	const char* p = buf + 4;
	int curridx = 0;
	uint32_t currarglen;

	if (argidx >= read_ns(buf + 2)) return -2;

	while (curridx < argidx) {
		currarglen = read_nl(p);
		p += 4 + currarglen;
		curridx++;
	}

	if (addzero != 0) addzero = 1;
	char* t = (char*)realloc(*argbuf, currarglen + addzero);
	if (!t) return -1;

	*argbuf = t;
	memcpy(*argbuf, p + 4, currarglen);
	if (addzero) *(*argbuf + currarglen) = '\0';
	*argbuflen = currarglen + addzero;

	return 0;
}

int ipc_cmd_remove(char** buf, int* buflen) {
	int l = ipc_cmd_is_complete(*buf, *buflen);

	if (l == 0) return 0;

	memmove(*buf, *buf + l, *buflen - l);
	char* t = (char*)realloc(*buf, *buflen - l);
	if (!t) return -1;

	*buf = t;
	*buflen -= l;

	return 1;
}

#ifdef __cplusplus
} //extern "C"
#endif
