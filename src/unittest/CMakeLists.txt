cmake_minimum_required(VERSION 2.6)
project(unittests)

enable_testing()

include_directories(include)

add_executable(t_gcodebuffer server/t_GCodeBuffer.cpp)
target_link_libraries(t_gcodebuffer drivers)

add_executable(t_marlindriver server/t_MarlinDriver.cpp)
target_link_libraries(t_marlindriver drivers)

add_test(server_gcodebuffer t_gcodebuffer)
add_test(server_marlindriver t_marlindriver)

add_custom_target(
	test
	COMMAND ctest
	WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
	DEPENDS t_gcodebuffer
)
