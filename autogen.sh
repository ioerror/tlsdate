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

WARNINGS="all,error"
export WARNINGS

autoreconf --install --verbose --force
