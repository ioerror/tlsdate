#!/bin/sh
#
# This generates our configure scripts and leads us onto the path of 
# the great Makefile...
#

set -e

exec autoreconf -ivf && \
aclocal && \
autoheader && \
autoconf && \
automake --add-missing --copy
