cmake_minimum_required(VERSION 2.6)
project(print3d)

set(SOURCES ${SOURCES} AbstractDriver.cpp DriverFactory.cpp GCodeBuffer.cpp MakerbotDriver.cpp MarlinDriver.cpp Serial.cpp)
set(HEADERS ${HEADERS} AbstractDriver.h DriverFactory.h GCodeBuffer.h MakerbotDriver.h S3GParser.h MarlinDriver.h Serial.h)

add_library(drivers ${SOURCES} ${HEADERS})

target_link_libraries(drivers gpx timer server settings)
