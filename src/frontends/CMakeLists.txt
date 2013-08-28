cmake_minimum_required(VERSION 2.6)
project(print3d)

#build frontend-global code here
#define options to enable/disable specific frontends

option(LUA_FRONTEND "build Lua frontend" ON)

add_subdirectory(lua)
