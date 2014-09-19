#!/bin/sh
# Die on the first three attempts.
. "$(dirname $0)"/../common.sh

inc_counter "runs"
if [ $(counter "runs") -lt 3 ]; then
	exit 1
fi
passed
