#!/bin/sh

PRINT3D_RES_PATH=/tmp
PRINT3D=/bin/print3d
DEVBASE=`basename $1`

sleep 1

#start server without forking (this script is backgrounded by print3d-manager)
$PRINT3D -F -d ${DEVBASE} >> $PRINT3D_RES_PATH/print3d-${DEVBASE}.log 2>&1

#remove socket (print3d-manager uses this to detect if a server is running for a certain device)
rm $PRINT3D_RES_PATH/print3d-${DEVBASE}
