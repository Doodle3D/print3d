## PRINT3D README

This package provides access to multiple types of 3D printers. It aims to be usable at least on
OSX and Openwrt (linux) and currently provides a command-line frontend and a lua binding.

C++ unit tests use Fructose 1.2.0 (http://fructose.sourceforge.net/).


## HOW TO BUILD

This package is intended to be built either for embedded devices using the Openwrt build system or natively using CMake.
Building natively is done using the script `build-local.sh` (or manually). The included Eclipse CDT project also has the necessary targets configured.

Requirements to build and run the code natively are: cmake (more to come).

## Startup scripts
The Print3D package includes shell scripts that start print3D when a printer is connected.
- `/etc.init.d/`[print3d](https://github.com/Doodle3D/print3d/blob/master/src/script/print3d_init). A init script that starts the print3d-manager as a deamon. 
- `/usr/libexec/`[print3d-manager](https://github.com/Doodle3D/print3d/blob/master/src/script/print3d-manager.sh). Uses inotifyd to start `print3d-new-device` when a new device is connected. 
- `/usr/libexec/`[print3d-new-device](https://github.com/Doodle3D/print3d/blob/master/src/script/print3d-new-device.sh). Figures out whether to start print3d in a seemingly crude way. If appropriate it tries to start `print3d-runner`
- `/usr/libexec/`[print3d-runner.sh](https://github.com/Doodle3D/print3d/blob/master/src/script/print3d-runner.sh). Starts the print3d deamon. 

## CMDline usage
- **Print3D** is a deamon that communicates with the printer. It sends the printer gcode line for line from it's buffer, it automatically checks the temperature etc. Preferably it's started when a printer is connected. 
- **P3D** is a example frontend / client to talk to the deamon. You can request the current temperature, request the printer state, tell it to execute gcode, stop printing etc. 

After building they can be added to your path using for example: 
``` bash
sudo ln -s "$PWD/build/Debug/frontends/p3d" /usr/bin/p3d
sudo ln -s "$PWD/build/Debug/server/print3d" /usr/bin/print3d
```
In one terminal window you can start print3d:
``` bash
print3d -V -d tty.usbmodemXXXXXX -p ultimaker
```
In another you can send commands using p3d:
``` bash
p3d -v -c 'G28 X Y Z'
```
