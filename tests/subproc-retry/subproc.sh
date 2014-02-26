#!/bin/sh
. "$(dirname $0)"/../common.sh

inc_counter "runs"
if [ $(counter "runs") -lt 3 ]; then
	echo "dying"
	echo "$$" >> /tmp/pidz
	exit 1
fi
passed
