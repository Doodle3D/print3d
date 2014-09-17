cmake_minimum_required(VERSION 2.6)
project(print3d)

if(APPLE)
	#create multi-architecture files on OSX (mainly for library loading in older 32 bit lua binaries)
	set(CMAKE_OSX_ARCHITECTURES x86_64;i386)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")

option(GENERATE_DOCS "generate documentation" OFF)
#option(PRINT3D_DEBUG "debugging support" OFF)
option(ENABLE_UCI "include UCI (OpenWrt only)" ON)

#configure_file( ${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_SOURCE_DIR}/config.h )

add_subdirectory(server)
add_subdirectory(drivers)
add_subdirectory(frontends)
add_subdirectory(aux)
if(NOT CMAKE_CROSSCOMPILING)
	add_subdirectory(unittest)
endif(NOT CMAKE_CROSSCOMPILING)

add_library(logger logger.c logger.h)
set_target_properties(logger PROPERTIES COMPILE_FLAGS "-fPIC")
target_link_libraries(logger ipc_shared)

add_library(settings settings.c settings.h)
set_target_properties(settings PROPERTIES COMPILE_FLAGS "-fPIC")

if(ENABLE_UCI)
	if(CMAKE_SYSTEM_NAME STREQUAL Linux AND CMAKE_CROSSCOMPILING)
		target_link_libraries(settings -luci)
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUCI_ENABLED=1")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUCI_ENABLED=1")
	else()
		message(SEND_ERROR "Cannot enable UCI since we're not compiling for OpenWrt")
		set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUCI_ENABLED=0")
		set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUCI_ENABLED=0")
	endif(CMAKE_SYSTEM_NAME STREQUAL Linux AND CMAKE_CROSSCOMPILING)
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DUCI_ENABLED=0")
	set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DUCI_ENABLED=0")
endif(ENABLE_UCI)

add_library(timer Timer.cpp Timer.h)

add_library(utils utils.c utils.h)
set_target_properties(utils PROPERTIES COMPILE_FLAGS "-fPIC")
target_link_libraries(utils m)

add_library(ipc_shared ipc_shared.c ipc_shared.h)
set_target_properties(ipc_shared PROPERTIES COMPILE_FLAGS "-fPIC -std=c99")
target_link_libraries(ipc_shared logger utils)

if(GENERATE_DOCS)
	# add a target to generate documentation with Doxygen (from: http://majewsky.wordpress.com/2010/08/14/tip-of-the-day-cmake-and-doxygen/)
	find_package(Doxygen)
	if(DOXYGEN_FOUND)
		configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile @ONLY)
		add_custom_target(doc ALL
		#add_custom_target(doc
			${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
			WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
			COMMENT "Generating documentation with Doxygen" VERBATIM
		)
	endif(DOXYGEN_FOUND)
endif(GENERATE_DOCS)
