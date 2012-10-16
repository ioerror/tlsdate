# vim:ft=automake

EXTRA_DIST=
TESTS=
bin_PROGRAMS=
check_PROGRAMS=
lib_LTLIBRARIES=
man_MANS=
noinst_HEADERS=
noinst_LTLIBRARIES=
noinst_PROGRAMS=

# GNU style is "make check", this will make check and test work
TESTS+= $(check_PROGRAMS)
test: check

AM_CFLAGS = -fvisibility=hidden

# 
ACLOCAL_AMFLAGS= -I m4 --install --force

# Our Debian version
DEBIAN_VERSION := $(shell if [ -e debian/changelog ]; then cat debian/changelog|head -n1|cut -d\- -f2| head -c 1; else echo "unknown"; fi)

EXTRA_DIST+= AUTHORS
EXTRA_DIST+= README
EXTRA_DIST+= LICENSE
EXTRA_DIST+= TODO
EXTRA_DIST+= apparmor-profile
EXTRA_DIST+= CHANGELOG
EXTRA_DIST+= autogen.sh 

include src/include.am

# Our documentation
man_MANS+= man/tlsdate.1
man_MANS+= man/tlsdate-helper.1
EXTRA_DIST+= $(man_MANS)

.PHONY: debian_orig git-tag git-push git-tag-debian deb really-clean
debian_orig:
	$(MAKE) distcheck
	mv tlsdate-$(VERSION).tar.gz ../tlsdate_$(VERSION).orig.tar.gz

git-tag:
	git tag -u 0xD81D840E -s tlsdate-$(VERSION)

git-tag-debian:
	git tag -u 0xD81D840E -s tlsdate-$(VERSION)-debian-${DEBIAN_VERSION}

git-push:
	git push --tags
	git push

deb: debian_orig
	debuild -rfakeroot -uc -us -d

valgrind_test:
	TESTS_ENVIRONMENT="./libtool --mode=execute valgrind  --trace-children=yes --leak-check=full" ./src/tlsdate -v -V -n -H encrypted.google.com
