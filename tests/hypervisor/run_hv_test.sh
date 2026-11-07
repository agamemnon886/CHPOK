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
# Runs the hypervisor tests: each directory here holds a configuration
# whose build/qemu.sh (made by make hv-test, or make hv HV_CONFIG=...) is
# run in QEMU on the host, one after another.
#
#   apex-mutex  tests/apex and tests/mutex pass side by side
#   bad-mem     a VM with too little RAM stops with a nested page fault,
#               the other VM still passes
#   reset       the monitor's reset command restarts a VM
#   gdb         tests/gdbserver/run_gdb_test.sh with the test as a VM
#
# Usage: tests/hypervisor/run_hv_test.sh [case...]
#   (default: every directory here with a config.xml; each one needs a
#   case_<name>() below, with - in the name written as _)
#   TIMEOUT   seconds to wait for each case (default 120)
#   VERBOSE   1: show the logs of each case (default 0)
#   NATIVE    passed on to run_gdb_test.sh
#   PORT      first TCP port used (default 8160)

set -u

dir=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$dir/../.." && pwd)
timeout=${TIMEOUT:-120}
verbose=${VERBOSE:-0}
port=${PORT:-8160}
cases=${*:-$(cd "$dir" && for c in */config.xml; do dirname "$c"; done)}

if ! command -v qemu-system-x86_64 > /dev/null; then
    echo "$0: qemu-system-x86_64 not found" >&2
    exit 2
fi

show_logs() {
    [ "$verbose" = 1 ] || return 0
    for f in "$@"; do
        echo "== $f"
        tr -d '\r' < "$f" \
            | LC_ALL=C sed 's/\x1b\[[0-9;?]*[A-Za-z]//g; s/^/    /'
    done
}

# Number of lines of file $2 with $1 (0 if there is no file yet).
count() {
    n=$(grep -ac "$1" "$2" 2> /dev/null)
    echo "${n:-0}"
}

# Start the case's QEMU in the background: qemu_pid. The hypervisor
# console goes to $build/hv.log, the VM consoles to $build/<vm>.log.
start_qemu() {
    build=$dir/$1/build
    if [ ! -x "$build/qemu.sh" ] || [ ! -f "$build/hv.elf" ]; then
        echo "$0: $build/hv.elf not found; build it with: make hv-test" >&2
        exit 2
    fi
    rm -f "$build"/*.log
    shift
    env HV_COM1="file:$build/hv.log" "$@" sh "$build/qemu.sh" \
        > "$build/qemu.out" 2>&1 &
    qemu_pid=$!
}

stop_qemu() {
    kill $qemu_pid 2> /dev/null
    wait $qemu_pid 2> /dev/null
}

# Wait until the shell condition $1 holds, QEMU stops or time runs out.
wait_until() {
    ticks=0
    while ! eval "$1" && [ "$ticks" -lt $((timeout * 5)) ] \
        && kill -0 $qemu_pid 2> /dev/null; do
        sleep 0.2
        ticks=$((ticks + 1))
    done
    eval "$1"
}

expected_done() {
    ls "$root/tests/$1"/*/SConscript 2> /dev/null | wc -l | tr -d ' '
}

# A JetOS test's console: all partitions done, no failure.
test_passed() {
    [ "$(count 'DONE: 0 failure(s)' "$1")" -ge "$2" ] \
        && [ "$(count FAIL "$1")" -eq 0 ]
}

case_apex_mutex() {
    start_qemu apex-mutex
    apex=$(expected_done apex)
    mutex=$(expected_done mutex)
    wait_until "[ \$(count DONE: $build/apex.log) -ge $apex ] \
        && [ \$(count DONE: $build/mutex.log) -ge $mutex ]"
    stop_qemu
    show_logs "$build/hv.log" "$build/apex.log" "$build/mutex.log"
    test_passed "$build/apex.log" "$apex" \
        && test_passed "$build/mutex.log" "$mutex" \
        && ! grep -aq 'stopped' "$build/hv.log"
}

case_bad_mem() {
    start_qemu bad-mem
    mutex=$(expected_done mutex)
    wait_until "grep -aqs 'small: stopped' $build/hv.log \
        && [ \$(count DONE: $build/mutex.log) -ge $mutex ]"
    stop_qemu
    show_logs "$build/hv.log" "$build/small.log" "$build/mutex.log"
    grep -aq 'small: access to guest-physical address' "$build/hv.log" \
        && grep -aq 'small: stopped' "$build/hv.log" \
        && test_passed "$build/mutex.log" "$mutex"
}

case_reset() {
    start_qemu reset HV_COM2="tcp:127.0.0.1:$port,server=on,wait=on"
    python3 "$dir/monitor_reset.py" "$port" "$build/mon.log" "$timeout"
    result=$?
    stop_qemu
    show_logs "$build/hv.log" "$build/mon.log"
    [ "$result" -eq 0 ] && grep -aq 'mon: reset' "$build/hv.log"
}

# The hypervisor is x86_64 only, whatever BSP the caller has.
case_gdb() {
    build=$dir/gdb/build
    BSP=x86_64-qemu VERBOSE=$verbose GDB_PORT=$((port + 1)) \
        TIMEOUT=$timeout HV_QEMU="$build/qemu.sh" \
        "$root/tests/gdbserver/run_gdb_test.sh"
}

# Where a case leaves its logs.
logs_of() {
    if [ "$1" = gdb ]; then
        echo "$root/tests/gdbserver/build/x86_64-qemu/gdb-test-hv*"
    else
        echo "$dir/$1/build"
    fi
}

status=0
for c in $cases; do
    run=case_$(echo "$c" | tr - _)
    if ! command -v "$run" > /dev/null; then
        echo "FAIL tests/hypervisor/$c: $0 has no $run()"
        status=1
        continue
    fi
    if "$run"; then
        echo "PASS tests/hypervisor/$c"
    else
        echo "FAIL tests/hypervisor/$c (logs: $(logs_of "$c"))"
        status=1
    fi
done
exit $status
