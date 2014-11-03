cmake_minimum_required(VERSION 2.6)
project(unittests)

enable_testing()

include_directories(include)

add_executable(t_gcodebuffer server/t_GCodeBuffer.cpp)
target_link_libraries(t_gcodebuffer drivers)
add_test(server_gcodebuffer t_gcodebuffer)

add_custom_target(
	unittest
	COMMAND ctest
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	DEPENDS t_gcodebuffer
)
