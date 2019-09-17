#!/bin/sh

cd

[ -z "$PULT" ]  &&  PULT=~/4pult
THISHOST=`hostname -s|tr A-Z a-z`
LIST_FILE=$PULT/configs/cx-servers-$THISHOST.conf

for N in `sed -e 's/#.*//' <$LIST_FILE`
do
	[ -z "${N//[0-9]}" ]  &&  $PULT/bin/run-cx-server :$N
done
