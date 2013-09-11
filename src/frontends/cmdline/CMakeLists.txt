cmake_minimum_required(VERSION 2.6)
project(print3d)

set(SOURCES fe_cmdline.c fe_cmdline.h actions.c)
set(HEADERS)

if(CMDLINE_FRONTEND)
	set(EXEC_NAME p3d)
	add_executable(${EXEC_NAME} ${SOURCES} ${HEADERS})
	target_link_libraries(${EXEC_NAME} fe_common)
	install(TARGETS ${EXEC_NAME} RUNTIME DESTINATION bin)
endif(CMDLINE_FRONTEND)
