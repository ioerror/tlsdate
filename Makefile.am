# Our Debian version
DEBIAN_VERSION := $(shell if [ -d debian/changelog ]; then cat debian/changelog|head -n1|cut -d\- -f2| head -c 1; else echo "unknown"; fi) 

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

# We're not shipping headers
noinst_HEADERS = tlsdate.h

# Our documentation
man_MANS = tlsdate.1 tlsdate-helper.1
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

.PHONY: debian_orig git-tag git-push git-tag-debian deb
debian_orig:
	$(MAKE) dist
	mv tlsdate-$(VERSION).tar.gz ../tlsdate_$(VERSION).orig.tar.gz

git-tag:
	git tag -u 0xD81D840E -s tlsdate-$(VERSION)

git-tag-debian:
	git tag -u 0xD81D840E -s tlsdate-$(VERSION)-debian-${DEBIAN_VERSION}

git-push:
	git push --tags
	git push

deb: debian_orig
	debuild -rfakeroot -uc -us
