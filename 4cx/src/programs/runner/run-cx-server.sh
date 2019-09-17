#!/bin/sh

[ -z "$PULT" ]  &&  PULT=~/4pult
THISHOST=`hostname -s|tr A-Z a-z`
SRVPARAMS_FILE=$PULT/configs/srvparams.conf
N=${1#:}

PARAMS=""
[ -f "$SRVPARAMS" ]  ||  SRVPARAMS=/dev/null
while read kw name rest
do
    if [ "$kw" = ".srvparams" ]
    then
        case $THISHOST:$N in
            $name)
                if [ "${rest#params=}" != "$rest" ]
                then
                    PARAMS="${rest#params=}"
                    # Is it ""-quoted?
                    if [ "${PARAMS%\"}" != "$PARAMS" -a "${PARAMS#\"}" != "$PARAMS" ]
                    then
                        PARAMS=${PARAMS%\"}
                        PARAMS=${PARAMS#\"}
                    fi
                fi
                ;;
        esac
    fi
done < ${SRVPARAMS_FILE}

echo "starting cxsd#$N PARAMS: $PARAMS"

cd $PULT
./sbin/cxsd -c configs/cxsd.conf -f configs/devlist-$THISHOST-$N.lst $PARAMS :$N
