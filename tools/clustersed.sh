#!/usr/bin/awk -f
#$Id: clustersed.sh,v 1.1 2000/02/25 21:48:58 garbled Exp $
BEGIN {
	OFS = "";
	RS = "\n";
	FS = ":";
	clusterfile = ARGV[1];
	NARGC = ARGC;
	ARGC = 2;
	if (ENVIRON["CLUSTER"] && clusterfile == "-")
		ARGV[1] = ENVIRON["CLUSTER"]

}

{
	if (match($1,/GROUP/)) {
		reading = 0;
		for (i = 2; i < NARGC; i++) {
			len = length(ARGV[i]);
			newstr = substr($2, 0, len);
			if (ARGV[i] == newstr) {
				reading = 1;
				print "GROUP:", $2;
			} else {
				reading = 0;
			}
		}
		next;
	}
	if (reading == 1) {
		print $1;
	}
}
