SUBDIR=	dsh pcp barrier jsd rvt dvt tools dtop

htmlmans:
	for dir in ${SUBDIR} ; do \
		(cd ../../$$dir ; \
		for i in `ls *.1` ; do \
			echo "groff -mdoc2html -dformat=HTML -P-b -P-u -P-o -Tascii \
			-ww $$i >../html/man/`/usr/bin/basename $$i .1`.html" ; \
			groff -mdoc2html -dformat=HTML -P-b -P-u -P-o -Tascii \
			-ww $$i >../html/man/`/usr/bin/basename $$i .1`.html ; \
		done );\
	done
	for i in `ls *.html` ; do \
		cat $$i | sed -e 's@../html1/@@g' >outfile ; \
		mv outfile $$i ;\
	done
