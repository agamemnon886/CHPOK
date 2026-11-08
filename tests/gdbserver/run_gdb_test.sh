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
# Runs tests/gdbserver with GDB connected to the kernel's GDB server: QEMU
# runs on the host with the second serial port on a TCP port, gdb-multiarch
# runs smoke.gdb (in the build image, or on the host with NATIVE=1). The
# test must be built first (make run-gdbserver; make gdb-test does both).
#
# Modified by Kirill Brazhnikov, 2026-11-05: arm64-qemu is tested too (BSP).
#
# Modified by Kirill Brazhnikov, 2026-11-06: HV_QEMU runs the test as a VM
# of the hypervisor (tests/hypervisor/gdb).
#
# Modified by Kirill Brazhnikov, 2026-11-08: the hypervisor runs on both
# BSPs.
#
# Usage: tests/gdbserver/run_gdb_test.sh
#   BSP       x86_64-qemu (default) or arm64-qemu
#   GDB_PORT  TCP port of the GDB server (default 8123)
#   TIMEOUT   seconds to wait for each step (default 90)
#   NATIVE    1: run ${GDB:-gdb-multiarch} on the host
#   VERBOSE   1: show the GDB and QEMU output (default 0)
#   HV_QEMU   qemu.sh of a hypervisor build that runs this test as a VM,
#             with its console on COM1, its GDB port on COM2 and the
#             hypervisor's console on COM3 (on either BSP)
#
# The test passes if every check of smoke.py passes, the system goes on
# after GDB detaches (both partitions print "DONE: 0 failure(s)") and the
# value written by GDB reaches P1 only. Exit status: 0 passed, 1 failed,
# 2 error.

set -u

dir=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$dir/../.." && pwd)
bsp=${BSP:-x86_64-qemu}
elf=$dir/build/$bsp/pok.elf
hv=${HV_QEMU:+-hv}
log=$dir/build/$bsp/gdb-test$hv.log
gdb_log=$dir/build/$bsp/gdb-test$hv-gdb.log
port=${GDB_PORT:-8123}
timeout=${TIMEOUT:-90}
verbose=${VERBOSE:-0}
image=chpok-build
container=chpok-gdb-test-$$

# The build makes GDB_commands (attach GDB to the partitions) too.
if [ ! -f "$elf" ] || [ ! -f "$dir/build/$bsp/GDB_commands" ]; then
    echo "$0: $elf not found; build it with: make run-gdbserver" >&2
    exit 2
fi
if [ -n "${HV_QEMU:-}" ] && [ ! -f "$HV_QEMU" ]; then
    echo "$0: $HV_QEMU not found; build it with: make hv-test" >&2
    exit 2
fi
case "$bsp" in
    x86_64-qemu) qemu="qemu-system-x86_64 -vga none" ;;
    arm64-qemu)  qemu="qemu-system-aarch64 -M virt -cpu cortex-a53" ;;
    *)
        echo "$0: unknown BSP=$bsp; use x86_64-qemu or arm64-qemu" >&2
        exit 2
        ;;
esac
if ! command -v "${qemu%% *}" > /dev/null; then
    echo "$0: ${qemu%% *} not found" >&2
    exit 2
fi

# Lines of a previous run mustn't be taken for this one's.
rm -f "$log" "$gdb_log"

# QEMU waits for GDB before it starts the system.
if [ -n "${HV_QEMU:-}" ]; then
    HV_COM1="file:$log" HV_COM2="tcp:127.0.0.1:$port,server=on,wait=on" \
        HV_COM3="file:$log.hypervisor" sh "$HV_QEMU" > /dev/null 2>&1 &
else
    $qemu -m 1G -display none -serial "file:$log" \
        -serial "tcp:127.0.0.1:$port,server=on,wait=on" -kernel "$elf" \
        > /dev/null 2>&1 &
fi
qemu_pid=$!
sleep 1

if [ -n "${NATIVE:-}" ]; then
    (cd "$dir" && GDB_BSP=$bsp GDB_TARGET=127.0.0.1:$port \
        ${GDB:-gdb-multiarch} -batch \
        -nx "build/$bsp/pok.elf" -x smoke.gdb) > "$gdb_log" 2>&1 &
