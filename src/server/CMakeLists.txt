cmake_minimum_required(VERSION 2.6)
project(print3d)

set(SOURCES Client.cpp CommandHandler.cpp Logger.cpp Server.cpp)
set(HEADERS Client.h CommandHandler.h Logger.h Server.h)

add_library(server ${SOURCES} ${HEADERS})
target_link_libraries(server drivers ipc_shared utils)

add_executable(print3d main.cpp)
target_link_libraries(print3d server)

install(TARGETS print3d RUNTIME DESTINATION bin)
