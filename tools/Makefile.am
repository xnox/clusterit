## Process this file with automake to produce Makefile.in
AUTOMAKE=@AUTOMAKE@ --add-missing --foreign
ACLOCAL=@ACLOCAL@ -I autoconf

INCLUDES =

bin_SCRIPTS = dshbak prm clustersed pdf
CLEANFILES = $(bin_SCRIPTS) dshbak.sh

script_sources = dshbak.sh prm.sh clustersed.sh

do_subst = sed -e 's,\@AWK\@,@AWK@,g'

dshbak.sh: dshbak.src Makefile
	$(do_subst) < $(srcdir)/dshbak.src > dshbak.sh

pdf: pdf.sh
	cp pdf.sh pdf
	chmod 755 pdf

dshbak: dshbak.sh
	cp dshbak.sh dshbak
	chmod 755 dshbak

prm: prm.sh
	cp prm.sh prm
	chmod 755 prm

clustersed: clustersed.sh
	cp clustersed.sh clustersed
	chmod 755 clustersed

man_MANS = \
	clustersed.1 \
	dshbak.1 \
	pdf.1 \
	prm.1
