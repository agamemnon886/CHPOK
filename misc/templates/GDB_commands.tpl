{# Modified by Kirill Brazhnikov, 2026-11-04: rewritten for the GDB server
   ported to x86_64. The old commands used structures which JetOS no longer
   has; now GDB attaches to every partition after connecting, each with the
   symbols of its part.elf (in Python, so that a partition without
   registers doesn't stop it). #}
# Commands for GDB connected to the JetOS GDB server (see README.md):
#
#   gdb-multiarch build/<board>/pok.elf \
#       -ex 'source build/<board>/GDB_commands' \
#       -ex 'target extended-remote 127.0.0.1:8000'
#
# Inferior 1 (process 1) is the kernel; its threads are the kernel-only
# partitions (Monitor, GDB). Inferior N+2 (process N+2) is partition N;
# its threads are the partition's processes, the last one is IDLE.
#
# A breakpoint is inserted in every inferior which has its location;
# GDB 14 and later can restrict it to one: break main inferior 2.

set remotetimeout 10
# Resuming always resumes the whole module.
set schedule-multiple on

# After connecting, attach to every partition (process N+2 for partition N)
# in a new inferior with the symbols of its part.elf, then go back to the
# thread which has stopped: GDB resumes the system from it. A partition
# which hasn't run yet (POK_NEEDS_WAIT_FOR_GDB) has no registers, so GDB
# can't show where it is: that error doesn't stop the attaching.
define target hookpost-extended-remote
python
stopped = gdb.selected_thread()
partitions = [
{% for part_build_dir in partition_build_dirs %}
    ({{loop.index0 + 2}}, "{{conf.partitions[loop.index0].name}}",
     "{{part_build_dir}}part.elf"),
{% endfor %}
]
for pid, name, elf in partitions:
    known = set(inferior.num for inferior in gdb.inferiors())
    gdb.execute("add-inferior -exec " + elf)
    added = [i for i in gdb.inferiors() if i.num not in known][0]
    gdb.execute("inferior %d" % added.num)
    try:
        gdb.execute("attach %d" % pid)
    except gdb.error as e:
        print("Partition %s: %s" % (name, e))
stopped.switch()
gdb.execute("frame")
end
end
