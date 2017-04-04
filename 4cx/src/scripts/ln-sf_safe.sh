#!/bin/sh

if [ "z${1}z" = "zz"  -o  "z${2}z" = "zz" ]
then
	echo "Usage: $0 TARGET LINK_NAME" >&2
	exit 1
fi

if [ -e "$2" -a ! -L "$2" ]
then
	echo "$0: '$2': File exists and isn't a symlink" >&2
	false
else
	ln -sf "$1" "$2"
fi
