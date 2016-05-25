#!/bin/sh
# This file is part of the Doodle3D project (http://doodle3d.com).
#
# Copyright (c) 2013-2014, Doodle3D
# This software is licensed under the terms of the GNU GPL v2 or later.
# See file LICENSE.txt or visit http://www.gnu.org/licenses/gpl.html for full license details.

PRINT3D_RES_PATH=/tmp
PRINT3D_RUNNER=/usr/libexec/print3d-runner.sh
LOG_ID=print3d-mgr

for DEV in $(ls /dev/ttyACM* /dev/ttyUSB*); do
	DEVBASE=`basename ${DEV}`

	SOCKET=$PRINT3D_RES_PATH/print3d-${DEVBASE}

	logger -t $LOG_ID "Starting print3d server for /dev/$DEVBASE"
	if [ ! -S $SOCKET ]; then
		$PRINT3D_RUNNER ${DEVBASE} &
		#disown $!
		# Make sure that print3d-runner and childs are never killed by the OOM-killer
		# oom_adj can range from -16 to +15 with -17 being a special oom_disabled value.
		# oom_score_adj is more modern and can range from -1000 to +1000. It does not have a oom killer disabled but with -1000 (added to the badness score) there is a very very slim change that the oom killer can kill it
		# See http://zurlinux.com/?p=2147  The lowest possible value, -1000, is equivalent to disabling oom killing entirely for that task since it will always report a badness score of 0.
		(echo -1000 > /proc/$!/oom_score_adj)
		sleep 2
	fi
done
