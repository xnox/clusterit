#!/bin/sh
# $Id: pdf.sh,v 1.1 1998/10/13 06:58:51 garbled Exp $
args=`getopt klng:t:w: $*`
if test $? != 0
then
	echo 'Usage: pdf [-kln] [-g size] [-t type] [-w node1,...,nodeN] [file | file_system ...]'
	exit 2
fi
set -- $args
for i
do
	case "$i"
	in
		-k|-l|-n)
			flag=`echo $flag $i`; shift;;
		-t)
			flag=`echo $flag $i $2`; shift; shift;;
		-w)
			warg=$2; shift; shift;;
		-g)
			garg="$2%"; shift; shift;;
		--)
			shift; break;;
	esac
done
(
if [ -z $warg ]; then \
	dsh df $flag $* ;\
else \
	dsh -w $warg df $flag $* ;\
fi 
)| grep -v Filesystem | awk '{print $1 " " $2 " " $3 " " $4 " " $5 " " $6 " " $7}' |(
echo 'Node    Filesystem             Blocks     Used    Avail  Cap Mounted On'
while read node fs blocks used avail capacity mount; do \
	if [ $capacity \> $garg -a $garg ]; then \
		printf "%-8s%-19.19s %9d%9d%9d %4.4s %-19.19s\n" $node $fs $blocks $used $avail $capacity $mount; \
	fi \
done
)
