/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#define LUA_LIB

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include "lua_compat.h"
#include "../../logger.h"
#include "../communicator.h"


#define LIB_MOD_NAME "print3d"
#define LIB_META_NAME LIB_MOD_NAME ".meta"
#define PRINTER_META_KEY LIB_META_NAME


static const ELOG_LEVEL logLevel = LLVL_VERBOSE;

struct printerData_s {
		char *deviceId;
};


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
int initContext(lua_State* L) {
	struct printerData_s* ctx = getContext(L);

	if (!ctx) {
		lua_pushnil(L);
		lua_pushstring(L, "could not allocate printer context");
		return -1;
	}

	if (log_open_stream(stderr, logLevel) < 0) {
		lua_pushnil(L);
		lua_pushfstring(L, "could not open log stream (%s)", strerror(errno));
		return -1;
	}

	if (comm_openSocketForDeviceId(ctx->deviceId) < 0) {
		lua_pushnil(L);
		lua_pushfstring(L, "could not open IPC socket (%s)", strerror(errno));
		return -1;
	}

	return 0;
}


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

	ipc_free_device_list(devlist);
	luaL_setmetatable(L, LIB_META_NAME);
	return 1;
}


static int l_getTemperatures(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack

	int16_t tH, tHt, tB, tBt;
	int rv = comm_getTemperature(&tH, IPC_TEMP_HOTEND);
	if (rv >= 0) rv = comm_getTemperature(&tHt, IPC_TEMP_HOTEND_TGT);
	if (rv >= 0) rv = comm_getTemperature(&tB, IPC_TEMP_BED);
	if (rv >= 0) rv = comm_getTemperature(&tBt, IPC_TEMP_BED_TGT);
	comm_closeSocket();

	if (rv > -1) {
		lua_newtable(L);
		lua_pushnumber(L, tH); lua_setfield(L, -2, "hotend");
		lua_pushnumber(L, tHt); lua_setfield(L, -2, "hotend_target");
		lua_pushnumber(L, tB); lua_setfield(L, -2, "bed");
		lua_pushnumber(L, tBt); lua_setfield(L, -2, "bed_target");
		return 1;
	} else {
		lua_pushnil(L);
		lua_pushfstring(L, "error comunicating with server (%s)", comm_getError());
		return 2;
	}
}


static int l_clearGcode(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack
	int rv = comm_clearGcode();
	comm_closeSocket();

	if (rv < -1) {
		lua_pushnil(L);
		lua_pushfstring(L, "error comunicating with server (%s)", comm_getError());
		return 2;
	}

	lua_pushboolean(L, 1);
	return 1;
}


static int l_appendGcode(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack
	int rv = comm_sendGcodeData(luaL_checkstring(L, 2));
	comm_closeSocket();

	if (rv < -1) {
		lua_pushnil(L);
		lua_pushfstring(L, "error comunicating with server (%s)", comm_getError());
		return 2;
	}

	lua_pushboolean(L, 1);
	return 1;
}


static int l_appendFileContents(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack
	int rv = comm_sendGcodeFile(luaL_checkstring(L, 2));
	comm_closeSocket();

	if (rv < -1) {
		lua_pushnil(L);
		lua_pushfstring(L, "error comunicating with server (%s)", comm_getError());
		return 2;
	}

	lua_pushboolean(L, 1);
	return 1;
}


static int l_startPrint(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack
	int rv = comm_startPrintGcode();
	comm_closeSocket();

	if (rv < -1) {
		lua_pushnil(L);
		lua_pushfstring(L, "error comunicating with server (%s)", comm_getError());
		return 2;
	}

	lua_pushboolean(L, 1);
	return 1;
}


static int l_stopPrint(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack

	const char *endCode = 0;
	if (lua_gettop(L) > 1) {
		endCode = luaL_checkstring(L, 2);
	}

	int rv = comm_stopPrintGcode(endCode);
	comm_closeSocket();

	if (rv < -1) {
		lua_pushnil(L);
		lua_pushfstring(L, "error comunicating with server (%s)", comm_getError());
		return 2;
	}

	lua_pushboolean(L, 1);
	return 1;
}


static int l_heatup(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack

	int rv = comm_heatup(luaL_checkinteger(L, 2));
	comm_closeSocket();

	if (rv < -1) {
		lua_pushnil(L);
		lua_pushfstring(L, "error comunicating with server (%s)", comm_getError());
		return 2;
	}

	lua_pushboolean(L, 1);
	return 1;
}


//returns currentLine+bufferedLines+totalLines or nil+errmsg
static int l_getProgress(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack

	int32_t currentLine, bufferedLines, totalLines;
	int rv = comm_getProgress(&currentLine, &bufferedLines, &totalLines);
	comm_closeSocket();

	if (rv > -1) {
		lua_pushnumber(L, currentLine);
		lua_pushnumber(L, bufferedLines);
		lua_pushnumber(L, totalLines);
		return 3;
	} else {
		lua_pushnil(L);
		lua_pushfstring(L, "error comunicating with server (%s)", comm_getError());
		return 2;
	}
}


static int l_getState(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack

	char *state;
	int rv = comm_getState(&state);
	comm_closeSocket();

	if (rv > -1) {
		lua_pushstring(L, state);
		return 1;
	} else {
		lua_pushnil(L);
		lua_pushfstring(L, "error comunicating with server (%s)", comm_getError());
		return 2;
	}
}



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
