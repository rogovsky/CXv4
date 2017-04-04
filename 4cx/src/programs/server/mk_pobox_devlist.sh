#!/bin/sh

N=1
while read t name rest
do
	echo "dev	box$N	noop	w1$t	-"
	echo "cpoint	$name	box$N.0 $rest"
	N=$[N+1]
done
