#!/bin/sh
# Wake up from stdin twice.
c=0
while [ $c -lt 120 ]; do
  echo
  sleep 0.2
  c=$((c+1))
done
