cmake_minimum_required(VERSION 2.6)
project(print3d)

set(CMAKE_INSTALL_PREFIX /)

if(NOT LUA_CFLAGS)
	find_program(PKG_CONFIG pkg-config)
	if(PKG_CONFIG)
		execute_process(
			COMMAND pkg-config --silence-errors --cflags lua
			OUTPUT_VARIABLE LUA_CFLAGS
			OUTPUT_STRIP_TRAILING_WHITESPACE
		)
	endif()
endif()

add_definitions(--std=gnu99 -I.. ${LUA_CFLAGS})
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)

if(APPLE)
	include_directories(/opt/local/include)
	link_directories(/opt/local/lib)
endif()

if(APPLE)
	set(CMAKE_SHARED_MODULE_CREATE_C_FLAGS "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS} -undefined dynamic_lookup")
endif(APPLE)

if(NOT LUAPATH)
	execute_process(
		COMMAND  lua -e "for k in string.gmatch(package.cpath .. \";\", \"([^;]+)/..so;\") do if k:sub(1,1) == \"/\" then print(k) break end end"
		OUTPUT_VARIABLE LUAPATH
		RESULT_VARIABLE LUA_CHECK_RES
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)

	if(LUA_FRONTEND)
		if(NOT ${LUA_CHECK_RES} EQUAL 0 OR ${LUAPATH} EQUAL "")
			message(SEND_ERROR "Lua was not found on your system")
		endif()
	endif()
endif()

if(LUA_FRONTEND)
	add_library(print3d_lua MODULE fe_lua.c)
	set_target_properties(print3d_lua PROPERTIES
		OUTPUT_NAME print3d
		PREFIX ""
	)

	install(TARGETS print3d_lua
		LIBRARY DESTINATION ${LUAPATH}
	)
endif()
