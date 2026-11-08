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
# Modified by Kirill Brazhnikov, 2026-11-07: the bad-crc case (the image
# check at boot).
#
# Modified by Kirill Brazhnikov, 2026-11-08: BSP selects the hypervisor
# that is tested (x86_64-qemu or arm64-qemu), built in build/$BSP/.
#
# Runs the hypervisor tests: each directory here holds a configuration
# whose build/$BSP/qemu.sh (made by make hv-test, or make hv
# HV_CONFIG=...) is run in QEMU on the host, one after another.
#
#   apex-mutex  tests/apex and tests/mutex pass side by side
#   bad-mem     a VM with too little RAM stops with a nested page fault
#               (a stage-2 abort on arm64), the other VM still passes
#   reset       the monitor's reset command restarts a VM
#   bad-crc     the image as built passes the CRC check at boot; a copy
#               with one bit flipped stops with a CRC mismatch
#   gdb         tests/gdbserver/run_gdb_test.sh with the test as a VM
#
# Usage: tests/hypervisor/run_hv_test.sh [case...]
#   (default: every directory here with a config.xml; each one needs a
#   case_<name>() below, with - in the name written as _)
#   BSP       x86_64-qemu (default) or arm64-qemu
#   TIMEOUT   seconds to wait for each case (default 120)
#   VERBOSE   1: show the logs of each case (default 0)
#   NATIVE    passed on to run_gdb_test.sh
#   PORT      first TCP port used (default 8160)

set -u

dir=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$dir/../.." && pwd)
bsp=${BSP:-x86_64-qemu}
timeout=${TIMEOUT:-120}
verbose=${VERBOSE:-0}
port=${PORT:-8160}
cases=${*:-$(cd "$dir" && for c in */config.xml; do dirname "$c"; done)}

case "$bsp" in
    x86_64-qemu) qemu=qemu-system-x86_64 ;;
    arm64-qemu)  qemu=qemu-system-aarch64 ;;
    *)
        echo "$0: unknown BSP=$bsp; use x86_64-qemu or arm64-qemu" >&2
        exit 2
        ;;
esac
if ! command -v $qemu > /dev/null; then
    echo "$0: $qemu not found" >&2
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
    build=$dir/$1/build/$bsp
    if [ ! -x "$build/qemu.sh" ] || [ ! -f "$build/hv.elf" ]; then
        echo "$0: $build/hv.elf not found; build it with:" \
            "make hv-test BSP=$bsp" >&2
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

# hvcrc.py --check of hv.elf $1: "<stored> <computed> <size> <offset>"
# of the checked bytes; the exit status is 1 if the CRCs differ.
crc_of() {
    crc_line=$(python3 "$root/hypervisor/hvcrc.py" --check "$1")
    crc_status=$?
    crc_pattern='.* of \([0-9]*\) bytes at offset \([0-9]*\): '
    crc_pattern=$crc_pattern'stored \([^,]*\), computed \(.*\)$'
    echo "$crc_line" | sed -n "s/$crc_pattern/\\3 \\4 \\1 \\2/p"
    return $crc_status
}

case_bad_crc() {
    # The image as built: the check passes and the VM starts.
    crc=$(crc_of "$dir/bad-crc/build/$bsp/hv.elf") || return 1
    set -- $crc
    start_qemu bad-crc
    wait_until "grep -aqs 'mutex: starts on CPU' $build/hv.log"
    stop_qemu
    show_logs "$build/hv.log"
    grep -aq "image CRC-32 $1 OK" "$build/hv.log" \
        && grep -aq 'mutex: starts on CPU' "$build/hv.log" || return 1

    # A copy with one bit flipped in the middle of the checked bytes
    # (mostly the guest image): the hypervisor stops before any VM runs.
    corrupt=$build/corrupt
    rm -rf "$corrupt"
    mkdir "$corrupt"
    cp "$build/qemu.sh" "$build/hv.elf" "$corrupt/"
    offset=$(($4 + $3 / 2))
    byte=$(od -An -tu1 -j "$offset" -N1 "$corrupt/hv.elf" | tr -d ' ')
    printf "$(printf '\\%03o' $((byte ^ 1)))" \
        | dd of="$corrupt/hv.elf" bs=1 seek="$offset" conv=notrunc \
        2> /dev/null
    if crc=$(crc_of "$corrupt/hv.elf"); then
        echo "$0: flipping the bit at $offset didn't change the CRC" >&2
        return 1
    fi
    set -- $crc
    env HV_COM1="file:$corrupt/hv.log" sh "$corrupt/qemu.sh" \
        > "$corrupt/qemu.out" 2>&1 &
    qemu_pid=$!
    wait_until "grep -aqs 'fatal error' $corrupt/hv.log"
    stop_qemu
    show_logs "$corrupt/hv.log"
    grep -aq "image CRC-32 mismatch: expected $1, actual $2" \
        "$corrupt/hv.log" \
        && grep -aq 'fatal error on CPU 0: the hypervisor image' \
        "$corrupt/hv.log" \
        && ! grep -aq 'starts on CPU' "$corrupt/hv.log"
}

case_gdb() {
    build=$dir/gdb/build/$bsp
    BSP=$bsp VERBOSE=$verbose GDB_PORT=$((port + 1)) \
        TIMEOUT=$timeout HV_QEMU="$build/qemu.sh" \
        "$root/tests/gdbserver/run_gdb_test.sh"
}

# Where a case leaves its logs.
logs_of() {
    if [ "$1" = gdb ]; then
        echo "$root/tests/gdbserver/build/$bsp/gdb-test-hv*"
    else
        echo "$dir/$1/build/$bsp"
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
