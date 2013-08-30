cmake_minimum_required(VERSION 2.6)
project(print3d)

#add_definitions(-Wall -Werror)
add_definitions(-Wall)

#option(PRINT3D_DEBUG "debugging support" OFF)

#configure_file( ${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_SOURCE_DIR}/config.h )

include_directories(${CMAKE_CURRENT_SOURCE_DIR})

add_subdirectory(server)
add_subdirectory(drivers)
add_subdirectory(frontends)
