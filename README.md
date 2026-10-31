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

To debug, use QEMU's gdb stub (`scons rundbg` and `scons debug` in an
example directory).
