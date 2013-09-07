cmake_minimum_required(VERSION 2.6)
project(print3d)

set(SOURCES fe_cmdline.c fe_cmdline.h actions.c)
set(HEADERS)

if(CMDLINE_FRONTEND)
	add_executable(p3d ${SOURCES} ${HEADERS})
	target_link_libraries(p3d fe_common)
endif(CMDLINE_FRONTEND)