else
    # Docker Desktop reaches the host's 127.0.0.1 as host.docker.internal;
    # on Linux the container uses the host's network.
    if [ "$(uname)" = Darwin ]; then
        net="--add-host=host.docker.internal:host-gateway"
        target=host.docker.internal:$port
    else
        net="--network=host"
        target=127.0.0.1:$port
    fi
    docker run --rm --init --name $container $net \
        --user "$(id -u):$(id -g)" -v "$root:$root" -w "$dir" \
        -e GDB_BSP=$bsp -e GDB_TARGET=$target $image \
        gdb-multiarch -batch -nx "build/$bsp/pok.elf" -x smoke.gdb \
        > "$gdb_log" 2>&1 &
fi
gdb_pid=$!

# Wait (polled 5 times a second) until 'pattern' is in 'file', or the
# process 'pid' ends, or the time-out.
wait_for() {
    ticks=0
    while ! grep -aq "$2" "$1" 2> /dev/null && kill -0 "$3" 2> /dev/null \
        && [ "$ticks" -lt $((timeout * 5)) ]; do
        sleep 0.2
        ticks=$((ticks + 1))
    done
    grep -aq "$2" "$1" 2> /dev/null
}

# Ctrl-C: GDB interrupts the system when it gets SIGINT, at each line
# GDB-TEST-INTERRUPT <n> (the system runs, or GDB steps a loop).
interrupted=0
for n in 1 2; do
    if wait_for "$gdb_log" "GDB-TEST-INTERRUPT $n" $gdb_pid; then
        sleep 1
        if [ -n "${NATIVE:-}" ]; then
            pkill -INT -f "smoke.gdb" && interrupted=$((interrupted + 1))
        else
            docker kill --signal=INT $container > /dev/null \
                && interrupted=$((interrupted + 1))
        fi
    fi
done

wait_for "$gdb_log" GDB-TEST-END $gdb_pid
if kill -0 $gdb_pid 2> /dev/null; then
    [ -n "${NATIVE:-}" ] || docker kill $container > /dev/null 2>&1
    kill $gdb_pid 2> /dev/null
fi
wait $gdb_pid 2> /dev/null

# After GDB has detached, the system goes on to the end of the test.
ticks=0
while [ "$(grep -ac 'DONE:' "$log")" -lt 2 ] \
    && [ "$ticks" -lt $((timeout * 5)) ] && kill -0 $qemu_pid 2> /dev/null; do
    sleep 0.2
    ticks=$((ticks + 1))
done
kill $qemu_pid 2> /dev/null
wait $qemu_pid 2> /dev/null

if [ "$verbose" = 1 ]; then
    echo "== GDB"
    sed 's/^/    /' "$gdb_log"
    echo "== QEMU"
    tr -d '\r' < "$log" | LC_ALL=C sed 's/\x1b\[[0-9;?]*[A-Za-z]//g; s/^/    /'
    if [ -n "${HV_QEMU:-}" ]; then
        echo "== hypervisor"
        tr -d '\r' < "$log.hypervisor" | sed 's/^/    /'
    fi
fi

passed=$(grep -ac 'GDB-TEST .*: PASS' "$gdb_log")
failed=$(grep -ac 'GDB-TEST .*: FAIL' "$gdb_log")
failures=""
[ "$failed" -eq 0 ] || failures="$failures
    $(grep -a 'GDB-TEST .*: FAIL' "$gdb_log")"
grep -aq GDB-TEST-END "$gdb_log" \
    || failures="$failures
    GDB session didn't finish (see $gdb_log)"
[ "$interrupted" = 2 ] || failures="$failures
    GDB wasn't interrupted twice"
grep -aq 'P1 POKE 42' "$log" \
    || failures="$failures
    P1 didn't get the value written by GDB"
! grep -aq 'P2 POKE' "$log" \
    || failures="$failures
    P2 got the value written into P1"
[ "$(grep -ac 'DONE: 0 failure(s)' "$log")" -eq 2 ] \
    || failures="$failures
    the partitions didn't finish without failures (see $log)"
! grep -aq FAIL "$log" \
    || failures="$failures
$(tr -d '\r' < "$log" | grep -a FAIL | sed 's/^/    /')"

where="$bsp${HV_QEMU:+ under the hypervisor}"
if [ -z "$failures" ]; then
    echo "PASS tests/gdbserver with GDB on $where: $passed checks"
    exit 0
fi

echo "FAIL tests/gdbserver with GDB on $where: $passed checks passed," \
    "$failed failed$failures"
echo "    GDB output: $gdb_log"
[ -z "${HV_QEMU:-}" ] || echo "    hypervisor console: $log.hypervisor"
exit 1
