cmake_minimum_required(VERSION 2.6)
project(print3d)

set(CMAKE_INSTALL_PREFIX /)

link_directories(${CMAKE_CURRENT_SOURCE_DIR})
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../drivers)

set(SOURCES ${SOURCES} main.cpp Logger.cpp Serial.cpp Server.cpp)
set(HEADERS ${HEADERS} Logger.h Serial.h Server.h)

add_executable(print3d ${SOURCES} ${HEADERS})
