# Copyright (C) 2013 Colin Walters <walters@verbum.org>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

NULL = 

ACLOCAL_AMFLAGS = -I m4 ${ACLOCAL_FLAGS}
AM_CPPFLAGS = -DDATADIR='"$(datadir)"' -DLIBEXECDIR='"$(libexecdir)"' \
	-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_34 -DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_34 \
	$(NULL)
AM_CFLAGS = $(WARN_CFLAGS)

INSTALL_DATA_HOOKS =
BUILT_SOURCES =
MANPAGES =
CLEANFILES =
EXTRA_DIST =
bin_PROGRAMS =
sbin_PROGRAMS =
bin_SCRIPTS =
libexec_PROGRAMS =
noinst_LTLIBRARIES =
noinst_PROGRAMS =
privlibdir = $(pkglibdir)
privlib_LTLIBRARIES =

EXTRA_DIST += autogen.sh COPYING

include Makefile-glvm.am
include Makefile-main.am

install-data-hook: $(INSTALL_DATA_HOOKS)

#  Docbook generation copied from systemd/Makefile.am
#
#  Copyright 2010 Lennart Poettering
#
#  systemd is free software; you can redistribute it and/or modify it
#  under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  systemd is distributed in the hope that it will be useful, but
#  WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
#  General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with systemd; If not, see <http://www.gnu.org/licenses/>.

XML_FILES = \
	${patsubst %.1,%.xml,${patsubst %.3,%.xml,${patsubst %.5,%.xml,${patsubst %.7,%.xml,${patsubst %.8,%.xml,$(MANPAGES)}}}}}
EXTRA_DIST += $(XML_FILES)

if BUILD_DOCUMENTATION

dist_man_MANS = $(MANPAGES)

XSLTPROC_FLAGS = \
	--nonet \
	--param funcsynopsis.style "'ansi'"

XSLTPROC_PROCESS_MAN = \
	$(AM_V_GEN)$(MKDIR_P) $(dir $@) && \
	$(XSLTPROC) -o $@ $(XSLTPROC_FLAGS) http://docbook.sourceforge.net/release/xsl/current/manpages/docbook.xsl $<

doc/%.1: doc/%.xml
	$(XSLTPROC_PROCESS_MAN)

endif

dist-snapshot:
	set -x; \
	GITVERSION=$(git_version_rpm); export GITVERSION; \
	TARFILE_TMP=$(PACKAGE)-$${GITVERSION}.tar.tmp; \
	REV=$$(git rev-parse HEAD); \
	echo "Archiving $(PACKAGE) at $${REV}"; \
	(cd $(srcdir); git archive --format=tar --prefix=ostree-$${GITVERSION}/ $${REV}) > $${TARFILE_TMP}; \
	(cd $$(git rev-parse --show-toplevel); git submodule status) | while read line; do \
	  rev=$$(echo $$line | cut -f 1 -d ' '); path=$$(echo $$line | cut -f 2 -d ' '); \
	  echo "Archiving $${path} at $${rev}"; \
	  (cd $(srcdir)/$$path; git archive --format=tar --prefix=ostree-$${GITVERSION}/$$path/ $${rev}) > submodule.tar; \
	  tar -A -f $${TARFILE_TMP} submodule.tar; \
	  rm submodule.tar; \
	done; \
	mv $(PACKAGE)-$${GITVERSION}.tar{.tmp,}; \
	gzip -f $(PACKAGE)-$${GITVERSION}.tar 
