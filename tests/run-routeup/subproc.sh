#!/bin/sh
. "$(dirname $0)"/../common.sh

inc_counter "runs"
c=$(counter "runs")
[ $c -eq 2 ] && passed

# Always emit a bogus time so that stdin retriggers.
src/test/emit "0"
