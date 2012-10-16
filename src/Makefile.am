SUBDIRS = compat

AUTOMAKE_OPTIONS = foreign 1.7

# Our main program
bin_PROGRAMS = tlsdate-routeup tlsdate tlsdate-helper tlsdated tlsdated_unittest
AM_CFLAGS = -fvisibility=hidden
tlsdate_routeup_SOURCES = routeup.c util.c
tlsdate_routeup_CFLAGS = $(AM_CFLAGS) -DROUTEUP_MAIN
tlsdate_SOURCES = tlsdate.c
tlsdate_helper_SOURCES = tlsdate-helper.c
tlsdate_helper_LDADD = -lssl -lcrypto -lrt \
                       $(top_builddir)/src/compat/libtlsdate_compat.la
tlsdated_SOURCES = routeup.c tlsdated.c util.c
tlsdated_CFLAGS = $(AM_CFLAGS) -DTLSDATED_MAIN
tlsdated_unittest_SOURCES = routeup.c tlsdated-unittest.c util.c

TESTS = tlsdated_unittest

# We're not shipping headers
noinst_HEADERS = tlsdate.h
