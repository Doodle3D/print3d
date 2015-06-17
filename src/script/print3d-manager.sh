#!/bin/sh
# This file is part of the Doodle3D project (http://doodle3d.com).
#
# Copyright (c) 2013, Doodle3D
# This software is licensed under the terms of the GNU GPL v2 or later.
# See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.

# try to start print3d once at boot
/usr/libexec/print3d-new-device.sh

# then try to start print3d only when new devices appear
inotifyd /usr/libexec/print3d-new-device.sh /dev:n
