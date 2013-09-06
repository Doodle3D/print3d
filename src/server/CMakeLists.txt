cmake_minimum_required(VERSION 2.6)
project(print3d)

set(SOURCES main.cpp Client.cpp CommandHandler.cpp Logger.cpp Server.cpp)
set(HEADERS Client.h CommandHandler.h Logger.h Server.h)

add_executable(print3d ${SOURCES} ${HEADERS})

target_link_libraries(print3d drivers ipc_shared utils)
