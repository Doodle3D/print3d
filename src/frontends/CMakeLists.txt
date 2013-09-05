cmake_minimum_required(VERSION 2.6)
project(print3d)

option(LUA_FRONTEND "build Lua frontend" ON)

add_library(fe_common communicator.c communicator.h)
target_link_libraries(fe_common ipc_shared logger utils)

add_subdirectory(lua)
