#!/bin/sh

cd

CONFIG_FILE=~/4pult/configs/cx-starter-`hostname -s|tr A-Z a-z`.conf
PARAMS_FILE=~/4pult/configs/srvparams.conf
LOG_FILE=/var/tmp/cx-starter.log

if [ ! -f "$CONFIG_FILE" ]
then
	echo "$0: config file \"$CONFIG_FILE\" is missing" >&2
	exit 1
fi

if [ ! -f "$PARAMS_FILE" ]
then
	PARAMS_FILE=""
fi

GEOMETRY=`grep '^#GEOMETRY=' $CONFIG_FILE | sed -e 's/#GEOMETRY=//'`
GEOM_OPT=""
if [ -n "$GEOMETRY" ]
then
	GEOM_OPT="-geometry"
fi

(xterm -geometry +0-0 -iconic -sl 10000 -T "cx-starter logs" -e \
 /bin/sh -c \
 "~/4pult/bin/cx-starter $GEOM_OPT $GEOMETRY $CONFIG_FILE $PARAMS_FILE 2>&1 | tee -a $LOG_FILE" &)
