JetOS is a real-time operating system for avionics applications. It was started as a fork of [POK](http://pok.tuxfamily.org/) that is an open source ARINC-653 compliant real-time operating system. All the changes made are provided as open source software under GPLv3 license.

Since then, code has been significantly rewritten and the new operating system have got a new name: *JetOS*.

The key new features include:
* preemptible kernel;
* APEX services complying to ARINC-653 part 1 specification;
* IO partitions with UDP/IP networking stack and device drivers.

JetOS supports x86_64 and arm64 (AArch64).

## Building

Examples are built with `make`, which runs the build in a Docker container
(see `misc/docker/Dockerfile`), so only `make` and Docker are needed, on
Linux or macOS:

    make                     # all examples for x86_64 (board x86_64-qemu)
    make BSP=arm64-qemu      # all examples for arm64 (board arm64-qemu)
    make memblocks           # one example
    make help                # other targets and options

The resulting kernel image `examples/<name>/build/<board>/pok.elf` runs in QEMU:

    qemu-system-x86_64 -vga none -m 1G -nographic -kernel examples/memblocks/build/x86_64-qemu/pok.elf
    qemu-system-aarch64 -M virt -cpu cortex-a53 -m 1G -nographic -kernel examples/memblocks/build/arm64-qemu/pok.elf

## Debugging

The x86_64 kernel has a GDB server which knows the partitions and their
processes. GDB connects to it over the second serial port, and sees the
kernel as process 1 and partition N as process N+2: the threads of process
1 are the kernel-only partitions (Monitor, GDB), the threads of a
partition's process are its ARINC 653 processes (the last one, IDLE, runs
when no process is ready). While GDB holds the system, the whole module is
stopped and the system time doesn't run, so time slots, deadlines and
timers aren't affected by debugging.

GDB can connect, and interrupt the running system with Ctrl-C, during the
time slots of type GDB in the schedule:

    <Slot Type="GDB" Duration="10ms" />

or at boot if the kernel is built with `-D POK_NEEDS_WAIT_FOR_GDB` (as
`examples/pure-arinc653-buffer-2-partitions` is: it waits for GDB before
it starts the partitions). A partition's initialization (`main()`) has
already run when the first GDB slot comes; to stop in it, wait for GDB at
boot (`cflags = ' -D POK_NEEDS_WAIT_FOR_GDB'` in the example's
`SConstruct`): breakpoints set then stay when the partitions' code is
loaded.

Run QEMU with the second serial port on a TCP port; QEMU waits for GDB:

    qemu-system-x86_64 -vga none -m 1G -serial stdio \
        -serial tcp:127.0.0.1:8000,server=on,wait=on \
        -kernel examples/msections-basic/build/x86_64-qemu/pok.elf

The build also makes `build/x86_64-qemu/GDB_commands`, a GDB script which
attaches GDB to every partition, each with the symbols of its `part.elf`.
Connect a GDB which supports x86-64 and has Python (such as
`gdb-multiarch`; not, e.g., `arm-none-eabi-gdb`) from the example
directory:

    gdb-multiarch build/x86_64-qemu/pok.elf \
        -ex 'source build/x86_64-qemu/GDB_commands' \
        -ex 'target extended-remote 127.0.0.1:8000'

`gdb-multiarch` is in the build image; on macOS, run it there (from the
top-level directory) and connect to QEMU on the host:

    docker run --rm -it --add-host=host.docker.internal:host-gateway \
        -v "$PWD:$PWD" -w "$PWD/examples/msections-basic" chpok-build \
        gdb-multiarch build/x86_64-qemu/pok.elf \
        -ex 'source build/x86_64-qemu/GDB_commands' \
        -ex 'target extended-remote host.docker.internal:8000'

Breakpoints, hardware breakpoints (`hbreak`) and watchpoints (together up
to four, in the debug registers), stepping, the x87 and SSE registers, and
reading and writing memory work per partition, although all partitions use
the same addresses. A breakpoint on a function is inserted in every
partition which has it (GDB 14 and later: `break main inferior 2`); a line
of a partition's source is given with its path, e.g.
`break P1/src/main.c:34`. `detach` lets the system go on.
`make gdb-test` runs `tests/gdbserver` with GDB.

Limitations: the server is all-stop (resuming resumes the whole module)
and steps only the thread which has stopped last (interrupts are disabled
for the stepped instruction, so no other thread runs meanwhile, unless
the instruction is a system call which waits); a stop in kernel code is
reported for the thread of the current partition; breakpoints in the code
the server itself uses (the serial driver, string functions) are fatal;
memory blocks can't be read yet; breakpoints left by a GDB which has gone
without detaching stop the system until GDB connects again. The server
isn't ported to arm64.

`next` in a polling loop, such as the GDB partition's own
(`pok_gdb_thread()`, where GDB stops after Ctrl-C), doesn't end until the
loop exits; stop it with Ctrl-C. GDB then still takes the step for
unfinished and resumes it when another inferior is detached, so resume
(e.g. `continue`) before `detach inferiors ...`.

QEMU's own gdb stub works on both architectures, without knowledge of the
partitions (`scons rundbg` and `scons debug` in an example directory).

## Testing

The tests under `tests/` check the APEX services against ARINC 653 and
libpok's C library (`tests/libc`). They are built like the examples and run in QEMU on the host, so
`qemu-system-x86_64` / `qemu-system-aarch64` must be installed:

    make run                     # build and run all tests for x86_64
    make BSP=arm64-qemu run      # the same for arm64
    make run-mutex               # one test
    make run VERBOSE=0           # only the results, not the test output

See `tests/README` for what each test covers and how to add one.

## MISRA C:2025

The kernel, libpok, the architecture code, the boards and syspart follow
MISRA C:2025. Where a guideline can't be followed, a comment that starts with
`MISRA C:2025` names it and gives the reason:

    grep -rn "MISRA C:2025" kernel libpok boards syspart misc
