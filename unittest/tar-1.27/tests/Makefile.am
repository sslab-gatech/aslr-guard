# Makefile for GNU tar regression tests.

# Copyright 1996-1997, 1999-2001, 2003-2007, 2009, 2012-2013 Free Software

# This file is part of GNU tar.

# GNU tar is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.

# GNU tar is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

EXTRA_DIST = $(TESTSUITE_AT) testsuite package.m4 star/README star/quicktest.sh
DISTCLEANFILES       = atconfig $(check_SCRIPTS)
MAINTAINERCLEANFILES = Makefile.in $(TESTSUITE)

## ------------ ##
## package.m4.  ##
## ------------ ##

$(srcdir)/package.m4: $(top_srcdir)/configure.ac
	{                                      \
	  echo '# Signature of the current package.'; \
	  echo 'm4_define([AT_PACKAGE_NAME],      [@PACKAGE_NAME@])'; \
	  echo 'm4_define([AT_PACKAGE_TARNAME],   [@PACKAGE_TARNAME@])'; \
	  echo 'm4_define([AT_PACKAGE_VERSION],   [@PACKAGE_VERSION@])'; \
	  echo 'm4_define([AT_PACKAGE_STRING],    [@PACKAGE_STRING@])'; \
	  echo 'm4_define([AT_PACKAGE_BUGREPORT], [@PACKAGE_BUGREPORT@])'; \
	} >$(srcdir)/package.m4

#

## ------------ ##
## Test suite.  ##
## ------------ ##

TESTSUITE_AT = \
 T-cd.at\
 T-empty.at\
 T-null.at\
 T-rec.at\
 T-zfile.at\
 T-nonl.at\
 T-mult.at\
 T-nest.at\
 testsuite.at\
 append.at\
 append01.at\
 append02.at\
 append03.at\
 append04.at\
 backup01.at\
 chtype.at\
 comprec.at\
 delete01.at\
 delete02.at\
 delete03.at\
 delete04.at\
 delete05.at\
 exclude.at\
 exclude01.at\
 exclude02.at\
 exclude03.at\
 exclude04.at\
 exclude05.at\
 exclude06.at\
 exclude07.at\
 exclude08.at\
 exclude09.at\
 exclude10.at\
 exclude11.at\
 exclude12.at\
 exclude13.at\
 exclude14.at\
 exclude15.at\
 exclude16.at\
 extrac01.at\
 extrac02.at\
 extrac03.at\
 extrac04.at\
 extrac05.at\
 extrac06.at\
 extrac07.at\
 extrac08.at\
 extrac09.at\
 extrac10.at\
 extrac11.at\
 extrac12.at\
 extrac13.at\
 extrac14.at\
 extrac15.at\
 extrac16.at\
 extrac17.at\
 extrac18.at\
 extrac19.at\
 filerem01.at\
 filerem02.at\
 gzip.at\
 grow.at\
 incremental.at\
 incr01.at\
 incr02.at\
 incr03.at\
 incr04.at\
 incr05.at\
 incr06.at\
 incr07.at\
 incr08.at\
 incr09.at\
 indexfile.at\
 ignfail.at\
 label01.at\
 label02.at\
 label03.at\
 label04.at\
 label05.at\
 link01.at\
 link02.at\
 link03.at\
 link04.at\
 listed01.at\
 listed02.at\
 listed03.at\
 listed04.at\
 listed05.at\
 long01.at\
 longv7.at\
 lustar01.at\
 lustar02.at\
 lustar03.at\
 multiv01.at\
 multiv02.at\
 multiv03.at\
 multiv04.at\
 multiv05.at\
 multiv06.at\
 multiv07.at\
 multiv08.at\
 old.at\
 opcomp01.at\
 opcomp02.at\
 opcomp03.at\
 opcomp04.at\
 opcomp05.at\
 opcomp06.at\
 options.at\
 options02.at\
 owner.at\
 pipe.at\
 recurse.at\
 rename01.at\
 rename02.at\
 rename03.at\
 rename04.at\
 rename05.at\
 remfiles01.at\
 remfiles02.at\
 remfiles03.at\
 remfiles04a.at\
 remfiles04b.at\
 remfiles04c.at\
 remfiles05a.at\
 remfiles05b.at\
 remfiles05c.at\
 remfiles06a.at\
 remfiles06b.at\
 remfiles06c.at\
 remfiles07a.at\
 remfiles07b.at\
 remfiles07c.at\
 remfiles08a.at\
 remfiles08b.at\
 remfiles08c.at\
 remfiles09a.at\
 remfiles09b.at\
 remfiles09c.at\
 same-order01.at\
 same-order02.at\
 shortfile.at\
 shortupd.at\
 shortrec.at\
 sigpipe.at\
 sparse01.at\
 sparse02.at\
 sparse03.at\
 sparse04.at\
 sparsemv.at\
 sparsemvp.at\
 spmvp00.at\
 spmvp01.at\
 spmvp10.at\
 time01.at\
 truncate.at\
 update.at\
 update01.at\
 update02.at\
 volsize.at\
 volume.at\
 verbose.at\
 verify.at\
 version.at\
 xform-h.at\
 xform01.at\
 star/gtarfail.at\
 star/gtarfail2.at\
 star/multi-fail.at\
 star/ustar-big-2g.at\
 star/ustar-big-8g.at\
 star/pax-big-10g.at\
 xattr01.at\
 xattr02.at\
 xattr03.at\
 xattr04.at\
 xattr05.at\
 acls01.at\
 acls02.at\
 selnx01.at\
 selacl01.at\
 capabs_raw01.at

TESTSUITE = $(srcdir)/testsuite

AUTOTEST = $(AUTOM4TE) --language=autotest
$(TESTSUITE): package.m4 $(TESTSUITE_AT)
	$(AUTOTEST) -I $(srcdir) testsuite.at -o $@.tmp
	mv $@.tmp $@

atconfig: $(top_builddir)/config.status
	cd $(top_builddir) && ./config.status tests/$@

clean-local:
	test ! -f $(TESTSUITE) || $(SHELL) $(TESTSUITE) --clean

check-local: atconfig atlocal $(TESTSUITE)
	$(SHELL) $(TESTSUITE) $(TESTSUITEFLAGS)

check-full:
	FULL_TEST=1 $(MAKE) check

#check_SCRIPTS = tar

# Run the test suite on the *installed* tree.
installcheck-local:
	$(SHELL) $(TESTSUITE) $(TESTSUITEFLAGS) AUTOTEST_PATH=$(exec_prefix)/bin


## ------------ ##
## genfile      ##
## ------------ ##

check_PROGRAMS = genfile

genfile_SOURCES = genfile.c argcv.c argcv.h

localedir = $(datadir)/locale
AM_CPPFLAGS = \
 -I$(top_srcdir)/gnu\
 -I../gnu\
 -I$(top_srcdir)/gnu\
 -I$(top_srcdir)/lib\
 -DLOCALEDIR=\"$(localedir)\"

LDADD = ../gnu/libgnu.a $(LIBINTL) $(LIB_CLOCK_GETTIME) $(LIB_EACCESS)
