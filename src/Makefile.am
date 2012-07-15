AUTOMAKE_OPTIONS = foreign 1.7

# Our main program
bin_PROGRAMS = tlsdate tlsdate-helper
tlsdate_SOURCES = tlsdate.c
tlsdate_helper_SOURCES = tlsdate-helper.c
tlsdate_helper_LDADD = -lssl -lcrypto

# We're not shipping headers
noinst_HEADERS = tlsdate.h
