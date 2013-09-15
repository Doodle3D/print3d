#define LUA_LIB

#include <errno.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "../../logger.h"
#include "../communicator.h"

#define LIB_MOD_NAME "print3d"
#define LIB_META_NAME LIB_MOD_NAME ".meta"
//#define PRINTER_META_KEY "Print3d.print3d"
#define PRINTER_META_KEY LIB_META_NAME


static const ELOG_LEVEL logLevel = LLVL_VERBOSE;

struct printerData_s {
		const char *deviceId;
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


//make all printer-specific calls also usable with the printer:call() syntax (see libuci-lua)
//write code such that printer IDs never have to be passed (always default to first (and probably only) printer found)

//TODO: implement:
//number getNumPrinters()
//table getPrinterList()
//table getPrinterDetails(id?)

static int l_lua_gc(lua_State *L) {
	struct printerData_s *ctx = getContext(L);
	if (ctx) {
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

static int l_getPrinter(lua_State *L) {
	size_t devLen;
	const char* dev = (lua_gettop(L) > 0) ? luaL_checklstring(L, 1, &devLen) : 0;

	if (!dev) lua_error(L);


	struct printerData_s **pd = lua_newuserdata(L, sizeof(struct printerData_s*));
	*pd = (struct printerData_s*)malloc(sizeof(struct printerData_s));

	(*pd)->deviceId = dev;

	luaL_setmetatable(L, LIB_META_NAME);
	return 1;
}


static int l_getTemperature(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack
	int16_t temperature;
	int rv = comm_getTemperature(&temperature);
	comm_closeSocket();

	if (rv > -1) {
		lua_pushnumber(L, temperature);
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

	return 0;
}

static int l_appendGcode(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack
	if (lua_gettop(L) == 0) {
		lua_pushnil(L);
		lua_pushstring(L, "expected argument containing gcode");
	}

	const char *data = luaL_checkstring(L, 1);
	int rv = comm_sendGcodeData(data);
	comm_closeSocket();

	if (rv < -1) {
		lua_pushnil(L);
		lua_pushfstring(L, "error comunicating with server (%s)", comm_getError());
		return 2;
	}

	return 0;
}

static int l_appendFileContents(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack
	if (lua_gettop(L) == 0) {
		lua_pushnil(L);
		lua_pushstring(L, "expected argument containing filename");
	}

	const char *file = luaL_checkstring(L, 1);
	int rv = comm_sendGcodeFile(file);
	comm_closeSocket();

	if (rv < -1) {
		lua_pushnil(L);
		lua_pushfstring(L, "error comunicating with server (%s)", comm_getError());
		return 2;
	}

	return 0;
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

	return 0;
}

static int l_stopPrint(lua_State *L) {
	if (initContext(L) != 0) return 2; //nil+msg already on stack
	int rv = comm_stopPrintGcode();
	comm_closeSocket();

	if (rv < -1) {
		lua_pushnil(L);
		lua_pushfstring(L, "error comunicating with server (%s)", comm_getError());
		return 2;
	}

	return 0;
}

//static int l_isPrinting(lua_State *L) {
//	return 0;
//}
//static int l_getProgress(lua_State *L) {
//	return 0;
//}



static const struct luaL_Reg print3d_f[] = {
		{"getPrinter", l_getPrinter},
};

static const struct luaL_Reg print3d_m[] = {
		{"__gc", l_lua_gc},
		{"__tostring", l_lua_tostring},
		{"getTemperature", l_getTemperature},
		{"clearGcode", l_clearGcode},
		{"appendGcode", l_appendGcode},
		{"appendFileContents", l_appendFileContents},
		{"startPrint", l_startPrint},
		{"stopPrint", l_stopPrint},
		//{"isPrinting", l_isPrinting},
		//{"getProgress", l_getProgress},
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
