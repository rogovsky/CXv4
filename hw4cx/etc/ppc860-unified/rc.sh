#!/bin/sh

>/etc/mtab

# mount /proc so "reboot" works
/bin/mount -t proc proc /proc

/etc/rc.banner
/etc/rc.hardware

. /etc/arch.conf
. /etc/eth0.conf
MY_IP=$MY_NET.$MY_TAIL
test -z "$ARCH_MNTDIR"  &&  ARCH_MNTDIR=/export/HOST

/sbin/ifconfig lo 127.0.0.1

echo "Configuring eth0: ip $MY_IP, netmask 255.255.255.0..."
/sbin/ifconfig eth0 $MY_IP netmask 255.255.255.0
test -n "$MY_GW"  &&  /sbin/route add default gw $MY_GW

/usr/sbin/xinetd -stayalive -reuse -pidfile /tmp/xinetd.pid  ||  inetd -R 10

/etc/mounthost.sh $MY_HOST $ARCH_MNTDIR $MY_TAIL $ARCH_SUBDIR &
