/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#define LUA_LIB

#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "lua_compat.h"
#include "../../logger.h"
#include "../communicator.h"


/***********
 * KLUDGES *
 ***********/

/* from lauxlib.h, openwrt refuses to include that here */
#ifndef LUA_FILEHANDLE

# define LUA_FILEHANDLE "FILE*"

typedef struct luaL_Stream {
  FILE *f;  /* stream (NULL for incompletely created streams) */
  lua_CFunction closef;  /* to close stream (NULL for closed streams) */
} luaL_Stream;

#endif


/********************
 * DEFINITIONS ETC. *
 ********************/

#define LIB_MOD_NAME "print3d"
#define LIB_META_NAME LIB_MOD_NAME ".meta"
#define PRINTER_META_KEY LIB_META_NAME


//names of metadata fields for appendGcode
static const char *GCODE_MD_SEQ_NUMBER = "seq_number";
static const char *GCODE_MD_SEQ_TOTAL = "seq_total";
static const char *GCODE_MD_SOURCE = "source";

struct printerData_s {
		char *deviceId;
		int initialized;
		ELOG_LEVEL logLevel;
		FILE *logStream;
};


/********************
 * HELPER FUNCTIONS *
 ********************/

#if 0
//from: http://www.lua.org/pil/24.2.3.html
static void stackDump (lua_State *L) {
	int i, top = lua_gettop(L);
	for (i = 1; i <= top; i++) {  /* repeat for each level */
		int t = lua_type(L, i);
		switch (t) {
		case LUA_TSTRING:  /* strings */
			printf("`%s'", lua_tostring(L, i));
			break;
		case LUA_TBOOLEAN:  /* booleans */
			printf(lua_toboolean(L, i) ? "true" : "false");
			break;
		case LUA_TNUMBER:  /* numbers */
			printf("%g", lua_tonumber(L, i));
			break;
		default:  /* other values */
			printf("%s", lua_typename(L, t));
			break;
		}
		printf("  ");  /* put a separator */
	}
	printf("\n");  /* end the listing */
}
#endif

static struct printerData_s *getContext(lua_State *L) {
	struct printerData_s **ctx = NULL;
	if (lua_isuserdata(L, 1)) {
		ctx = luaL_checkudata(L, 1, LIB_META_NAME);
		if (!ctx || !*ctx) {
			luaL_error(L, "failed to get printer context");
			return NULL;
		}
	}
	return ctx ? *ctx : NULL;
}


//fetches context from stack, inits log and opens IPC socket. returns 0 on success or -1 on system error
//in the former case, nothing will be pushed on the stack, otherwise nil+errmsg will be pushed
static int initContext(lua_State* L) {
	struct printerData_s* ctx = getContext(L);

	if (!ctx) {
		lua_pushnil(L);
		lua_pushstring(L, "could not get printer context");
		return -1;
	}

	//if (ctx->initialized) return 0;

	if (!log_is_open()) {
		if (log_open_stream(ctx->logStream, ctx->logLevel) < 0) {
			lua_pushnil(L);
			lua_pushfstring(L, "could not open log stream (%s)", strerror(errno));
			return -1;
		}
	}

	if (!comm_is_socket_open()) {
		if (comm_openSocket(ctx->deviceId) < 0) {
			lua_pushnil(L);
			lua_pushfstring(L, "could not open IPC socket (%s)", strerror(errno));
			return -1;
		}
	}

	ctx->initialized = 1;

	return 0;
}

/*
 * Initializes the stack based on the result parameter, with:
 * true on success,
 * nil+msg on error (message obtained from comm_getError()) or
 * false+status on fail (status obtained from comm_getError()).
 */
static int initStackWithReturnStatus(lua_State *L, int result) {
	if (result == -1) {
		lua_pushnil(L);
		lua_pushfstring(L, "error communicating with server (%s)", comm_getError());
		return 2;
	} else if (result < -1) {
		lua_pushboolean(L, 0);
		lua_pushstring(L, comm_getError());
		return 2;
	}

	lua_pushboolean(L, 1);
	return 1;
}


/************************
 * 'META' API FUNCTIONS *
 ************************/

static int l_lua_gc(lua_State *L) {
	struct printerData_s *ctx = getContext(L);
	if (ctx) {
		free(ctx->deviceId);
		free(ctx);
		ctx = NULL; //FIXME: this is useless, but how do we indicate the structure has been freed? (or is this only called after the very last use?)
	}
	return 0;
}

int l_lua_tostring(lua_State *L) {
	struct printerData_s *ctx = getContext(L);
	lua_pushfstring(L, "3D-printer[%s]", ctx->deviceId);
	return 1;
}


/*****************
 * API FUNCTIONS *
 *****************/

