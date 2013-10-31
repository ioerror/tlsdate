#!/bin/sh

kill_tlsdated() {
	kill -TERM $PPID
}

passed() {
	kill_tlsdated
	echo "ok"
}

failed() {
	kill_tlsdated
	echo "failed"
}

mydir() {
	echo "$(dirname "$0")"
}

counter() {
	cat "$(mydir)"/"$1"
}

inc_counter() {
	c=$(counter "$1")
	echo $((c + 1)) >"$(mydir)"/"$1"
}

reset_counter() {
	echo 0 > "$(mydir)"/"$1"
}
