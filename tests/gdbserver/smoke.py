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

# Checks of the GDB server, run in GDB by smoke.gdb (see run_gdb_test.sh).
# Each check prints "GDB-TEST <name>: PASS" or "GDB-TEST <name>: FAIL".
#
# Modified by Kirill Brazhnikov, 2026-11-05: the register checks depend on
# the architecture (x86-64 or AArch64); check_kernel_hw() is new; the
# registers of another process of the stopped partition are checked.

import os
import time

import gdb


# Signal of the last stop (SignalEvent), e.g. "SIGINT".
last_stop = {"signal": None}


def on_stop(event):
    last_stop["signal"] = getattr(event, "stop_signal", None)


gdb.events.stop.connect(on_stop)


def check(name, ok, detail=""):
    if ok:
        print("GDB-TEST %s: PASS" % name)
    else:
        print("GDB-TEST %s: FAIL %s" % (name, detail))


def run(command):
    return gdb.execute(command, to_string=True)


def frame_function():
    function = gdb.selected_frame().function()
    return function.name if function is not None else None


def counter():
    return int(gdb.parse_and_eval("gdb_counter"))


def connect():
    gdb.execute("target extended-remote " + os.environ["GDB_TARGET"])


def check_connection():
    pids = sorted(inferior.pid for inferior in gdb.inferiors())
    check("attached to the kernel and both partitions", pids == [1, 2, 3],
          str(pids))
    threads = run("info threads")
    check("kernel threads listed", "GDB.kernel_thread" in threads, threads)
    check("P1 processes listed", "P1.worker" in threads, threads)
    check("P2 processes listed", "P2.worker" in threads, threads)


def check_breakpoints():
    # The same code is at the same address in both partitions: the
    # breakpoint is inserted only into P1 (inferior 2).
    gdb.execute("break gdb_marker")
    breakpoint = gdb.breakpoints()[-1]
    for location in breakpoint.locations:
        if 3 in location.thread_groups:
            location.enabled = False

    previous = None
    for i in range(3):
        gdb.execute("continue")
        inferior = gdb.selected_inferior().num
        check("breakpoint %d stops P1" % i, inferior == 2,
              "inferior %d" % inferior)
        check("breakpoint %d in gdb_marker" % i,
              frame_function() == "gdb_marker", str(frame_function()))
        n = counter()
        if previous is not None:
            check("breakpoint %d: next period" % i, n == previous + 1,
                  "%d after %d" % (n, previous))
        previous = n

    check("backtrace", "worker" in run("bt"))

    # Another process of the partition which has stopped shows its own
    # registers: P1's main process waits in a system call.
    threads = run("info threads")
    main = [line for line in threads.splitlines() if "P1.main" in line]
    check("other thread of the stopped partition",
          len(main) == 1 and "lja_do_syscall" in main[0], threads)

    pc = int(gdb.parse_and_eval("$pc"))
    gdb.execute("stepi")
    check("stepi", int(gdb.parse_and_eval("$pc")) != pc)
    gdb.execute("finish")
    check("finish returns to worker", frame_function() == "worker",
          str(frame_function()))
    check("gdb_marker has written the counter", counter() == previous + 1,
          "%d after %d" % (counter(), previous))


def architecture():
    return gdb.selected_inferior().architecture().name()


def check_registers():
    if architecture() == "aarch64":
        check_registers_aarch64()
    else:
        check_registers_x86_64()


def check_registers_x86_64():
    rip = int(gdb.parse_and_eval("$rip"))
    check("rip is the PC", rip == int(gdb.parse_and_eval("$pc")))
    check("user code segment", int(gdb.parse_and_eval("$cs")) & 3 == 3)
    check("fctrl", int(gdb.parse_and_eval("$fctrl")) == 0x37f,
          run("p/x $fctrl"))
    check("mxcsr", int(gdb.parse_and_eval("$mxcsr")) & 0xffbf == 0x1f80,
          run("p/x $mxcsr"))
    check("xmm0 available", "unavailable" not in run("p $xmm0"))
    check("st0 available", "unavailable" not in run("p $st0"))
    check("ds is not available", "unavailable" in run("p $ds"))

    # Write a register and read it back.
    gdb.execute("set var $r12 = 0x1234567890")
    check("write r12", int(gdb.parse_and_eval("$r12")) == 0x1234567890)


def check_registers_aarch64():
    # The partitions' memory starts at 0x100000000 (kernel/arch/arm64).
    check("EL0", int(gdb.parse_and_eval("$cpsr")) & 0x1f == 0,
          run("p/x $cpsr"))
    sp = int(gdb.parse_and_eval("$sp"))
    check("user stack", sp >> 32 == 1, "%#x" % sp)
    check("v0 available", "unavailable" not in run("p $v0"))
    check("v31 available", "unavailable" not in run("p $v31"))
    check("fpcr available", "unavailable" not in run("p $fpcr"))
    check("fpsr available", "unavailable" not in run("p $fpsr"))

    # Write a register and read it back (and put the old value back: the
    # thread goes on).
    old = int(gdb.parse_and_eval("$x19"))
    gdb.execute("set var $x19 = 0x1234567890")
    check("write x19", int(gdb.parse_and_eval("$x19")) == 0x1234567890)
    gdb.execute("set var $x19 = %d" % old)

    # A thread of the kernel has no FP/SIMD registers saved.
    thread = gdb.selected_thread()
    gdb.execute("thread 1.2")
    check("kernel thread: no v0", "unavailable" in run("p $v0"))
    thread.switch()


def check_memory():
    gdb.execute("set var gdb_poke = 42")
    check("write memory", int(gdb.parse_and_eval("gdb_poke")) == 42)


