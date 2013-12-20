/*
 * This file is part of the Doodle3D project (http://doodle3d.com).
 *
 * Copyright (c) 2013, Doodle3D
 * This software is licensed under the terms of the GNU GPL v2 or later.
 * See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.
 */

/**
 * This file contains compatibility definitions for Lua 5.1. At the time of
 * writing, OpenWrt does not include Lua 5.2, therefore we provide 5.2 specific
 * functions in a form supported by 5.1 where necessary.
 * Code has been copied from http://lua-users.org/wiki/CompatibilityWithLuaFive
 * and from the 5.2 version of lauxlib.h.
 */
#ifndef LUA_COMPAT_H_SEEN
#define LUA_COMPAT_H_SEEN

#ifdef __cplusplus
	extern "C" {
#endif

#include "lua.h"
#include "lauxlib.h"


#if !defined LUA_VERSION_NUM
# error "It looks like your Lua version is older than 5.1 - this is unsupported."
# define luaL_Reg luaL_reg /* first thing to define in case we would like to support 5.0 anyway... */
#endif

#if !defined LUA_VERSION_NUM || LUA_VERSION_NUM==501
//#warning "Lua 5.1 compatibility definitions enabled"

/* Adapted from Lua 5.2.0 */
static void luaL_setfuncs (lua_State *L, const luaL_Reg *l, int nup) {
  luaL_checkstack(L, nup, "too many upvalues");
  for (; l->name != NULL; l++) {  /* fill the table with given functions */
    int i;
    for (i = 0; i < nup; i++)  /* copy upvalues to the top */
      lua_pushvalue(L, -nup);
    lua_pushstring(L, l->name);
    lua_pushcclosure(L, l->func, nup);  /* closure with those upvalues */
    lua_settable(L, -(nup + 3));
  }
  lua_pop(L, nup);  /* remove upvalues */
}

static void luaL_setmetatable (lua_State *L, const char *tname) {
  luaL_getmetatable(L, tname);
  lua_setmetatable(L, -2);
}

#define luaL_newlibtable(L,l)   \
  lua_createtable(L, 0, sizeof(l)/sizeof((l)[0]) - 1)

#define luaL_newlib(L,l)   (luaL_newlibtable(L,l), luaL_setfuncs(L,l,0))

#endif /* !defined LUA_VERSION_NUM || LUA_VERSION_NUM==501 */


#ifdef __cplusplus
	} //extern "C" {
#endif

#endif /* ! LUA_COMPAT_H_SEEN */
