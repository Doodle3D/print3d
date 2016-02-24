## PRINT3D README

This package provides access to multiple types of 3D printers. It aims to be usable at least on
OSX and Openwrt (linux) and currently provides a command-line frontend and a lua binding.


## HOW TO BUILD

This package is intended to be built either for embedded devices using the Openwrt build system or natively using CMake.
Building natively is done using the script `build-local.sh` (or manually). The included Eclipse CDT project also has the necessary targets configured. Requirements to build and run the code natively are: cmake (more to come).
Building for OpenWRT is slightly complex and not described here.

C++ unit tests use Fructose 1.2.0 (http://fructose.sourceforge.net/).


## Startup scripts

The Print3D package includes shell scripts that start print3D when a printer is connected.
- The init script `/etc.init.d/`[print3d](https://github.com/Doodle3D/print3d/blob/master/src/script/print3d_init), started at boot, runs inotifyd which in turn starts a print server when a printer is connected.
- Inotifyd uses `/usr/libexec/`[print3d-new-device](https://github.com/Doodle3D/print3d/blob/master/src/script/print3d-new-device.sh) to detect the device path to start a server for and in turn starts `print3d-runner` _backgrounded_ to spawn the server.
- `/usr/libexec/`[print3d-runner.sh](https://github.com/Doodle3D/print3d/blob/master/src/script/print3d-runner.sh) then finally starts the print3d server. Since the printer server creates a socket under /tmp to communicate with the front-ends, `print3d-runner.sh` does not background it, so it is able to remove the socket after the server exits.


## Command-line usage

- **Print3D** is a daemon that communicates with the printer. It sends the printer gcode line by line from its buffer, it automatically checks the temperature etc. As described above, it is started when a printer is connected.
- **P3D** is a front-end/client to talk to the daemon. It can be used for example to request the current temperature and printer state, or to execute gcode, stop printing, etc.

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


## Miscellaneous notes

Code would be more readable and maintainable if all code was C++ (except for the Lua binding, which is the reason for the code partly being written in C).
In hindsight, the IPC mechanism seems overly complicated because GCode has to be split up in very small chunks. A simpler mechanism should be possible (e.g. shared memory or even an existing library). Interesting read on various IPC mechanisms: <http://www.advancedlinuxprogramming.com/alp-folder/alp-ch05-ipc.pdf>.
