# vim:ft=automake

AM_CFLAGS="-D__STDC_LIMIT_MACROS"

BUILT_SOURCES=
doc_DATA=
EXTRA_DIST=
TESTS=
bin_PROGRAMS=
sbin_PROGRAMS=
check_PROGRAMS=
lib_LTLIBRARIES=
man_MANS=
noinst_HEADERS=
noinst_LTLIBRARIES=
noinst_PROGRAMS=

if !TARGET_OSX
# GNU style is "make check", this will make check and test work
TESTS+= src/conf_unittest src/proxy-bio_unittest
if TARGET_LINUX
TESTS+= src/tlsdated_unittest
endif
test: check
endif

ACLOCAL_AMFLAGS= -I m4

# Our Debian version
DEBIAN_VERSION := $(shell if [ -e debian/changelog ]; then cat debian/changelog|head -n1|cut -d\- -f2| head -c 1; else echo "unknown"; fi)

doc_DATA+= AUTHORS
doc_DATA+= CHANGELOG
doc_DATA+= LICENSE
doc_DATA+= README
doc_DATA+= TODO

EXTRA_DIST+= $(doc_DATA)
EXTRA_DIST+= apparmor-profile
EXTRA_DIST+= autogen.sh 
EXTRA_DIST+= tlsdated.service

include src/include.am

# Cleanup individual files in order to preserve uninstall/etc order
maintainer-clean-local:
	@rm -r -f autom4te.cache
	@rm Makefile.in
	@rm aclocal.m4
	@rm config.in
	@rm config/config.guess
	@rm config/config.sub
	@rm config/depcomp
	@rm config/install-sh
	@rm config/ltmain.sh
	@rm config/missing
	@rm configure
	@rmdir config
	@rm dbus/org.torproject.tlsdate.conf
	@rm m4/libtool.m4
	@rm m4/ltoptions.m4
	@rm m4/ltsugar.m4
	@rm m4/ltversion.m4
	@rm m4/lt~obsolete.m4
	@rm -f tlsdate-*.tar.gz
	-rm src/configmake.h
	-rm dbus/org.torproject.tlsdate.conf
	@find ./ | $(GREP) \~$$ | xargs rm -f

certdir =  @TLSDATE_CA_ROOTS@
cert_DATA = ca-roots/tlsdate-ca-roots.conf
EXTRA_DIST+= $(cert_DATA)

confdir = @TLSDATE_CONF_DIR@
if TARGET_LINUX
conf_DATA = etc/tlsdated.conf
EXTRA_DIST+= $(conf_DATA)
endif

# Our documentation
man_MANS+= man/tlsdate.1
man_MANS+= man/tlsdate-helper.1

if TARGET_LINUX
man_MANS+= man/tlsdated.8
man_MANS+= man/tlsdated.conf.5
endif

EXTRA_DIST+= $(man_MANS)

.PHONY: debian_orig git-tag git-push git-tag-debian deb really-clean valgrind_test
debian_orig:
	$(MAKE) dist
	mv tlsdate-$(VERSION).tar.gz ../tlsdate_$(VERSION).orig.tar.gz

git-tag:
	git tag -u 0x1801A819 -s tlsdate-$(VERSION)

git-tag-debian:
	git tag -u 0x1801A819 -s tlsdate-$(VERSION)-debian-${DEBIAN_VERSION}

git-push:
	git push --tags
	git push

deb: debian_orig
	debuild -i'.*' -rfakeroot -uc -us -d

# This generates code coverage data that is useful for continuous integration
lcov:
#  ./configure --enable-code-coverage-checks && make && # ... run everything and then...
	lcov --directory src -b . --capture --output-file tlsdate.lcov && genhtml -o lcov/ tlsdate.lcov

valgrind_test:
	TESTS_ENVIRONMENT="./libtool --mode=execute valgrind  --trace-children=yes --leak-check=full" ./src/tlsdate -v -V -n -H encrypted.google.com

# This allows us to parse the Mozilla NSS CA trusted root list and ensures we
# respect the trust bits as they are set - using them directly without the
# context is dangerous. This gives us a basic set of CA roots to trust for use
# with tlsdate without relying on any system CA list.
# You'll need agl's extract-nss-root-certs to recreate this file:
# https://github.com/agl/extract-nss-root-certs
update_ca_root_data:
	curl https://mxr.mozilla.org/mozilla/source/security/nss/lib/ckfw/builtins/certdata.txt\?raw\=1 -o ca-roots/certdata.txt
	go run ~/Documents/code/git/extract-nss-root-certs/convert_mozilla_certdata.go ca-roots/certdata.txt > ca-roots/tlsdate-ca-roots.conf
	rm ca-roots/certdata.txt

test_run:
	./src/tlsdate-helper google.com 443 tlsv1 racket verbose ca-roots/tlsdate-ca-roots.conf dont-set-clock showtime no-fun holdfast none

BUILT_SOURCES+= src/configmake.h
noinst_HEADERS+= src/configmake.h
src/configmake.h: ${top_srcdir}/Makefile.in
	@echo "Making $@"
	@rm -f $@-t $@
	@{ echo '/* DO NOT EDIT! GENERATED AUTOMATICALLY! */'; \
	  echo '#define TLSDATE_CONFIG "$(sysconfdir)/ca-roots/"'; \
	  echo '#define TLSDATE_CERTFILE "$(sysconfdir)/tlsdate/ca-roots/tlsdate-ca-roots.conf"'; \
	  echo '#define TLSDATE_CONF_DIR "$(sysconfdir)/tlsdate/"'; \
	  echo '#define TLSDATE_HELPER "$(bindir)/tlsdate-helper"'; \
	  echo '#define TLSDATE "$(bindir)/tlsdate"'; \
	  echo '#define TLSDATED "$(bindir)/tlsdated"'; \
	  } | sed '/""/d' > $@-t
	@if diff $@-t $@ >/dev/null 2>&1 ; then \
	  rm @-t ; \
	  else \
	  mv $@-t $@ ; \
	  fi

