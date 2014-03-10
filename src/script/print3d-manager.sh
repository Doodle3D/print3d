#!/bin/sh
# This file is part of the Doodle3D project (http://doodle3d.com).
#
# Copyright (c) 2013, Doodle3D
# This software is licensed under the terms of the GNU GPL v2 or later.
# See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.

PRINT3D_RES_PATH=/tmp
PRINT3D_RUNNER=/bin/print3d-runner.sh

inotifyd /bin/print3d-new-device.sh /dev:n
