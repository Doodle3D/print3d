#include <inttypes.h>
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
		/* special codes (not sent or received) */
		{ IPC_CMDS_INVALID, "invalid", "-", "" },
		{ IPC_CMDS_NONE, "none", "-", "" },

		/* request commands sent by clients */
		{ IPC_CMDQ_TEST, "test", "*", "*" },
		{ IPC_CMDQ_GET_TEMPERATURE, "getTemperature", "", "w" },
		{ IPC_CMDQ_CLEAR_GCODE, "clearGcode", "", "" },
		{ IPC_CMDQ_APPEND_GCODE, "appendGcode", "s", "" },
		{ IPC_CMDQ_PRINT_GCODE, "printGcode", "", "" },

		/* response commands send by server */
		{ IPC_CMDR_OK, "ok", "*", NULL },
		{ IPC_CMDR_ERROR, "error", "is", NULL },
		{ IPC_CMDR_NOT_IMPLEMENTED, "not_implemented", "", NULL },

		{ 0, NULL, NULL, NULL } /* sentinel */
};

const char* IPC_SOCKET_PATH_PREFIX = "/tmp/print3d-";


static const ipc_cmd_name_s* findCommandDescription(IPC_COMMAND_CODE code) {
	const ipc_cmd_name_s* d = IPC_COMMANDS;

	while (d->name) {
		if (d->code == code) return d;
		d++;
	}

	return 0;
}

char* ipc_construct_socket_path(const char* deviceId) {
	char* buf = (char*)malloc(strlen(IPC_SOCKET_PATH_PREFIX) + strlen(deviceId) + 1);
	strcpy(buf, IPC_SOCKET_PATH_PREFIX);
	strcat(buf, deviceId);
	return buf;
}

char* ipc_construct_cmd(int* cmdlen, IPC_COMMAND_CODE code, const char* format, ...) {
	va_list args;
	va_start(args, format);
	char* result = ipc_va_construct_cmd(cmdlen, code, format, args);
	va_end(args);
	return result;
}

char* ipc_va_construct_cmd(int* cmdlen, IPC_COMMAND_CODE code, const char* format, va_list args) {
	//TODO: use format associated with code, unless it is equal to "*", then use format
	const ipc_cmd_name_s* description = findCommandDescription(code);
	const char* fmtp = (strcmp(description->arg_fmt, "*") == 0) ? format : description->arg_fmt;
	int rv;

	if (!fmtp) return NULL;

	char* cmd = 0;
	*cmdlen = 0;

	rv = ipc_cmd_set(&cmd, cmdlen, code);

	while(*fmtp) {
		char argtype = *fmtp;

		switch(argtype) {
		case 'b': {
			int8_t arg = va_arg(args, int8_t);
			ipc_cmd_add_arg(&cmd, cmdlen, &arg, 1);
			break;
		}
		case 'w': {
			int16_t arg = htons(va_arg(args, int16_t));
			ipc_cmd_add_arg(&cmd, cmdlen, (void*)&arg, 2);
			break;
		}
		case 'W': {
			int32_t arg = htonl(va_arg(args, int32_t));
			ipc_cmd_add_arg(&cmd, cmdlen, (void*)&arg, 4);
			break;
		}
		case 's': {
			char* arg = va_arg(args, char*);
			ipc_cmd_add_arg(&cmd, cmdlen, (void*)arg, strlen(arg));
			break;
		}
		case 'x': { //add binary blob, requires a second length argument
			char* arg = va_arg(args, char*);
			uint32_t arglen = va_arg(args, uint32_t);
			ipc_cmd_add_arg(&cmd, cmdlen, (void*)arg, arglen);
			break;
		}
		}

		fmtp++;
	}

	return cmd;
}

int ipc_cmd_set(char** buf, int* buflen, IPC_COMMAND_CODE code) {
	char* t = (char*)realloc(*buf, 4);
	if (!t) return -1;

	*buf = t;
	store_ns(*buf, code);
	store_ns(*buf + 2, 0);
	*buflen = 4;

	return 0;
}

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

int ipc_cmd_num_args(const char* buf, int buflen) {
	if (buflen < 4) return -2;
	return read_ns(buf + 2);
}

IPC_COMMAND_CODE ipc_cmd_get(const char* buf, int buflen) {
	if (buflen < 2) return IPC_CMDS_INVALID;
	return read_ns(buf);
}

int ipc_cmd_get_arg(const char* buf, int buflen, char** argbuf, int* argbuflen, int argidx, int addzero) {
	const char* p = buf + 4;
	int curridx = 0;
	uint32_t currarglen;

	if (argidx >= read_ns(buf + 2)) return -2;

	while (1) {
		currarglen = read_nl(p);

		if (curridx == argidx) break;

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

int ipc_cmd_get_short_arg(const char* buf, int buflen, int argidx, int16_t* out) {
	char* argbuf = 0;
	int argbuflen = 0;

	int rv = ipc_cmd_get_arg(buf, buflen, &argbuf, &argbuflen, argidx, 0);
	if (rv < 0) return rv;

	*out = read_ns(argbuf);
	free(argbuf);
	return 0;
}

int ipc_cmd_get_string_arg(const char* buf, int buflen, int argidx, char** out) {
	char* argbuf = 0;
	int argbuflen = 0;

	int rv = ipc_cmd_get_arg(buf, buflen, &argbuf, &argbuflen, argidx, 1);
	if (rv < 0) return rv;

	*out = (char*)malloc(argbuflen);
	memcpy(*out, argbuf, argbuflen);
	free(argbuf);
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
