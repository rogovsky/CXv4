#!/bin/sh

echo " "
echo "*************************************"
echo -n "CM5307 Power PC linux image release "
cat /var/elf/image.release
echo
cat /var/elf/image.date
echo "*************************************"
echo " "
echo "/etc/rc.sh: configuring loopback interface"
ifconfig lo 127.0.0.1
echo "/etc/rc.sh: configuring ethernet interface"
ifconfig eth0 192.168.1.2 netmask 255.255.255.0
if !(/usr/sbin/camac_ok) then
echo "/etc/rc.sh: loading firmware"
pload /etc/cm5307/cam_bus.rbf
fi
if (/usr/sbin/camac_ok) then
echo "/etc/rc.sh: loading camac driver"
insmod /lib/modules/2.4.22/camac.o
fi

>/etc/mtab

# mount /proc so "reboot" works
/bin/mount -t proc proc /proc

/usr/sbin/xinetd -stayalive -reuse -pidfile /tmp/xinetd.pid
