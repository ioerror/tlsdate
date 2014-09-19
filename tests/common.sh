#!/bin/sh

kill_tlsdated() {
	kill -TERM $PPID
}

result_passed() {
	res=$(cat "$(mydir)"/"result")
	if [ $res = "ok" ]; then
		return 0
	fi
	return 1
}

check_err() {
	grep -q "$1" "$(mydir)"/"run-err"
}

passed_if_timed_out() {
	echo "ok" > "$(mydir)"/"result"
}

passed() {
	echo "ok" > "$(mydir)"/"result"
	kill_tlsdated
}

failed() {
	echo "failed" > "$(mydir)"/"result"
	kill_tlsdated
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

reset_time() {
	date +%s > "$(mydir)"/"$1"
}

emit_time() {
	src/test/emit `cat "$(mydir)"/"$1"`
}
