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
Linux or macOS.

On macOS with [Colima](https://github.com/abiosoft/colima) as the Docker
runtime, start it and build the Docker build image first:

    colima start             # start the Docker VM
    make docker-image        # (re)build the build image

Then build the examples:

    make                     # all examples for x86_64 (board x86_64-qemu)
    make BSP=arm64-qemu      # all examples for arm64 (board arm64-qemu)
    make memblocks           # one example
    make help                # other targets and options

The resulting kernel image `examples/<name>/build/<board>/pok.elf` runs in QEMU:

    qemu-system-x86_64 -vga none -m 1G -nographic -kernel examples/memblocks/build/x86_64-qemu/pok.elf
    qemu-system-aarch64 -M virt -cpu cortex-a53 -m 1G -nographic -kernel examples/memblocks/build/arm64-qemu/pok.elf

## Debugging

The kernel (x86_64 and arm64) has a GDB server which knows the partitions
and their processes. GDB connects to it over the second serial port, and
sees the kernel as process 1 and partition N as process N+2: the threads of
process 1 are the kernel-only partitions (Monitor, GDB), the threads of a
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

    qemu-system-aarch64 -M virt -cpu cortex-a53 -m 1G -display none \
        -serial stdio -serial tcp:127.0.0.1:8000,server=on,wait=on \
        -kernel examples/msections-basic/build/arm64-qemu/pok.elf

On arm64 the second serial port exists only if QEMU is given two `-serial`
options (the `virt` board of a recent QEMU, tested with 11.1, then has a
second UART, which the kernel finds in the device tree that QEMU passes);
with one, as in `-nographic`, the GDB partition just idles.

The build also makes `build/<board>/GDB_commands`, a GDB script which
attaches GDB to every partition, each with the symbols of its `part.elf`.
Connect a GDB which supports the target and has Python (such as
`gdb-multiarch`; not, e.g., `arm-none-eabi-gdb`) from the example
directory, with the `pok.elf` and `GDB_commands` of the board QEMU runs
(with the other board's, GDB misreads the registers, e.g. "Invalid hex
digit"):

    # x86_64
    gdb-multiarch build/x86_64-qemu/pok.elf \
        -ex 'source build/x86_64-qemu/GDB_commands' \
        -ex 'target extended-remote 127.0.0.1:8000'

    # arm64
    gdb-multiarch build/arm64-qemu/pok.elf \
        -ex 'source build/arm64-qemu/GDB_commands' \
        -ex 'target extended-remote 127.0.0.1:8000'

`gdb-multiarch` is in the build image; on macOS, run it there (from the
top-level directory) and connect to QEMU on the host:

    # x86_64
    docker run --rm -it --add-host=host.docker.internal:host-gateway \
        -v "$PWD:$PWD" -w "$PWD/examples/msections-basic" chpok-build \
        gdb-multiarch build/x86_64-qemu/pok.elf \
        -ex 'source build/x86_64-qemu/GDB_commands' \
        -ex 'target extended-remote host.docker.internal:8000'

    # arm64
    docker run --rm -it --add-host=host.docker.internal:host-gateway \
        -v "$PWD:$PWD" -w "$PWD/examples/msections-basic" chpok-build \
        gdb-multiarch build/arm64-qemu/pok.elf \
        -ex 'source build/arm64-qemu/GDB_commands' \
        -ex 'target extended-remote host.docker.internal:8000'

The system is already running when GDB connects: resume it with
`continue` (not `run`, which would start `pok.elf` on the host).

Breakpoints, hardware breakpoints (`hbreak`) and watchpoints (in the debug
registers: together up to four on x86_64; six breakpoints and four
watchpoints on arm64), stepping, the floating point registers (x87 and SSE;
FP/SIMD on arm64), and reading and writing memory work per partition,
although all partitions use the same addresses. The kernel's code can be
debugged too (in inferior 1, see below). A breakpoint on a function is
inserted in every partition which has it (GDB 14 and later: `break main
inferior 2`); a line of a partition's source is given with its path, e.g.
`break P1/src/main.c:34`. `detach` lets the system go on. `make gdb-test`
(`make BSP=arm64-qemu gdb-test`) runs `tests/gdbserver` with GDB.

Limitations: the server is all-stop (resuming resumes the whole module) and
steps only the thread which has stopped last (interrupts are disabled for
the stepped instruction, so no other thread runs meanwhile, unless the
instruction is a system call which waits); a stop in the kernel's code (a
breakpoint or watchpoint in a system call or an interrupt handler) is
reported for the thread of the GDB partition, which stands for the kernel
(the partition's own thread is shown where it has entered the kernel);
breakpoints in the code the server itself uses (the serial driver, string
functions) are fatal; memory blocks can't be read yet; breakpoints left by
a GDB which has gone without detaching stop the system until GDB connects
again; on arm64, debug exceptions are masked in the exception entry and
return code and in the server itself, so breakpoints and watchpoints don't
fire there.

`next` in a polling loop, such as the GDB partition's own
(`pok_gdb_thread()`, where GDB stops after Ctrl-C), doesn't end until the
loop exits; stop it with Ctrl-C. GDB then still takes the step for
unfinished and resumes it when another inferior is detached, so resume
(e.g. `continue`) before `detach inferiors ...`.

QEMU's own gdb stub works on both architectures, without knowledge of the
partitions (`scons rundbg` and `scons debug` in an example directory).

## Hypervisor

`hypervisor/` is a small x86_64 hypervisor that runs several JetOS images
(`pok.elf`, built for `x86_64-qemu`) side by side. Each image runs in its
own virtual machine (VM) with its own RAM, CPUs and serial ports, as an
XML file describes. The VMs are isolated from each other, and JetOS
partitions the applications inside each VM as usual.

The hypervisor uses AMD SVM with nested paging. QEMU emulates both without
KVM (TCG), so it runs in QEMU on any host, including Macs with Apple
silicon (`-cpu qemu64,+svm,+npt`). QEMU's TCG doesn't emulate Intel VMX.
With KVM on an AMD Linux host, nested SVM should also work
(`-enable-kvm -cpu host`), but that hasn't been tested.

### Building and running

Hypervisor configurations are in `examples/hv-*/config.xml`. Run these
commands from the top-level directory. Like the examples, the build runs in
the Docker build image (`NATIVE=1` builds on the host):

    make hv                      # examples/hv-two-vms
    make hv HV=<name>            # examples/<name>/config.xml
    make hv HV_CONFIG=<file>     # any configuration in the repository
    make run-hv                  # build, then run it in QEMU
    make hv-test                 # the hypervisor tests (tests/hypervisor)

`make hv` first builds the JetOS projects whose images the configuration
names: an image path `.../<project>/build/x86_64-qemu/pok.elf` builds
`<project>`. Then it builds the hypervisor. The results go to `build/`
next to the configuration file:

    hv.elf          the hypervisor, with the VMs' images embedded
    qemu.sh         runs hv.elf in QEMU with the right CPUs, RAM and
                    serial ports
    hv_config.c,    generated from the configuration (hypervisor/hvconfig.py)
    hv_images.S,
    imageN.bin

`make` and `make clean` skip the `hv-*` directories as examples, because
they have no `SConstruct`. `make clean` (or `make clean-hv`) removes their
`build/` directories.

`qemu.sh` connects the serial ports in COM order:
- the hypervisor's console goes to the terminal (stdio);
- each VM's console goes to the file `build/<vm>.log`;
- each GDB port goes to a TCP port, starting at 8100 (`GDB_PORT` changes
  the first one);
- unused ports are left unconnected.

To watch a VM's console:

    tail -f examples/hv-two-vms/build/vm1.log

`HV_COM1` ... `HV_COM4` replace the QEMU `-serial` value of a port. For
example, to type into a VM's console (the JetOS monitor):

    HV_COM3=tcp:127.0.0.1:8200,server=on,wait=off \
        sh examples/hv-two-vms/build/qemu.sh
    nc 127.0.0.1 8200

Further arguments of `qemu.sh` are passed to QEMU. Stop QEMU with Ctrl-C
(or `kill`). The hypervisor console shows the VMs and what happens to
them:

    [hv] JetOS hypervisor: 2 VM(s), 3 CPU(s)
    [hv] TSC 1000381 kHz, local APIC timer 62552 kHz
    [hv] 127 MiB of RAM, 3 CPU(s)
    [hv] vm1: 64 MiB at 0x400000, 1 CPU(s) from CPU 0, console COM2, GDB none
    [hv] vm2: 32 MiB at 0x4400000, 2 CPU(s) from CPU 1, console COM3, GDB COM4
    [hv] vm2: vCPU 1 parked on CPU 2 (the guest uses one CPU)
    [hv] vm2: starts on CPU 1
    [hv] vm1: starts on CPU 0

### Configuration

`examples/hv-two-vms/config.xml`:

    <Hypervisor>
        <Console Port="COM1"/>

        <VM Name="vm1" CPUs="1" Memory="64MiB"
            Image="../inter_partition_sampling/build/x86_64-qemu/pok.elf">
            <Console Port="COM2"/>
        </VM>

        <VM Name="vm2" CPUs="2" Memory="32MiB"
            Image="../msections-wait/build/x86_64-qemu/pok.elf">
            <Console Port="COM3"/>
            <Gdb Port="COM4"/>
        </VM>
    </Hypervisor>

| Element / attribute  | Meaning |
|----------------------|---------|
| `Hypervisor/Console` | The hypervisor's own console (optional). |
| `VM`                 | One to eight VMs. |
| `VM/@Name`           | 1-30 characters (letters, digits, `_ . -`), unique (case is ignored). |
| `VM/@Image`          | The JetOS `pok.elf`, relative to the configuration file. Any x86_64 ELF works if it has a Multiboot header with load addresses, as JetOS has. |
| `VM/@Memory`         | Guest RAM in bytes, or with `KiB`, `MiB` or `GiB`. It must be a multiple of 2 MiB and at most 1 GiB. JetOS needs 16 MiB plus the memory of its partitions (they start at 16 MiB). |
| `VM/@CPUs`           | Physical CPUs reserved for the VM (default 1). |
| `VM/Console`         | The host serial port that becomes the guest's COM1, its console (optional; without it, the guest has no COM1). |
| `VM/Gdb`             | The host serial port that becomes the guest's COM2, where the JetOS GDB server listens (optional). |

The serial ports are QEMU's four ISA ports: COM1 (0x3F8), COM2 (0x2F8),
COM3 (0x3E8) and COM4 (0x2E8). Each one can be used at most once. There can
be at most 16 CPUs in all, and the VMs and the hypervisor must fit into
3 GiB of RAM. `hypervisor/config.xsd` is the schema. `hvconfig.py` checks
everything else, including the images: an arm64 image, or one that doesn't
fit into its `Memory`, is an error.

### What a VM sees

The hypervisor loads the image the way a Multiboot loader (QEMU `-kernel`)
does. It starts the image in 32-bit protected mode, with the Multiboot
information (memory sizes and a memory map) at 0x9000. Guest-physical
memory from 0 to `Memory` is RAM, and any access outside it stops the VM.

| I/O ports            | What the VM gets |
|----------------------|------------------|
| 0x20-0x21, 0xA0-0xA1 | The 8259A interrupt controller pair (emulated). |
| 0x40-0x43, 0x61      | The 8254 timer and port B (emulated). Channel 0 gives IRQ 0 at the programmed rate; channel 2 is used to calibrate the TSC. |
| 0x3F8-0x3FF (COM1)   | The VM's console port. Accesses are forwarded, or go straight to the port if it is the host's COM1. |
| 0x2F8-0x2FF (COM2)   | The VM's GDB port. Accesses are forwarded, or go straight to the port if it is the host's COM2. |
| 0x64, 0xCF9          | Writing 0xFE to 0x64, or a reset to 0xCF9, resets the VM: its image is loaded again and it restarts. |
| Other ports          | Read as all ones; writes are ignored. The hypervisor console reports the first access to each port. |

The VM has no local APIC, I/O APIC or PCI devices, so the `syspart-*`
examples don't run in a VM. RDTSC runs directly, and the VM's TSC starts at
0. CPUID returns the host's values, except that SVM and the APIC are
hidden, it reports one CPU and it sets the "hypervisor present" bit. MSRs
are emulated: EFER, PAT, the system call MSRs and the segment base MSRs
work, and all others read 0.

Each vCPU runs on its own physical CPU, so the VMs don't share CPU time.
JetOS uses one CPU. A VM's first vCPU runs the image; the others are parked
(reserved for a JetOS with SMP support).

A VM stops when it accesses memory outside its RAM, triple-faults, halts
with interrupts disabled, or runs an unsupported instruction. The
hypervisor then prints the reason and the VM's registers, and the other
VMs keep running.

### Debugging a VM

Each VM runs its own JetOS GDB server (see Debugging above) on its guest
COM2, which the VM's `Gdb` element connects to a host serial port. One GDB
can be connected to each VM that has a `Gdb` port, at the same time. When
GDB stops a VM, only that VM stops; the other VMs keep running.

1. Give the VM a GDB port in the configuration (`<Gdb Port="COM4"/>`).
   Give its image's schedule a GDB slot (`<Slot Type="GDB" .../>`), or
   build the image with `POK_NEEDS_WAIT_FOR_GDB` to make it wait for GDB at
   boot.

2. Build and run the hypervisor:

       make run-hv

   `qemu.sh` connects the VMs' GDB ports to TCP ports in VM order: 8100
   for the first VM that has a `Gdb` port, 8101 for the next one, and so
   on (`GDB_PORT=<port>` sets the first one). By default QEMU doesn't wait
   for GDB (`wait=off`). To attach before anything runs, replace the port
   with one that waits. QEMU then starts no CPU until GDB connects:

       HV_COM4=tcp:127.0.0.1:8100,server=on,wait=on \
           sh examples/hv-two-vms/build/qemu.sh

3. Connect GDB with the VM's image (its example directory, `pok.elf` and
   `GDB_commands`). For example, for vm2 of `hv-two-vms` (msections-wait,
   TCP port 8100), from the top-level directory, through the build image
   on macOS:

       docker run --rm -it --add-host=host.docker.internal:host-gateway \
           -v "$PWD:$PWD" -w "$PWD/examples/msections-wait" chpok-build \
           gdb-multiarch build/x86_64-qemu/pok.elf \
           -ex 'source build/x86_64-qemu/GDB_commands' \
           -ex 'target extended-remote host.docker.internal:8100'

   On a Linux host, run `gdb-multiarch` directly with
   `target extended-remote 127.0.0.1:8100`. Everything in Debugging above
   works the same way (inferiors per partition, breakpoints, `continue`
   rather than `run`, `detach`).

To debug several VMs at once, give each one a `Gdb` port and start one GDB
per TCP port. There are only four serial ports, so with two consoles and
two GDB ports, the hypervisor console has to go. For example:

    <Hypervisor>
        <VM Name="vm1" Memory="64MiB"
            Image="../inter_partition_sampling/build/x86_64-qemu/pok.elf">
            <Console Port="COM1"/>
            <Gdb Port="COM2"/>       <!-- TCP 8100 -->
        </VM>
        <VM Name="vm2" Memory="32MiB"
            Image="../msections-wait/build/x86_64-qemu/pok.elf">
            <Console Port="COM3"/>
            <Gdb Port="COM4"/>       <!-- TCP 8101 -->
        </VM>
    </Hypervisor>

A VM whose console is the host's COM1, or whose GDB port is the host's
COM2, uses that port directly. Otherwise each access to the port goes
through the hypervisor, which works but is slower. `make hv-test` runs
`tests/gdbserver/run_gdb_test.sh` against a VM this way
(`tests/hypervisor/gdb`).

The hypervisor itself is debugged with QEMU's gdb stub. Each physical CPU
is a GDB thread:

    sh examples/hv-two-vms/build/qemu.sh -s -S
    gdb-multiarch examples/hv-two-vms/build/hv.elf \
        -ex 'target remote 127.0.0.1:1234'

(From the build image: `target remote host.docker.internal:1234`.)

### Files

    hypervisor/config.xsd,      configuration schema and generator
    hvconfig.py,
    templates/hv_config.c.tpl
    hypervisor/SConstruct,      build
    hv.lds
    hypervisor/include/hv/      hv.h (data structures and interfaces between
                                the parts), svm.h (VMCB), x86.h
                                (instructions), config.h (the generated VM
                                table)
    hypervisor/src/entry.S      Multiboot entry, long mode, AP trampoline
    src/main.c, smp.c, acpi.c   start-up: CPUs (ACPI MADT), VMs, APs
    src/lapic.c, pit.c, idt.c,  physical timers and interrupts
    isr.S
    src/console.c, printf.c,    hypervisor console
    uart.c
    src/svm.c, svm_run.S        SVM: VMCB, run loop, exit handling
    src/vm.c, npt.c             VM RAM, image loading, nested page tables
    src/ioport.c, vpic.c,       the VMs' I/O ports and emulated devices
    vpit.c, vuart.c
    examples/hv-*/              hypervisor configurations
    tests/hypervisor/           hypervisor tests (make hv-test)

## Testing

The tests under `tests/` check the APEX services against ARINC 653 and
libpok's C library (`tests/libc`). They are built like the examples and run in QEMU on the host, so
`qemu-system-x86_64` / `qemu-system-aarch64` must be installed:

    make run                     # build and run all tests for x86_64
    make BSP=arm64-qemu run      # the same for arm64
    make run-mutex               # one test
    make run VERBOSE=0           # only the results, not the test output
    make hv-test                 # the hypervisor tests (x86_64)

See `tests/README` for what each test covers and how to add one.

## MISRA C:2025

The kernel, libpok, the architecture code, the boards, syspart and the
hypervisor follow MISRA C:2025. Where a guideline can't be followed, a
comment that starts with `MISRA C:2025` names it and gives the reason:

    grep -rn "MISRA C:2025" kernel libpok boards syspart misc hypervisor
