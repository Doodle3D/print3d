cmake_minimum_required(VERSION 2.6)
project(print3d)

add_definitions(--std=gnu99)

option(CMDLINE_FRONTEND "build command-line frontend" ON)
option(LUA_FRONTEND "build Lua frontend" ON)

add_library(fe_common communicator.c communicator.h)
set_target_properties(fe_common PROPERTIES COMPILE_FLAGS "-fPIC")
target_link_libraries(fe_common ipc_shared logger utils)

add_subdirectory(cmdline)
