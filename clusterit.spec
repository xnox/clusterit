Summary: clusterit is a collection of tools for distributed computing.
Name: clusterit
Version: 2.3
Release: 1
License: BSD with advertising clause (Tim Rightnour), BSD-style (John Bovey)
Group: Utilities
URL: http://www.garbled.net/clusterit.html
Prefix: /usr
Source0: %{name}-%{version}.tar.gz
BuildRoot: /var/tmp/%{name}-build

%description
This is a collection of clustering tools to turn your ordinary
everyday pile of UNIX workstations into a speedy parallel beast.

It includes: 
       dsh -- Run a command on lots of machines in parallel.
       rseq -- Run a command on lots of machines in series.
       run -- Run a command on a random machine.
       jsh/jsd -- Run commands on a pool of machines, one per machine.
       pcp/pdf/prm -- distributed copy, df, and rm.
       dvt -- Use many machines interactively simultaneously.
       rvt -- Hacked version of xvt used by dvt to open the terminals.
       barrier/barrierd -- Synchonize your parallel shell scripts.

%prep
if [ -z "$SOURCE_DIR" ] ; then
       echo "SOURCE_DIR must be set to the root of the clusterit source distribution."
       echo 'Try something like SOURCE_DIR=`pwd` rpmbuild -bb clusterit.spec'
       exit -1
fi

%build

cd $SOURCE_DIR
[ -f dsh/rseq.1 ] || mv dsh/seq.1 dsh/rseq.1
[ -f dsh/rseq.c ] || mv dsh/seq.c dsh/rseq.c

%install
rm -rf ${RPM_BUILD_ROOT}
mkdir -p ${RPM_BUILD_ROOT}/usr/bin
mkdir -p ${RPM_BUILD_ROOT}/usr/man/man1
make PREFIX=${RPM_BUILD_ROOT}/usr -C $SOURCE_DIR install

%clean
rm -rf $RPM_BUILD_ROOT


%files
%dir /usr
%dir /usr/bin
%dir /usr/man
%dir /usr/man/man1
/usr/bin/barrier
/usr/bin/barrierd
/usr/bin/clustersed
/usr/bin/dsh
/usr/bin/dshbak
/usr/bin/dvt
/usr/bin/jsd
/usr/bin/jsh
/usr/bin/pcp
/usr/bin/pdf
/usr/bin/prm
/usr/bin/rseq
/usr/bin/run
/usr/bin/rvt

/usr/man/man1/barrier.1.gz
/usr/man/man1/barrierd.1.gz
/usr/man/man1/clustersed.1.gz
/usr/man/man1/dsh.1.gz
/usr/man/man1/dshbak.1.gz
/usr/man/man1/dvt.1.gz
/usr/man/man1/jsd.1.gz
/usr/man/man1/jsh.1.gz
/usr/man/man1/pcp.1.gz
/usr/man/man1/pdf.1.gz
/usr/man/man1/prm.1.gz
/usr/man/man1/rseq.1.gz
/usr/man/man1/run.1.gz
/usr/man/man1/rvt.1.gz

%doc

%changelog
* Sun Oct  3 2004 Tim Rightnour <root@garbled.net> - 
- Initial build.