def check_watchpoint():
    # P2 (inferior 3) writes gdb_counter at the same address as P1: the
    # hardware watchpoint stops only P2.
    gdb.execute("delete")
    gdb.execute("inferior 3")
    output = run("watch gdb_counter")
    check("hardware watchpoint", "Hardware watchpoint" in output, output)
    for i in range(2):
        old = counter()
        gdb.execute("continue")
        inferior = gdb.selected_inferior().num
        check("watchpoint %d stops P2" % i, inferior == 3,
              "inferior %d" % inferior)
        check("watchpoint %d: new value" % i, counter() == old + 1,
              "%d after %d" % (counter(), old))
    gdb.execute("delete")


def check_breakpoints_both():
    # Breakpoints in both partitions: to continue, GDB steps the stopped
    # thread over its breakpoint and expects no other thread to run (and
    # hit the other partition's breakpoint) before the step is done.
    gdb.execute("break gdb_marker")
    seen = set()
    for i in range(6):
        gdb.execute("continue")
        seen.add(gdb.selected_inferior().num)
        check("breakpoint in both partitions %d" % i,
              frame_function() == "gdb_marker", str(frame_function()))
    check("both partitions stop", seen == {2, 3}, str(seen))
    gdb.execute("delete")


def check_breakpoint_hidden():
    # GDB doesn't keep the code under breakpoints inserted with Z0: the
    # server must hide its breakpoint instructions from memory reads (or
    # GDB takes them for the program's own and skips them).
    gdb.execute("inferior 2")
    inferior = gdb.selected_inferior()
    address = int(gdb.parse_and_eval("&gdb_marker"))
    code = bytes(inferior.read_memory(address, 4))
    gdb.execute("set breakpoint always-inserted on")
    gdb.execute("break *%#x" % address)
    seen = bytes(inferior.read_memory(address, 4))
    check("breakpoint hidden from memory reads", seen == code,
          "%s, not %s" % (seen.hex(), code.hex()))
    # Writing over a breakpoint keeps it.
    inferior.write_memory(address, code)
    gdb.execute("continue")
    pc = int(gdb.parse_and_eval("$pc"))
    check("breakpoint kept after a write", pc == address,
          "%#x, not %#x" % (pc, address))
    check("breakpoint after a write stops P1",
          gdb.selected_inferior().num == 2)
    gdb.execute("delete")
    gdb.execute("set breakpoint always-inserted off")


def kernel_function():
    sal = gdb.selected_frame().find_sal()
    return sal.symtab.filename if sal.symtab is not None else None


def check_kernel_hw():
    # Hardware breakpoints and watchpoints work in the kernel too (they are
    # in the debug registers in any partition). The stop is reported for
    # the thread of the partition that is running. (Thread 1.2 is the GDB
    # partition's: the Monitor's, 1.1, has no registers, and GDB can't
    # resume from a thread without them.)
    gdb.execute("thread 1.2")
    output = run("hbreak jet_on_tick")
    check("hardware breakpoint in the kernel", "Hardware assisted" in output,
          output)
    for i in range(2):
        gdb.execute("continue")
        check("kernel hbreak %d stops in jet_on_tick" % i,
              frame_function() == "jet_on_tick", str(frame_function()))
    gdb.execute("delete")

    output = run("watch current_partition")
    check("hardware watchpoint in the kernel", "Hardware watchpoint" in output,
          output)
    for i in range(2):
        old = int(gdb.parse_and_eval("current_partition"))
        gdb.execute("continue")
        new = int(gdb.parse_and_eval("current_partition"))
        where = kernel_function()
        check("kernel watchpoint %d: value changed" % i, new != old,
              "%#x, was %#x" % (new, old))
        check("kernel watchpoint %d stops in the kernel" % i,
              where is not None and "/kernel/" in where, str(where))
    gdb.execute("delete")


def check_time_stops():
    # The system time doesn't run while GDB holds the system: two periods
    # of P1 (100 ms apart) are about as far apart if the system is held for
    # 3 s between them.
    gdb.execute("break gdb_marker")
    breakpoint = gdb.breakpoints()[-1]
    for location in breakpoint.locations:
        if 3 in location.thread_groups:
            location.enabled = False
    gdb.execute("continue")
    before = int(gdb.parse_and_eval("gdb_time"))
    time.sleep(3)
    gdb.execute("continue")
    after = int(gdb.parse_and_eval("gdb_time"))
    check("system time stopped while held",
          gdb.selected_inferior().num == 2 and after - before < 2000000000,
          "%d ms between two periods" % ((after - before) // 1000000))
    gdb.execute("delete")


def check_interrupt():
    check("interrupt: SIGINT", last_stop["signal"] == "SIGINT",
          str(last_stop["signal"]))
    inferior = gdb.selected_inferior().num
    check("interrupt stops in the GDB partition", inferior == 1,
          "inferior %d" % inferior)
    # The selected thread is marked with '*' in the list.
    threads = run("info threads")
    selected = [line for line in threads.splitlines()
                if line.startswith("*")]
    check("interrupt: GDB thread",
          len(selected) == 1 and "GDB.kernel_thread" in selected[0], threads)


def check_interrupt_step():
    # 'next' in the polling loop of the GDB partition doesn't end by
    # itself: Ctrl-C, which comes while the system is stopped between the
    # steps, has to stop it.
    check("interrupt during next: SIGINT", last_stop["signal"] == "SIGINT",
          str(last_stop["signal"]))
    check("interrupt during next: GDB partition",
          gdb.selected_inferior().num == 1)
