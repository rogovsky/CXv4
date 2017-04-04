#!/bin/sh

export SOUNDCHAN=@t100:ichw1-2:0.sounddev.0@u
export CDACLIENT=~/4pult/bin/cdaclient

while test -n "$1"
do               
        echo $1  
        export $1        
        shift    
done             

$CDACLIENT $SOUNDCHAN="''"
$CDACLIENT -mD-q $SOUNDCHAN | (while true
do
	read name
	#echo "name=$name"
	if [ "Z$name" != "Z" ]
	then
		play $name
		$CDACLIENT $SOUNDCHAN="''"
	fi
done)
