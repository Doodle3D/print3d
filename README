##PRINT3D README

This package provides access to multiple types of 3D printers. It aims to be usable at least on
OSX and Openwrt (linux) and currently provides a command-line frontend and a lua binding.

C++ unit tests use Fructose 1.2.0 (http://fructose.sourceforge.net/).


## HOW TO BUILD

This package is intended to be built either for embedded devices using the Openwrt build system or natively using CMake.
Building natively is done using the script `build-local.sh` (or manually). The included Eclipse CDT project also has the necessary targets configured.

Requirements to build and run the code natively are: cmake (more to come).

## Startup scripts
- [print3d_init](https://github.com/Doodle3D/print3d/blob/master/src/script/print3d_init). A init script that starts the print3d-manager as a deamon. 
- [print3d-manager](https://github.com/Doodle3D/print3d/blob/master/src/script/print3d-manager.sh). Uses inotifyd to start `print3d-new-device` when a new device is connected. 
- [print3d-new-device](https://github.com/Doodle3D/print3d/blob/master/src/script/print3d-new-device.sh). Figures out whether to start print3d in a seemingly crude way. If appropriate it tries to start `print3d-runner`
- [print3d-runner.sh](https://github.com/Doodle3D/print3d/blob/master/src/script/print3d-runner.sh). Starts the print3d deamon. 
