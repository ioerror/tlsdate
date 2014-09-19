#!/bin/sh
. "$(dirname $0)"/../common.sh

inc_counter "runs"
c=$(counter "runs")
[ $c -eq 3 ] && passed

# Bump it three seconds to overcome the steady state interval.
inc_counter "timestamp"
inc_counter "timestamp"
inc_counter "timestamp"
emit_time "timestamp"
