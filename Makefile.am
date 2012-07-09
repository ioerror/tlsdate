AUTOMAKE_OPTIONS = foreign 1.7
EXTRA_DIST = \
  AUTHORS \
  README \
  LICENSE \
  TODO \
  apparmor-profile \
  autogen.sh 

# Our main program
bin_PROGRAMS = tlsdate tlsdate-helper
tlsdate_SOURCES = tlsdate.c
tlsdate_helper_SOURCES = tlsdate-helper.c
tlsdate_helper_LDADD = -lssl -lcrypto

# Our documentation
man_MANS = tlsdate.1
EXTRA_DIST += $(man_MANS)

CLEANFILES = tlsdate-config.h.in tlsdate-config.h.in~ \
  tlsdate-config.h \
  Makefile.noautoconf \
  Makefile.in \
  Makefile \
  install-sh \
  depcomp \
  configure \
  config.sub \
  config.status \
  config.log \
  config.guess \
  aclocal.m4 \
  missing \
  stamp-h1

# XXX TODO:
# We also should remove the follow dirs with `make clean`
# autom4te.cache & .deps

.PHONY: debian_orig
debian_orig:
	$(MAKE) dist
	mv tlsdate-$(VERSION).tar.gz ../tlsdate_$(VERSION).orig.tar.gz
