cmake_minimum_required(VERSION 2.6)
project(print3d)

add_definitions(--std=gnu99)

add_custom_target(
	gpx
	COMMAND make -f ${CMAKE_CURRENT_SOURCE_DIR}/GPX.git/Makefile
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
