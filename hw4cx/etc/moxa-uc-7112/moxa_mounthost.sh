#!/bin/sh

mkdir -p /tmp/host

SECONDS=10
while ! mount -t nfs -o nolock,ro 192.168.129.254:/export/HOST /tmp/host
do
    sleep $SECONDS
    SECONDS=60
done

/tmp/host/oper/4pult/lib/server/drivers/moxa_arm_drvlets/cxv4moxaserver 8002 -d >/dev/null 2>/dev/null &
