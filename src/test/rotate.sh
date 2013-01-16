#!/bin/sh
echo "args: $@"
if [ "$2" = "host1" -a "$4" = "port1" -a "$6" = "proxy1" ]; then exit 1; fi
if [ "$2" = "host2" -a "$4" = "port2" -a "$6" = "proxy2" ]; then exit 2; fi
exit 3
