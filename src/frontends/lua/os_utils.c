/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013-2014, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

#define LUA_LIB

#include <unistd.h>
#include <sys/time.h>
#include "lua_compat.h"


/*********************
 * UTILITY FUNCTIONS *
 * *******************
 *
 * For microsecond-precision time and obtaining PID.
 * Note: this should really be in the firmware repository but that would require
 * a C(++) dependency on that package as well as duplicating lua_compat.h.
 */

/*
 * --- Get the current time with high precision.
 * -- The returned values are as returned by the C function `gettimeofday()`,
 * -- so it is in local time since epoch (1-1-1970).
 * --
 * -- @return Seconds since epoch.
 * -- @return Microseconds since epoch.
 * -- @name getTime
 */
static int l_getTime(lua_State *L) {
	struct timeval now;

	gettimeofday(&now, NULL);

	lua_pushinteger(L, now.tv_sec);
	lua_pushinteger(L, now.tv_usec);
	return 2;
}

/*
 * --- Get both the time elapsed since the given time in milliseconds.
 * -- Additionally, the current time is returned in the same fashion as getTime,
 * -- in order to allow for easy repeated measuring of elapsed time.
 * -- If less than two arguments are passed, nil+msg is returned.
 * --
 * -- @see getTime
 * -- @param sec Seconds since epoch to measure elapsed time from.
 * -- @param usec Microseconds since epoch to measure elapsed time from.
 * -- @return The number of milliseconds elapsed since the given time.
 * -- @return Current seconds since epoch.
 * -- @return Current microseconds since epoch.
 * -- @name getElapsedTime
 */
static int l_getElapsedTime(lua_State *L) {
	struct timeval begin, now;

	if (lua_gettop(L) >= 2) {
		begin.tv_sec = luaL_checkinteger(L, 1);
		begin.tv_usec = luaL_checkinteger(L, 2);
	} else {
		lua_pushnil(L);
		lua_pushstring(L, "two arguments required: seconds and microseconds");
		return 2;
	}

	gettimeofday(&now, NULL);
	//usec must be cast to int because if it has rolled over, the subtraction can be negative
	int msec_diff = (now.tv_sec - begin.tv_sec) * 1000 + ((int)now.tv_usec - (int)begin.tv_usec) / 1000;

	lua_pushinteger(L, msec_diff);
	lua_pushinteger(L, now.tv_sec);
	lua_pushinteger(L, now.tv_usec);
	return 3;
}

/*
 * --- Return the process ID of the current process.
 * --
 * -- @return The PID as returned by the C function `getPID()`.
 * -- @name getPID
 */
static int l_getPID(lua_State *L) {
	lua_pushinteger(L, getpid());
	return 1;
}


/**************************
 * REGISTRATION FUNCTIONS *
 **************************/

static const struct luaL_Reg os_utils_f[] = {
		{"getTime", l_getTime},
		{"getElapsedTime", l_getElapsedTime},
		{"getPID", l_getPID},
		{NULL, NULL}
};

int luaopen_os_utils(lua_State *L) {
	luaL_newlib(L, os_utils_f);
	return 1;
}
