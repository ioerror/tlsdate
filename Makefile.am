# Our Debian version
DEBIAN_VERSION := $(shell if [ -e debian/changelog ]; then cat debian/changelog|head -n1|cut -d\- -f2| head -c 1; else echo "unknown"; fi)
AUTOMAKE_OPTIONS = foreign 1.7
EXTRA_DIST = \
  AUTHORS \
  README \
  LICENSE \
  TODO \
  apparmor-profile \
  CHANGELOG \
  autogen.sh 

SUBDIRS = src

# Our documentation
man_MANS = man/tlsdate.1 man/tlsdate-helper.1
EXTRA_DIST += $(man_MANS)

.PHONY: debian_orig git-tag git-push git-tag-debian deb really-clean
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
	debuild -rfakeroot -uc -us -d

valgrind_test:
	PATH=src/:$(PATH) libtool --mode=execute valgrind  --trace-children=yes --leak-check=full ./src/tlsdate -v -V -n -H encrypted.google.com

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

