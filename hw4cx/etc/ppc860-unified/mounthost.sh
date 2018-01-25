#!/bin/sh

#### Get parameters from command line ################################
MY_HOST=$1
MY_PATH=$2
MY_TAIL=$3
MY_SUBD=$4

#### Definitions of places ###########################################
MY_HOSTDIR=$MY_HOST:$MY_PATH
MNT_HOST=/mnt/host
HOMEDIR=$MNT_HOST/$MY_SUBD
export HOMEDIR

RC_LOCAL_LCLETC=/etc/rc.local
RC_LOCAL_COMMON=$HOMEDIR/etc/rc.d/rc.local
RC_LOCAL_MYBYIP=$HOMEDIR/etc/rc.d/rc.local.$MY_TAIL

ISSUE_NET=/etc/issue.net
ISSUE_COMMON=$HOMEDIR/etc/issue.net
PLACE_MYBYIP=$HOMEDIR/etc/issue.net.place.$MY_TAIL

#### Mount filesystem from host ######################################
/bin/mkdir -p $MNT_HOST

echo "Mounting $MNT_HOST from $MY_HOSTDIR..."
SECONDS=10
while ! mount -t nfs -o nolock,ro $MY_HOSTDIR $MNT_HOST
do
    sleep $SECONDS
    SECONDS=60
done
echo "...done"

#### Retrieve date from host #########################################
rdate -s $MY_HOST

#### /etc/issue.net issues ###########################################
if [ -f $ISSUE_COMMON ]
then
    cp -p $ISSUE_COMMON $ISSUE_NET
fi
if   [ -f $PLACE_MYBYIP ]
then
    cat $PLACE_MYBYIP >>$ISSUE_NET
fi
 
#### Finally, select what to run #####################################
if   [ -n "$MY_TAIL" -a -f $RC_LOCAL_MYBYIP -a -x $RC_LOCAL_MYBYIP ]
then
    $RC_LOCAL_MYBYIP
elif [                  -f $RC_LOCAL_COMMON -a -x $RC_LOCAL_COMMON ]
then
    $RC_LOCAL_COMMON
else
    $RC_LOCAL_LCLETC
fi

#### END of mounthost.sh #############################################
