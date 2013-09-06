cmake_minimum_required(VERSION 2.6)
project(print3d)

#add_definitions(-Wall -Werror)
add_definitions(-Wall)

option(GENERATE_DOCS "generate documentation" OFF)
#option(PRINT3D_DEBUG "debugging support" OFF)

#configure_file( ${CMAKE_SOURCE_DIR}/config.h.in ${CMAKE_SOURCE_DIR}/config.h )

add_subdirectory(server)
add_subdirectory(drivers)
add_subdirectory(frontends)

add_library(logger logger.c logger.h)

add_library(utils utils.c utils.h)

add_library(ipc_shared ipc_shared.c ipc_shared.h)
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