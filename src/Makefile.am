SUBDIRS = compat

AUTOMAKE_OPTIONS = foreign 1.7

# Our main program
bin_PROGRAMS = tlsdate-routeup tlsdate tlsdate-helper \
               tlsdated tlsdated_unittest
if !POLARSSL
bin_PROGRAMS = $(bin_PROGRAMS) proxy_bio_unittest
endif
AM_CFLAGS = -fvisibility=hidden
if !POLARSSL
proxy_bio_unittest_SOURCES = proxy-bio.c proxy-bio-unittest.c test-bio.c util.c
proxy_bio_unittest_LDADD = -lssl -lcrypto
endif
tlsdate_routeup_SOURCES = routeup.c util.c
tlsdate_routeup_CFLAGS = $(AM_CFLAGS) -DROUTEUP_MAIN
tlsdate_SOURCES = tlsdate.c
tlsdate_helper_SOURCES = proxy-bio.c util.c tlsdate-helper.c
tlsdate_helper_LDADD = @SSL_LIBS@ @SSL_LDFLAGS@ -lrt \
                       $(top_builddir)/src/compat/libtlsdate_compat.la
tlsdated_SOURCES = routeup.c tlsdated.c util.c
tlsdated_CFLAGS = $(AM_CFLAGS) -DTLSDATED_MAIN
tlsdated_unittest_SOURCES = routeup.c tlsdated-unittest.c util.c

TESTS = tlsdated_unittest
if !POLARSSL
TESTS = $(TESTS) proxy_bio_unittest
endif

# We're not shipping headers
noinst_HEADERS = tlsdate.h
