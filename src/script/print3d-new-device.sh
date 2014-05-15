#!/bin/sh
# This file is part of the Doodle3D project (http://doodle3d.com).
#
# Copyright (c) 2013, Doodle3D
# This software is licensed under the terms of the GNU GPL v2 or later.
# See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.

PRINT3D_RES_PATH=/tmp
PRINT3D_RUNNER=/usr/libexec/print3d-runner.sh

DEVBASE=`dmesg | tail -2 | sed -n -e 's/.*\(tty\w*\).*/\1/p'`
SOCKET=$PRINT3D_RES_PATH/print3d-${DEVBASE}

if [ "x$DEVBASE" == "x" ]; then
	logger -t print3d-mgr "Not starting print3d server, no valid usb log message detected"
	exit 1
fi

logger -t print3d-mgr "Starting print3d server for /dev/$DEVBASE"
if [ ! -S $SOCKET ]; then
	$PRINT3D_RUNNER ${DEVBASE} &
	#disown $$
	sleep 2
fi
