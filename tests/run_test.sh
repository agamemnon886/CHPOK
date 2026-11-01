#!/bin/sh
# Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# Runs one test from tests/ in QEMU and checks its output (see
# tests/README). The test must be built first (make tests or
# make run-<name>; make run does both).
#
# Usage: tests/run_test.sh <name> [<board>]
#   <board>   x86_64-qemu (default) or arm64-qemu
#   TIMEOUT   seconds to wait for the test to finish (default 60)
#   VERBOSE   1: show the output of the test while it runs (default 0)
#
# The test passes if every partition prints its "DONE: 0 failure(s)" line
# and no line contains FAIL. Exit status: 0 passed, 1 failed, 2 error.

set -u

if [ $# -lt 1 ] || [ $# -gt 2 ]; then
    echo "usage: $0 <name> [<board>]" >&2
    exit 2
fi

name=$1
bsp=${2:-x86_64-qemu}
timeout=${TIMEOUT:-60}
verbose=${VERBOSE:-0}
dir=$(cd "$(dirname "$0")" && pwd)/$name
elf=$dir/build/$bsp/pok.elf
log=$dir/build/$bsp/test.log

case $bsp in
    x86_64-qemu) qemu="qemu-system-x86_64 -vga none" ;;
    arm64-qemu)  qemu="qemu-system-aarch64 -M virt -cpu cortex-a53" ;;
    *) echo "$0: unknown board $bsp" >&2; exit 2 ;;
esac

if [ ! -f "$dir/SConstruct" ]; then
    echo "$0: no test tests/$name" >&2
    exit 2
fi
if [ ! -f "$elf" ]; then
    echo "$0: $elf not found; build it with: make BSP=$bsp run-$name" >&2
    exit 2
fi
if ! command -v "${qemu%% *}" > /dev/null; then
    echo "$0: ${qemu%% *} not found" >&2
    exit 2
fi

# One "DONE:" line per partition (the directories with a SConscript).
expected=$(ls "$dir"/*/SConscript 2> /dev/null | wc -l | tr -d ' ')

# In verbose mode, print the complete lines added to the log since the
# last call, without carriage returns and terminal escape sequences (the
# x86_64 BIOS clears the screen).
shown=0
show_new_lines() {
    [ "$verbose" = 1 ] || return 0
    total=$(wc -l < "$log" | tr -d ' ')
    [ "$total" -gt "$shown" ] || return 0
    sed -n "$((shown + 1)),${total}p" "$log" \
        | LC_ALL=C awk '{ gsub(/\033\[[0-9;?]*[A-Za-z]|\033c|\r/, ""); print "    " $0 }'
    shown=$total
}

[ "$verbose" = 1 ] && echo "== running tests/$name on $bsp"

$qemu -m 1G -nographic -kernel "$elf" > "$log" 2>&1 < /dev/null &
qemu_pid=$!

# QEMU ignores SIGALRM, so wait here and kill it when the test is done,
# when QEMU stops by itself, or after the time-out. Polled 5 times a
# second.
ticks=0
while [ "$(grep -ac 'DONE:' "$log")" -lt "$expected" ] \
    && [ "$ticks" -lt $((timeout * 5)) ] && kill -0 $qemu_pid 2> /dev/null; do
    sleep 0.2
    ticks=$((ticks + 1))
    show_new_lines
done
kill $qemu_pid 2> /dev/null
wait $qemu_pid 2> /dev/null
show_new_lines

passed=$(grep -ac 'TEST .*: PASS' "$log")
failed=$(grep -ac 'FAIL' "$log")
done_all=$(grep -ac 'DONE:' "$log")
done_ok=$(grep -ac 'DONE: 0 failure(s)' "$log")
result="tests/$name on $bsp"

if [ "$failed" -eq 0 ] && [ "$done_ok" -ge "$expected" ]; then
    echo "PASS $result: $passed checks"
    exit 0
fi

echo "FAIL $result: $passed checks passed, $failed failed"
tr -d '\r' < "$log" | grep -a 'FAIL' | sed 's/^/    /'
if [ "$done_all" -lt "$expected" ]; then
    echo "    only $done_all of $expected partition(s) finished within ${timeout} s"
fi
echo "    full output: $log"
exit 1
