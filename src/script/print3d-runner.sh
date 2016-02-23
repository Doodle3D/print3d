#!/bin/sh
# This file is part of the Doodle3D project (http://doodle3d.com).
#
# Copyright (c) 2013-2014, Doodle3D
# This software is licensed under the terms of the GNU GPL v2 or later.
# See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.

PRINT3D_RES_PATH=/tmp
PRINT3D=/bin/print3d
DEVBASE=`basename $1`

#Options:
# -F: start server without forking (this script is backgrounded by print3d-manager)
# -u: read settings from UCI (mainly log location and level)
# -d ${DEVBASE}: which device to use for communication with the printer
OPTIONS="-u -F -d ${DEVBASE}"

sleep 5

#Note: we do not redirect output anymore; except for a few startup messages (which
#can be seen when running the server manually), everything can be found in the log file.
$PRINT3D $OPTIONS
#$PRINT3D $OPTIONS >> $PRINT3D_RES_PATH/print3d-${DEVBASE}.log 2>&1

#remove socket (print3d-manager uses this to detect if a server is running for a certain device)
rm $PRINT3D_RES_PATH/print3d-${DEVBASE}
