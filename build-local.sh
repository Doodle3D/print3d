#!/bin/sh

MAKE=make
CMAKE=cmake
CCMAKE=ccmake
SRC_DIR=../../src
BUILD_TARGET=Debug
#CMAKE_FLAGS="-G \"Unix Makefiles\""
CMAKE_FLAGS=("-G" "Unix Makefiles")
INTERACTIVE_MODE=no

for arg in $@; do
	case $arg in
	-h)
		echo "By default, the script builds for target '$BUILD_TARGET', specify one of 'Debug', 'Release', 'RelWithDebInfo' or 'MinSizeRel' to change this. Furthermore, the following arguments are valid:"
		echo "\t-h\tshow this help"
		echo "\t-i\trun interactive cmake configurator ($CCMAKE)"
		exit
		;;
	-i)
		INTERACTIVE_MODE=yes
		;;
	*)
		if [ $arg == "Debug" -o $arg == "Release" -o $arg == "RelWithDebInfo" -o $arg == "MinSizeRel" ]; then
			BUILD_TARGET=$arg
		else
			echo "$0: unknown argument: '$arg'"
			exit 1
		fi
		;;
	esac
done

CMAKE_FLAGS+=("-DCMAKE_BUILD_TYPE:STRING=$BUILD_TARGET")

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

$MAKE
