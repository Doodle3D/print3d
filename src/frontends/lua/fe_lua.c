#define LUA_LIB

#include <string.h>
#include "lua.h"
#include "lauxlib.h"
#include "../communicator.h"
#include "../logger.h"

static int l_getTemperature(lua_State *L) {
	size_t devLen;
	const char* dev = luaL_checklstring(L, 1, &devLen);
	int rv = getTemperature((devLen > 0) ? dev : "-");

	if (rv > INT_MIN) {
		lua_pushnumber(L, rv);
		return 1;
	} else {
		const char* errMsg = "error comunicating with server (TODO: return real C error)"; //TODO: return real C error
		lua_pushnil(L);
		lua_pushlstring(L, errMsg, strlen(errMsg));
		return 2;
	}
}

static int l_setTemperatureCheckInterval(lua_State *L) {
	//double d = luaL_checknumber(L, 1);
	//setTemperatureCheckInterval(d);
	lua_pushboolean(L, 1);
	return 1;
}

//make all printer-specific calls also usable with the printer:call() syntax (see libuci-lua)
//write code such that printer IDs never have to be passed (always default to first (and probably only) printer found)

//number getNumPrinters()
//table getPrinterIDs()
//table getPrinterDetails(id?)

//number getGcodeBufferSize(id?)
//void clearGcodeBuffer(id?)
//--bool appendToGcodeBuffer(id?)
//bool printGcodeBuffer(id?)
//bool isPrintingGcodeBuffer(id?) //--weglaten en getPos <0 laten teruggeven bij niet printen?
//number getGcodeBufferPrintPosition(id?, bool relative) //--absoluut, of /size -> 0-1 bereik
//bool stopGcodeBufferPrinting(id?)

//bool sendGcode(id?, data)

//number getTemperature(id?)
//...

static const struct luaL_Reg print3d[] = {
		{"getTemperature", l_getTemperature},
		{"setTemperatureCheckInterval", l_setTemperatureCheckInterval},
		{NULL, NULL}
};

int luaopen_print3d(lua_State *L) {
	luaL_newlib(L, print3d);
	return 1;
}
