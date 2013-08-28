cmake_minimum_required(VERSION 2.6)
project(print3d)

add_definitions(-Wall -Werror)

#option(UCI_DEBUG "debugging support" OFF)

#configure_file( ${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_SOURCE_DIR}/config.h )

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(server)
add_subdirectory(drivers)
add_subdirectory(frontends)

#add_executable (print3d print3d.cpp Serial.cpp Logger.cpp)
