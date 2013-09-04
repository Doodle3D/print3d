cmake_minimum_required(VERSION 2.6)
project(print3d)

option(LUA_FRONTEND "build Lua frontend" ON)

set(SOURCES communicator.c logger.c)
set(HEADERS communicator.h logger.h)

add_library(fe_common ${SOURCES} ${HEADERS})
target_link_libraries(fe_common ipc_shared)

add_subdirectory(lua)
