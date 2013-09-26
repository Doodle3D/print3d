#include <inttypes.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
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
		{ IPC_CMDQ_TEST, "test", "*", "*" }, //NOTE: echoes anything it receives?
		{ IPC_CMDQ_GET_TEMPERATURE, "getTemperature", "w", "w" }, //NOTE: accepts 'which' selector
		{ IPC_CMDQ_GCODE_CLEAR, "gcodeClear", "", "" },
		{ IPC_CMDQ_GCODE_APPEND, "gcodeAppend", "*", "" }, //NOTE: should accept "s" as well as "x"
		{ IPC_CMDQ_GCODE_APPEND_FILE, "gcodeAppendFile", "s", "" }, //NOTE: accepts absolute path to file
		{ IPC_CMDQ_GCODE_STARTPRINT, "gcodeStartPrint", "", "" },
		{ IPC_CMDQ_GCODE_STOPPRINT, "gcodeStopPrint", "*", "" }, //NOTE: accepts optional "s" or "x" with end g-code
		{ IPC_CMDQ_HEATUP, "heatup", "w", "" }, //NOTE: accepts heatup target temperature
		{ IPC_CMDQ_GET_PROGRESS, "getProgress", "", "ww" }, //NOTE: returns currentLine and numLines
		{ IPC_CMDQ_GET_STATE, "getState", "", "s" },

		/* response commands send by server */
		{ IPC_CMDR_OK, "ok", "*", NULL },
		{ IPC_CMDR_ERROR, "error", "is", NULL },
		{ IPC_CMDR_NOT_IMPLEMENTED, "not_implemented", "", NULL },

		{ 0, NULL, NULL, NULL } /* sentinel */
};

const char* IPC_SOCKET_PATH_PREFIX = "/tmp/print3d-";


static const int STRINGIFY_MAX_TEXT_DISPLAY_CHARS = 15;

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
	const ipc_cmd_name_s* description = findCommandDescription(code);
	const char* fmtp = format;

	if (!fmtp) {
		log_message(LLVL_BULK, "[IPC] construct_cmd: NULL format specifier, assuming no arguments");
		fmtp = "";
	}

	if (!equal(description->arg_fmt, "*") && !equal(description->arg_fmt, fmtp)) {
		log_message(LLVL_WARNING, "[IPC] construct_cmd: given message format '%s' not equal to predefined format '%s'", fmtp, description->arg_fmt);
	}

	char* cmd = 0;
	*cmdlen = 0;

	if (ipc_cmd_set(&cmd, cmdlen, code) < 0) {
		return NULL;
	}

	while(*fmtp) {
		char argtype = *fmtp;

		switch(argtype) {
		case 'w': {
			int16_t arg = htons(va_arg(args, int));
			ipc_cmd_add_arg(&cmd, cmdlen, (void*)&arg, 2);
			break;
		}
		case 'W': {
			int32_t arg = htonl(va_arg(args, int));
			ipc_cmd_add_arg(&cmd, cmdlen, (void*)&arg, 4);
			break;
		}
		case 's': {
			char* arg = va_arg(args, char*);
			ipc_cmd_add_arg(&cmd, cmdlen, (void*)arg, strlen(arg));
			break;
		}
		case 'x': { //add binary blob, requires a second argument specifying the length
			char* arg = va_arg(args, char*);
			uint32_t arglen = va_arg(args, int);
			ipc_cmd_add_arg(&cmd, cmdlen, (void*)arg, arglen);
			break;
		}
		default:
			log_message(LLVL_WARNING, "[IPC] illegal format specifier in construct_cmd (%c)", argtype);
			//TODO: free unfinished command and return, instead of ignoring this
			break;
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

int ipc_cmd_add_arg(char** buf, int* buflen, const void* arg, uint32_t arglen) {
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

int ipc_cmd_get_arg_len(const char *buf, int buflen, int argidx) {
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

	return currarglen;
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

int ipc_cmd_get_long_arg(const char* buf, int buflen, int argidx, int32_t* out) {
	char* argbuf = 0;
	int argbuflen = 0;

	int rv = ipc_cmd_get_arg(buf, buflen, &argbuf, &argbuflen, argidx, 0);
	if (rv < 0) return rv;

	*out = read_nl(argbuf);
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
	//NOTE: t might be NULL if we asked to reallocate to 0 bytes (happens on openwrt but not on osx)
	if (t == NULL && (*buflen - l) > 0) return -1;

	*buf = t;
	*buflen -= l;

	return 1;
}

int ipc_stringify_cmd(const char *buf, int buflen, const char *fmt, char **outbuf) {
	const ipc_cmd_name_s *cmd = &IPC_COMMANDS[ipc_cmd_get(buf, buflen)];
	int outlen = strlen(cmd->name) + 3; //"[" + name + "]" + nul

	*outbuf = (char*)realloc(*outbuf, outlen);
	if (*outbuf == NULL) return -1;
	*outbuf = "[";
	strcat(*outbuf, cmd->name);

	int num_args = ipc_cmd_num_args(buf, buflen);
	for (int i = 0; i < num_args; i++) {
		char type = fmt[i];
		if (type == '\0') {
			const char *text = ":!...";
			int args_left = num_args - 1 - i;
			outlen += strlen(text) + number_length(args_left);
			*outbuf = (char*)realloc(*outbuf, outlen);
			if (!*outbuf) return -1;
			strcat(*outbuf, text);
			number_to_string(args_left, *outbuf + strlen(*outbuf));
			break;
		}

		int arglen = ipc_cmd_get_arg_len(buf, buflen, i);
		int expected = (type == 'w') ? 2 : (type == 'W') ? 4 : 0;

		if (expected > 0 && arglen != expected) {
			//TODO: add mismatch text and stop
		}

		switch (type) {
			case 'w': {
				int16_t v;
				ipc_cmd_get_short_arg(buf, buflen, i, &v);
				outlen += number_length(v) + 2;
				*outbuf = (char*)realloc(*outbuf, outlen);
				if (!*outbuf) return -1;
				strcat(*outbuf, ":s");
				number_to_string(v, *outbuf + strlen(*outbuf));
				break;
			}

			case 'W': {
				int32_t v;
				ipc_cmd_get_long_arg(buf, buflen, i, &v);
				outlen += number_length(v) + 2;
				*outbuf = (char*)realloc(*outbuf, outlen);
				if (!*outbuf) return -1;
				strcat(*outbuf, ":l");
				number_to_string(v, *outbuf + strlen(*outbuf));
				break;
			}

			case 's': /* fall-through */
			case 'x': {
				char *s = 0;
				ipc_cmd_get_string_arg(buf, buflen, i, &s);
				int slen = strlen(s);

				if (slen <= STRINGIFY_MAX_TEXT_DISPLAY_CHARS) {
					outlen += slen + 2;
					*outbuf = (char*)realloc(*outbuf, outlen);
					if (!*outbuf) return -1;
					strcat(*outbuf, ":\"");
					strcat(*outbuf, s);
				} else {
					outlen += number_length(slen) + 2;
					*outbuf = (char*)realloc(*outbuf, outlen);
					if (!*outbuf) return -1;
					strcat(*outbuf, ":#");
					number_to_string(slen, *outbuf + strlen(*outbuf));
				}
				free(s);
				break;
			}
		}
	}
	strcat(*outbuf, "]");

	return 1;
}

#ifdef __cplusplus
} //extern "C"
#endif
