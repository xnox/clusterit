SUBDIR=	dsh pcp barrier jsd rvt dvt tools

catmans:
	for dir in ${SUBDIR} ; do \
		(cd ../$$dir ; \
		for i in `ls *.1` ; do \
			echo "nroff -man $$i >../catman/`/usr/bin/basename $$i .1`.0" ; \
			nroff -man $$i >../catman/`/usr/bin/basename $$i .1`.0 ;\
		done );\
	done