static int l_getPrinter(lua_State *L) {
	size_t devLen;
	const char* dev = (lua_gettop(L) > 0) ? luaL_checklstring(L, 1, &devLen) : 0;
	char **devlist = NULL;

	if (!dev) {
		devlist = ipc_find_devices();

		if (!devlist) { //no list
			lua_pushnil(L);
			lua_pushstring(L, "device auto-detect: could not obtain device list");
			return 2;
		}

		if (devlist[0] == NULL) { //no devices
			lua_pushnil(L);
			lua_pushstring(L, "device auto-detect: no devices found");
			return 2;
		}

		if (devlist[1] != NULL) { //multiple devices
			lua_pushnil(L);
			lua_pushstring(L, "device auto-detect: multiple devices found");
			return 2;
		}

		dev = devlist[0];
	}


	struct printerData_s **pd = lua_newuserdata(L, sizeof(struct printerData_s*));
	*pd = (struct printerData_s*)malloc(sizeof(struct printerData_s));

	(*pd)->deviceId = strdup(dev);
	(*pd)->initialized = 0;
	(*pd)->logLevel = LLVL_VERBOSE;
	(*pd)->logStream = stderr;

	ipc_free_device_list(devlist);
	luaL_setmetatable(L, LIB_META_NAME);
	return 1;
}


int l_lua_getId(lua_State *L) {
	struct printerData_s *ctx = getContext(L);
	lua_pushstring(L, ctx->deviceId);
	return 1;
}

int l_lua_hasSocket(lua_State *L) {
	int rv, hasSocket;
	struct stat statbuf;
	struct printerData_s *ctx = getContext(L);
	char *socketPath = ipc_construct_socket_path(ctx->deviceId);

	rv = stat(socketPath, &statbuf);
	if (rv == -1 && errno != ENOENT) {
		lua_pushnil(L);
		lua_pushfstring(L, "could not stat ipc socket path '%s' (%s)", socketPath, strerror(errno));
		free(socketPath);
		return 2;
	}
	free(socketPath);

	hasSocket = (rv == 0 && S_ISSOCK(statbuf.st_mode) != 0) ? 1 : 0;

	lua_pushboolean(L, hasSocket);
	return 1;
}

static int l_getTemperatures(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack

	int16_t tH, tHt, tB, tBt, tHeating;
	int rv = comm_getTemperature(&tH, IPC_TEMP_HOTEND);
	if (rv >= 0) rv = comm_getTemperature(&tHt, IPC_TEMP_HOTEND_TGT);
	if (rv >= 0) rv = comm_getTemperature(&tB, IPC_TEMP_BED);
	if (rv >= 0) rv = comm_getTemperature(&tBt, IPC_TEMP_BED_TGT);
	if (rv >= 0) rv = comm_getTemperature(&tHeating, IPC_TEMP_HEATING);
	comm_closeSocket();

	//note: ugly hack ahead
	int numElems = initStackWithReturnStatus(L, rv); //this works fine for error/fail but not for success
	if (rv >= 0) {
		lua_pop(L, numElems); //so we pop the boolean to reset the stack, and push a table...
		lua_newtable(L);
		lua_pushnumber(L, tH); lua_setfield(L, -2, "hotend");
		lua_pushnumber(L, tHt); lua_setfield(L, -2, "hotend_target");
		lua_pushnumber(L, tB); lua_setfield(L, -2, "bed");
		lua_pushnumber(L, tBt); lua_setfield(L, -2, "bed_target");
		lua_pushnumber(L, tHeating); lua_setfield(L, -2, "heating");
		return 1;
	} else {
		return numElems; //otherwise return whatever the initStack function did...
	}
}

static int l_clearGcode(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack
	int rv = comm_clearGCode();
	comm_closeSocket();

	return initStackWithReturnStatus(L, rv);
}

//requires gcode as first argument, accepts table as second argument with keys (all optional):
//seq_number, seq_total, source (see GCodeBuffer for details)
static int l_appendGcode(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack
	int nargs = lua_gettop(L);
	ipc_gcode_metadata_s metadata = { -1, -1, NULL };

	if (nargs < 2) {
		lua_pushnil(L);
		lua_pushstring(L, "missing argument");
		return 2;
	}

	if (nargs > 2) {
		lua_getfield(L, 3, GCODE_MD_SEQ_NUMBER);
		if (!lua_isnoneornil(L, -1)) metadata.seq_number = luaL_checkinteger(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 3, GCODE_MD_SEQ_TOTAL);
		if (!lua_isnoneornil(L, -1)) metadata.seq_total = luaL_checkinteger(L, -1);
		lua_pop(L, 1);

		lua_getfield(L, 3, GCODE_MD_SOURCE);
		if (!lua_isnoneornil(L, -1)) metadata.source = luaL_checkstring(L, -1);
		lua_pop(L, 1);
	}

	int rv = comm_sendGCodeData(luaL_checkstring(L, 2), &metadata);
	comm_closeSocket();

	return initStackWithReturnStatus(L, rv);
}

static int l_appendFileContents(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack
	int rv = comm_sendGCodeFile(luaL_checkstring(L, 2));
	comm_closeSocket();

	return initStackWithReturnStatus(L, rv);
}

