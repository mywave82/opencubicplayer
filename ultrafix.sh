#!/bin/bash
# OpenCP Module Player
# copyright (c) '94-'05 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
#
# ultrafix.sh - Fixed ULTRASOUND.INI files
#
# revision history: (please note changes here)
#  -ss050206   Stian Skjelstad <stian@nixia.no>
#    -first release

if [ -z "$2" ] || [ -n "$3" ]; then
	echo -e "Usage:\n$0 /path/to/ULTRASOUND.INI /path/to/pat/files"
else
	tmp=`echo $2|sed -e 's/\\//\\\\\\//g;s/.*/&\\\\\//;s/\\\\\/\\\\\/$/\\\\\//'`
	sed -e 's/\r//;s/[0-9]=.*/\U&/;s/PatchDir\=.*/PatchDir\='$tmp'/' "$1" > /tmp/ULTRASOUND.INI
	mv /tmp/ULTRASOUND.INI $1
fi
