#!/bin/bash

MAKE=make
CMAKE=cmake
CCMAKE=ccmake
SRC_DIR=../../src
BUILD_TARGET=Debug
#CMAKE_FLAGS="-G \"Unix Makefiles\""
CMAKE_FLAGS=("-G" "Unix Makefiles")
INTERACTIVE_MODE=no
TEST_MODE=no

command cmake --version > /dev/null 2>&1
if [ $? -ne 0 ]; then
	echo "Error: this build script needs cmake."
	exit 1
fi

for arg in $@; do
	case $arg in
	-h)
		echo -e "By default, the script builds for target '$BUILD_TARGET', specify one of 'Debug', 'Release', 'RelWithDebInfo' or 'MinSizeRel' to change this."
		echo -e "The Lua front-end can be enabled/disabled by specifying 'Lua' or 'NoLua' correspondingly. Likewise, UCI supoprt can be enabled/disabled using 'Uci' or 'NoUci'."
		echo -e "Furthermore, the following arguments are valid:"
		echo -e "\t-h\tshow this help"
		echo -e "\t-i\trun interactive cmake configurator ($CCMAKE)"
		exit
		;;
	-i)
		INTERACTIVE_MODE=yes
		;;
	-t)
		TEST_MODE=yes
		;;
	*)
		if [ $arg == "Debug" -o $arg == "Release" -o $arg == "RelWithDebInfo" -o $arg == "MinSizeRel" ]; then
			BUILD_TARGET=$arg
		elif [ $arg == "Lua" ]; then
			LUA="ON"
		elif [ $arg == "NoLua" ]; then
			LUA="OFF"
		elif [ $arg == "Uci" ]; then
			UCI="ON"
		elif [ $arg == "NoUci" ]; then
			UCI="OFF"
		else
			echo "$0: unknown argument: '$arg', try using the '-h' argument"
			exit 1
		fi
		;;
	esac
done

CMAKE_FLAGS+=("-DCMAKE_BUILD_TYPE:STRING=$BUILD_TARGET")
if [ "x$LUA" != "x" ]; then CMAKE_FLAGS+=("-DLUA_FRONTEND=$LUA"); fi
if [ "x$UCI" != "x" ]; then CMAKE_FLAGS+=("-DENABLE_UCI=$UCI"); fi

if [ $INTERACTIVE_MODE == "yes" ]; then
	echo "Building for target '$BUILD_TARGET'... (interactive)"
else
	echo "Building for target '$BUILD_TARGET'..."
fi

mkdir -p build/$BUILD_TARGET
cd build/$BUILD_TARGET

echo "Running: '$CMAKE "${CMAKE_FLAGS[@]}" $SRC_DIR'"
$CMAKE "${CMAKE_FLAGS[@]}" $SRC_DIR

if [ $INTERACTIVE_MODE == "yes" -a $? -eq 0 ]; then
	$CCMAKE $SRC_DIR
fi

if [ ! $TEST_MODE == "yes" ]; then
	$MAKE
else
	$MAKE test
fi
