#!/bin/sh

PRINT3D_RES_PATH=/tmp
PRINT3D_RUNNER=/bin/print3d-runner.sh

sleep 20
while true; do 
	for DEV in $(ls /dev/ttyACM* /dev/ttyUSB*); do
		DEVBASE=`basename ${DEV}`
		SOCKET=$PRINT3D_RES_PATH/print3d-${DEVBASE}
		if [ ! -S $SOCKET ]; then
			$PRINT3D_RUNNER ${DEVBASE} &
			#disown $$
			sleep 2
		fi
	done
	sleep 5
done
