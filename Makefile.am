# vim:ft=automake

BUILT_SOURCES=
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

ACLOCAL_AMFLAGS= -I m4

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
	@rm m4/libtool.m4
	@rm m4/ltoptions.m4
	@rm m4/ltsugar.m4
	@rm m4/ltversion.m4
	@rm m4/lt~obsolete.m4
	@rm -f tlsdate-*.tar.gz
	@find ./ | $(GREP) \~$$ | xargs rm -f

certdir =  @TLSDATE_CA_ROOTS@
cert_DATA = ca-roots/tlsdate-ca-roots.conf
EXTRA_DIST+= $(cert_DATA)


# Our documentation
man_MANS+= man/tlsdate.1
man_MANS+= man/tlsdate-helper.1
EXTRA_DIST+= $(man_MANS)

.PHONY: debian_orig git-tag git-push git-tag-debian deb really-clean valgrind_test
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

BUILT_SOURCES+= configmake.h
noinst_HEADERS+= configmake.h
configmake.h: ${top_srcdir}/Makefile.in
	@echo "Making $@"
	@rm -f $@-t $@
	@{ echo '/* DO NOT EDIT! GENERATED AUTOMATICALLY! */'; \
	  echo '#define TLSDATE_CONFIG "$(sysconfdir)/ca-roots/"'; \
	  } | sed '/""/d' > $@-t
	@if diff $@-t $@ >/dev/null 2>&1 ; then \
	  rm @-t ; \
	  else \
	  mv $@-t $@ ; \
	  fi

