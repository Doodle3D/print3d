#define LUA_LIB
#include <math.h> /* TEMP for l_cos() */
#include "lua.h"
#include "lauxlib.h"

static int l_cos(lua_State *L) {
	double d = luaL_checknumber(L, 1);
	lua_pushnumber(L, cos(d));
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
		{"p3dcos", l_cos},
		{NULL, NULL}
};

int luaopen_print3d(lua_State *L) {
	luaL_newlib(L, print3d);
	return 1;
}
