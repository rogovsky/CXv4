#!/bin/sh

CHANLIST="%4.2f:uset %4.2f:umes %4.2f:iexc %4.2f:imes %4.2f:finj %4.2f:fmes %7.2f:bias1 %7.2f:bias2 %7.2f:pincd1 %7.2f:pincd2 %7.2f:prefl1 +%7.2f:prefl2"

while true
do
    FNAME=/var/tmp/rezonator-journal-since-`date +%Y%m%d-%H%M`.log
    ~/4pult/bin/das-experiment -T 86400 -b canhw:19.rezonator $CHANLIST -o $FNAME
    (gzip -9 $FNAME&)
done
