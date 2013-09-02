cmake_minimum_required(VERSION 2.6)
project(print3d)

set(CMAKE_INSTALL_PREFIX /)

link_directories(${CMAKE_CURRENT_SOURCE_DIR})
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/../drivers)

set(SOURCES ${SOURCES} main.cpp Client.cpp CommandHandler.cpp Logger.cpp Printer.cpp Serial.cpp Server.cpp shared.c)
set(HEADERS ${HEADERS} Client.h CommandHandler.h Logger.h Printer.h Serial.h Server.h shared.h)

add_executable(print3d ${SOURCES} ${HEADERS})
