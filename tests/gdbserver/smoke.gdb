# Copyright (C) 2026 Kirill Brazhnikov <agamemnon886@gmail.com>
# Distributed under the 2-clause BSD license (see the top-level Makefile).
#
# GDB session of the GDB server test (see run_gdb_test.sh): connects to
# the GDB server at $GDB_TARGET, runs the checks of smoke.py, interrupts
# the system twice (run_gdb_test.sh sends SIGINT to GDB after each line
# GDB-TEST-INTERRUPT <n>) and detaches.

set pagination off
set confirm off
source build/x86_64-qemu/GDB_commands
source smoke.py

python connect()
python check_connection()
python check_breakpoints()
python check_registers()
python check_memory()
python check_watchpoint()
python check_breakpoints_both()
python check_breakpoint_hidden()
python check_time_stops()

# The output goes to a file: flush it, run_gdb_test.sh waits for the line.
python gdb.write("GDB-TEST-INTERRUPT 1\n"); gdb.flush()
continue
python check_interrupt()

# Into the polling loop of the GDB partition, where 'next' doesn't end.
next
next
next
python gdb.write("GDB-TEST-INTERRUPT 2\n"); gdb.flush()
next
python check_interrupt_step()

# GDB still takes the interrupted 'next' for in progress, and would
# restart it on detach: resume to a breakpoint first.
tbreak gdb_marker
continue

detach inferiors 2 3
inferior 1
detach
echo GDB-TEST-END\n
