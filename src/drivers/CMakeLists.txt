cmake_minimum_required(VERSION 2.6)
project(print3d)

set(SOURCES ${SOURCES} AbstractDriver.cpp DriverFactory.cpp UltimakerDriver.cpp Serial.cpp)
set(HEADERS ${HEADERS} AbstractDriver.h DriverFactory.h UltimakerDriver.h Serial.h)

add_library(drivers ${SOURCES} ${HEADERS})
