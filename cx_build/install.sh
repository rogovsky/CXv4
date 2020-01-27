#!/bin/sh


set -e

CXDIR=$HOME/cx

CXINSTDIR=$HOME/cx_inst

make -C $CXDIR/4cx/src install PREFIX=$CXINSTDIR NOX=1

make -C $CXDIR/frgn4cx install PREFIX=$CXINSTDIR NOX=1




