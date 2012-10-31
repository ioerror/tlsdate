SUBDIRS = compat

AUTOMAKE_OPTIONS = foreign 1.7

# Our main program
bin_PROGRAMS = proxy_bio_unittest tlsdate-routeup tlsdate tlsdate-helper \
               tlsdated tlsdated_unittest
AM_CFLAGS = -fvisibility=hidden
proxy_bio_unittest_SOURCES = proxy-bio.c proxy-bio-unittest.c test-bio.c
proxy_bio_unittest_LDADD = -lssl -lcrypto
tlsdate_routeup_SOURCES = routeup.c util.c
tlsdate_routeup_CFLAGS = $(AM_CFLAGS) -DROUTEUP_MAIN
tlsdate_SOURCES = tlsdate.c
tlsdate_helper_SOURCES = proxy-bio.c tlsdate-helper.c
tlsdate_helper_LDADD = -lssl -lcrypto -lrt \
                       $(top_builddir)/src/compat/libtlsdate_compat.la
tlsdated_SOURCES = routeup.c tlsdated.c util.c
tlsdated_CFLAGS = $(AM_CFLAGS) -DTLSDATED_MAIN
tlsdated_unittest_SOURCES = routeup.c tlsdated-unittest.c util.c

TESTS = proxy_bio_unittest tlsdated_unittest

# We're not shipping headers
noinst_HEADERS = tlsdate.h
