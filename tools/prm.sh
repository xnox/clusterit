#!/bin/sh
# $Id: prm.sh,v 1.2 1999/05/05 08:37:43 garbled Exp $
args=`getopt dfrPRWw:g:x: $*`
if test $? != 0
then
	echo 'Usage: prm [-dfrPRW] [-g nodegroup] [-w node1,...,nodeN] [-x node1,...,nodeN] file ...'
	exit 2
fi
set -- $args
for i
do
	case "$i"
	in
		-d|-f|-r|-P|-R|-W)
			flag=`echo $flag $i`; shift;;
		-w)
			warg=$2; shift; shift;;
		-g)
			garg=$2; shift; shift;;
		-x)
			xarg=$2; shift; shift;;
		--)
			shift; break;;
	esac
done
if [ -n "$warg" ]; then
	dshargs=`echo "-w $warg"`
elif [ -n "$garg" ]; then
	dshargs=`echo "-g $garg"`
fi
if [ -n "$xarg" ]; then
	dshargs=`echo "$dshargs -x $xarg"`
fi
dsh $dshargs rm $flag $*
