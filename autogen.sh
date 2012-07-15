#!/bin/sh
#
# This generates our configure scripts and leads us onto the path of 
# the great Makefile...
#

set -e

if [ ! -d config ];
then
  mkdir config;
fi

exec autoreconf -ivf -I src -I config && \
aclocal && \
autoheader && \
autoconf && \
automake --add-missing --copy
