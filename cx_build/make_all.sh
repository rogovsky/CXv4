#!/bin/sh

set -e

CXDIR=$HOME/cx

make -C $CXDIR/4cx/src create-exports
make -C $CXDIR/4cx/src NOX=1
