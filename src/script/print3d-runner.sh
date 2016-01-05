#!/bin/sh
# This file is part of the Doodle3D project (http://doodle3d.com).
#
# Copyright (c) 2013, Doodle3D
# This software is licensed under the terms of the GNU GPL v2 or later.
# See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.

#Note: temporary way to have print3d respect log level specified in UCI (print3d will read this internally later on)
LOG_LEVEL=`uci get wifibox.general.system_log_level`
LOG_FLAG=
case $LOG_LEVEL in
	quiet) LOG_FLAG=-Q ;;
	error) LOG_FLAG=-q ;;
	warning) ;;
	info) ;;
	verbose) LOG_FLAG=-v ;;
	bulk) LOG_FLAG=-V ;;
esac

PRINT3D_RES_PATH=/tmp
PRINT3D=/bin/print3d
DEVBASE=`basename $1`
OPTIONS=$LOG_FLAG

sleep 5

#start server without forking (this script is backgrounded by print3d-manager)
$PRINT3D -F -d ${DEVBASE} $OPTIONS >> $PRINT3D_RES_PATH/print3d-${DEVBASE}.log 2>&1

#remove socket (print3d-manager uses this to detect if a server is running for a certain device)
rm $PRINT3D_RES_PATH/print3d-${DEVBASE}
