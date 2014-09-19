#!/bin/sh
# Make sure that a longer steady state after a successful run
# doesn't call again too soon.
. "$(dirname $0)"/../common.sh

emit_time "timestamp"
# This one expects a timeout to end it.
passed_if_timed_out

inc_counter "runs"
c=$(counter "runs")
[ $c -eq 2 ] && failed

