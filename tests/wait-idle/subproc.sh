#!/bin/sh
. "$(dirname $0)"/../common.sh

inc_counter "runs"
c=$(counter "runs")
[ $c -eq 2 ] && failed

