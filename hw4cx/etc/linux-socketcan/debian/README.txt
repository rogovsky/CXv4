	How to make Debian bring CAN interfaces up at boot

0. Everything is made in /etc/network/

1. Create a "template" file /etc/network/can_template:
----/etc/network/can_template-----------------------------------------
iface canbus inet manual
        pre-up /sbin/ip link set $IFACE type can bitrate 125000
        pre-up /sbin/ip link set $IFACE type can restart-ms 200
        up     /sbin/ip link set $IFACE up
        down   /sbin/ip link set $IFACE down
----------------------------------------------------------------------

2. Create individual "interface definitions" for each canN as
/etc/network/can_interfaces/ (dir must be mkdir'ed).  Example for can0:
----/etc/network/can_interfaces/can0----------------------------------
auto can0
iface can0 inet manual inherits canbus
----------------------------------------------------------------------
2.1. In case of many interfaces, they can be cloned from can0.  Example
for 12 can interfaces (can0..can11):
----------------------------------------------------------------------
cd /etc/network/can_interfaces/
for i in {1..11};do sed -e s/can0/can$i/ <can0 >can$i;done
----------------------------------------------------------------------

3. Add use of new definitions to /etc/network/interfaces -- append the
following to its end:
----------------------------------------------------------------------
source can_template
source-directory can_interfaces
----------------------------------------------------------------------
