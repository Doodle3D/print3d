#!/bin/sh

PRINT3D_RES_PATH=/tmp
PRINT3D_RUNNER=/bin/print3d-runner.sh

DEVBASE=`dmesg | tail -2 | sed -n -e 's/.*\(tty\w*\).*/\1/p'`
SOCKET=$PRINT3D_RES_PATH/print3d-${DEVBASE}

logger -t print3d-mgr "Starting print3d server for /dev/$DEVBASE"
if [ ! -S $SOCKET ]; then
	$PRINT3D_RUNNER ${DEVBASE} &
	#disown $$
	sleep 2
fi