static int l_startPrint(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack
	int rv = comm_startPrintGCode();
	comm_closeSocket();

	return initStackWithReturnStatus(L, rv);
}

static int l_stopPrint(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack

	const char *endCode = 0;
	if (lua_gettop(L) > 1) {
		endCode = luaL_checkstring(L, 2);
	}

	int rv = comm_stopPrintGCode(endCode);
	comm_closeSocket();

	return initStackWithReturnStatus(L, rv);
}

static int l_heatup(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack

	int rv = comm_heatup(luaL_checkinteger(L, 2));
	comm_closeSocket();

	return initStackWithReturnStatus(L, rv);
}

//returns currentLine+bufferedLines+totalLines+bufferSize+maxBufferSize or nil+errmsg
static int l_getProgress(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack

	int32_t currentLine, bufferedLines, totalLines, bufferSize, maxBufferSize;
	int rv = comm_getProgress(&currentLine, &bufferedLines, &totalLines, &bufferSize, &maxBufferSize);
	comm_closeSocket();

	//to the south is another hack
	int numElems = initStackWithReturnStatus(L, rv);
	if (rv >= 0) {
		lua_pop(L, numElems);
		lua_pushnumber(L, currentLine);
		lua_pushnumber(L, bufferedLines);
		lua_pushnumber(L, totalLines);
		lua_pushnumber(L, bufferSize);
		lua_pushnumber(L, maxBufferSize);
		return 5;
	} else {
		return numElems;
	}
}

static int l_getState(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack

	char *state;
	int rv = comm_getState(&state);
	comm_closeSocket();

	//mind the hack below
	int numElems = initStackWithReturnStatus(L, rv);
	if (rv >= 0) {
		lua_pop(L, numElems);
		lua_pushstring(L, state);
		return 1;
	} else {
		return numElems;
	}
}

//does not do communication. it sets the log stream of the client side, not the server side
static int l_setLocalLogStream(lua_State *L) {
	struct printerData_s *ctx = getContext(L);

	if (lua_gettop(L) < 2) {
		lua_pushnil(L);
		lua_pushstring(L, "missing stream");
		return 2;
	}

	//Note: this basically mimics tofile() from Lua's liolib.c
	luaL_Stream* ls = (luaL_Stream*)luaL_checkudata(L, 2, LUA_FILEHANDLE);
	FILE *stream = ls->f;

	ctx->logStream = stream;

	lua_pushboolean(L, 1);
	return 1;
}

//does not do communication. it sets the log level of the client side, not the server side
static int l_setLocalLogLevel(lua_State *L) {
	struct printerData_s *ctx = getContext(L);

	if (lua_gettop(L) < 2) {
		lua_pushnil(L);
		lua_pushstring(L, "missing level name");
		return 2;
	}

	ELOG_LEVEL level = LLVL_INVALID;

	if (lua_isnumber(L, 2)) {
		level = luaL_checkinteger(L, 2);
		if (level < 1 || level > NUM_LOG_LEVELS) {
			lua_pushnil(L);
			lua_pushstring(L, "invalid log level number");
		}
	} else {
		const char *level_name = luaL_checkstring(L, 2);
		level = log_get_level_number(level_name);
		if (level == LLVL_INVALID) {
			lua_pushnil(L);
			lua_pushstring(L, "invalid log level name");
			return 2;
		}
	}

	ctx->logLevel = level;

	lua_pushboolean(L, 1);
	return 1;
}


/**************************
 * REGISTRATION FUNCTIONS *
 **************************/

static const struct luaL_Reg print3d_f[] = {
		{"getPrinter", l_getPrinter},
		{NULL, NULL}
};

static const struct luaL_Reg print3d_m[] = {
		{"__gc", l_lua_gc},
		{"__tostring", l_lua_tostring},

		{"getId", l_lua_getId},
		{"hasSocket", l_lua_hasSocket},
		{"getTemperatures", l_getTemperatures},
		{"clearGcode", l_clearGcode},
		{"appendGcode", l_appendGcode},
		{"appendFileContents", l_appendFileContents},
		{"startPrint", l_startPrint},
		{"stopPrint", l_stopPrint},
		{"heatup", l_heatup},
		{"getProgress", l_getProgress},
		{"getState", l_getState},
		{"setLocalLogStream", l_setLocalLogStream},
		{"setLocalLogLevel", l_setLocalLogLevel},

		{NULL, NULL}
};

int luaopen_print3d(lua_State *L) {
	luaL_newmetatable(L, PRINTER_META_KEY);

	lua_pushvalue(L, -1);  /* copy the metatable */
	lua_setfield(L, -2, "__index"); /* metatable.__index = metatable aka -3."__index" = -1 */
	luaL_setfuncs(L, print3d_m, 0);

	luaL_newlib(L, print3d_f);
	return 1;
}
