#!/bin/sh

set -e

CXDIR=$HOME/cx

# create directiries for exports
make -C $CXDIR/4cx/src create-exports
# make inplace
make -C $CXDIR/4cx/src NOX=1
# copy what just maked to exports
make -C $CXDIR/4cx/src exports