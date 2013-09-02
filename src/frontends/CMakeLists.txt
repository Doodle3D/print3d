cmake_minimum_required(VERSION 2.6)
project(print3d)

option(LUA_FRONTEND "build Lua frontend" ON)

link_directories(${CMAKE_CURRENT_SOURCE_DIR})
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../drivers)

#set(SOURCES ${SOURCES} communicator.c)
#set(HEADERS ${HEADERS} communicator.h)

add_subdirectory(lua)
