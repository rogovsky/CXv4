#!/bin/sh

echo " "
echo "*************************************"
echo -n "CAN gateway linux image release "
cat /var/elf/image.release
echo
cat /var/elf/image.date
echo "*************************************"
echo " "

/sbin/ifconfig lo 127.0.0.1

/sbin/ifconfig eth0 192.168.1.2 netmask 255.255.255.0

>/etc/mtab

# mount /proc so "reboot" works
/bin/mount -t proc proc /proc

echo "Loading CAN driver..."
insmod /lib/modules/2.4.22/Can.o

echo "Configuring CAN interfaces..."
/etc/cansetup

/usr/sbin/xinetd -stayalive -reuse -pidfile /tmp/xinetd.pid

echo "Starting CAN gateway..."
/usr/bin/can4lgw
