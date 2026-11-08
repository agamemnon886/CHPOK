#!/usr/bin/env python3
#
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
# Modified by Kirill Brazhnikov, 2026-11-08: --bsp selects the
# architecture (x86_64-qemu or arm64-qemu); {bsp} in an image path is
# replaced by it. arm64: the images' ELF segments are loaded, each VM gets
# a device tree (dtbN.bin), COM1-COM16 are the host's PL011s and PCI
# 16550s, and qemu.sh runs qemu-system-aarch64.
#
# Hypervisor configuration generator.
#
#   hvconfig.py [--bsp BSP] CONFIG.xml OUTDIR
#                                   check the configuration and its images
#                                   and write, into OUTDIR:
#       hv_config.c   the VM table (templates/hv_config.c.tpl)
#       hv_images.S   the images' loadable bytes (.incbin of imageN.bin),
#                     and on arm64 the VMs' device trees (dtbN.bin)
#       imageN.bin    what a loader would load of VM N's image
#       dtbN.bin      (arm64) the device tree of VM N
#       qemu.sh       the QEMU command that runs the hypervisor
#   hvconfig.py [--bsp BSP] --projects CONFIG.xml
#                                   print the project directories that
#                                   build the images (the part of each
#                                   image path before its last /build/)
#
# BSP is x86_64-qemu (default) or arm64-qemu; the images must be built for
# it. {bsp} in an Image attribute is replaced by BSP, so that one
# configuration serves both.
#
# x86_64: the images are loaded like QEMU's Multiboot loader does it: the
# Multiboot header's a.out kludge addresses give the bytes of the file to
# load, where, the zeroed area after them and the entry point.
#
# arm64: the images are loaded like QEMU's ELF loader does it: the PT_LOAD
# segments at their physical addresses; the guest RAM starts at
# 0x40000000 (QEMU 'virt'), where the VM's device tree is put.

import os
import struct
import sys

from lxml import etree
import jinja2

HERE = os.path.dirname(os.path.abspath(__file__))

MAX_VMS = 8
LARGE_PAGE = 2 << 20
MAX_VM_MEMORY = 1 << 30
MAX_QEMU_MEMORY = 3 << 30       # x86_64: RAM must stay below the 4th GiB.
HIGH_MEMORY = 1 << 20

MULTIBOOT_MAGIC = 0x1BADB002
MULTIBOOT_SEARCH = 8192
MULTIBOOT_AOUT_KLUDGE = 1 << 16
MULTIBOOT_VIDEO_MODE = 1 << 2
EM_X86_64 = 62
EM_AARCH64 = 183
PT_LOAD = 1

# arm64 guests (QEMU 'virt' memory map).
ARM64_RAM_BASE = 0x40000000
ARM64_DTB_SPACE = 64 << 10      # The device tree's room at the RAM base
ARM64_GIC_DIST = 0x08000000
ARM64_GIC_CPU = 0x08010000
ARM64_UART0 = 0x09000000
ARM64_RTC = 0x09010000
ARM64_UART1 = 0x09040000
ARM64_PCI_SERIAL_SLOT = 0x10    # PCI slot of COM3; COMn is at 0x10 + n - 3

DEFAULT_GDB_TCP_PORT = 8100


class ConfigError(Exception):
    pass


class Target(object):
    """What differs between the BSPs."""

    def __init__(self, bsp, machine, arch, ports, max_cpus):
        self.bsp = bsp
        self.machine = machine
        self.arch = arch
        self.ports = ports          # COM port names, in order
        self.max_cpus = max_cpus


TARGETS = {
    'x86_64-qemu': Target('x86_64-qemu', EM_X86_64, 'x86_64',
                          ['COM%d' % i for i in range(1, 5)], 16),
    # GICv2 has at most 8 CPUs; COM1/COM2 are the PL011s, COM3-COM16
    # PCI 16550s.
    'arm64-qemu': Target('arm64-qemu', EM_AARCH64, 'arm64',
                         ['COM%d' % i for i in range(1, 17)], 8),
}


def name_key(name):
    """ARINC 653-style names are compared case-insensitively."""
    return name.lower()


def parse_size(text):
    text = text.strip()
    units = [('GiB', 1 << 30), ('MiB', 1 << 20), ('KiB', 1 << 10),
             ('GB', 1 << 30), ('MB', 1 << 20), ('KB', 1 << 10)]
    for suffix, factor in units:
        if text.endswith(suffix):
            return int(text[:-len(suffix)].strip()) * factor
    return int(text)


def port_name(element):
    return element.get('Port').upper()


class VM(object):
    def __init__(self, index, element, config_dir, bsp):
        self.index = index
        self.name = element.get('Name')
        self.image = os.path.normpath(os.path.join(
            config_dir, element.get('Image').replace('{bsp}', bsp)))
        self.memory = parse_size(element.get('Memory'))
        self.cpus = int(element.get('CPUs', '1'))
        console = element.find('Console')
        gdb = element.find('Gdb')
        self.console = port_name(console) if console is not None else None
        self.gdb = port_name(gdb) if gdb is not None else None
        self.data = b''
        self.load_addr = 0
        self.bss_end = 0
        self.entry = 0
        self.dtb = None

    def where(self):
        return 'VM %s' % self.name


class Config(object):
    def __init__(self, path, bsp='x86_64-qemu'):
        if bsp not in TARGETS:
            raise ConfigError('unknown BSP %s (x86_64-qemu or arm64-qemu)'
                              % bsp)
        self.target = TARGETS[bsp]
        self.path = os.path.abspath(path)
        self.dir = os.path.dirname(self.path)
        try:
            schema = etree.XMLSchema(etree.parse(os.path.join(HERE,
                                                              'config.xsd')))
            tree = etree.parse(self.path)
        except (OSError, etree.XMLSyntaxError) as e:
            raise ConfigError(str(e))
        if not schema.validate(tree):
            raise ConfigError('\n'.join(str(e) for e in schema.error_log))
        root = tree.getroot()
        console = root.find('Console')
        self.console = port_name(console) if console is not None else None
        self.vms = [VM(i, e, self.dir, bsp)
                    for i, e in enumerate(root.findall('VM'))]

    def check(self):
        target = self.target
        if len(self.vms) > MAX_VMS:
            raise ConfigError('at most %d VMs' % MAX_VMS)
        names = {}
        for vm in self.vms:
            key = name_key(vm.name)
            if key in names:
                raise ConfigError('VM name %s is used twice' % vm.name)
            names[key] = vm
        users = {}
        uses = [(self.console, 'the hypervisor console')]
        for vm in self.vms:
            uses += [(vm.console, 'the console of VM %s' % vm.name),
                     (vm.gdb, 'the GDB port of VM %s' % vm.name)]
        for port, use in uses:
            if port is None:
                continue
            if port not in target.ports:
                raise ConfigError('%s (%s): %s has %s to %s' %
                                  (port, use, target.bsp, target.ports[0],
                                   target.ports[-1]))
            if port in users:
                raise ConfigError('%s is used by %s and by %s'
                                  % (port, users[port], use))
            users[port] = use
        total_cpus = sum(vm.cpus for vm in self.vms)
        if total_cpus > target.max_cpus:
            raise ConfigError('the VMs have %d CPUs, at most %d are '
                              'supported' % (total_cpus, target.max_cpus))
        for vm in self.vms:
            if vm.memory % LARGE_PAGE != 0 or vm.memory == 0:
                raise ConfigError('%s: Memory must be a multiple of 2 MiB'
                                  % vm.where())
            if vm.memory > MAX_VM_MEMORY:
                raise ConfigError('%s: Memory is at most 1 GiB'
                                  % vm.where())
            data = read_image(vm, target)
            if target.arch == 'x86_64':
                load_multiboot(vm, data)
            else:
                load_elf(vm, data)
                vm.dtb = vm_device_tree(vm)
        if self.qemu_memory() > MAX_QEMU_MEMORY:
            raise ConfigError('the VMs need more than 3 GiB of RAM')

    def qemu_memory(self):
        """QEMU -m in bytes: the hypervisor with its images, the VMs and
        some room, rounded up to 16 MiB."""
        images = sum(len(vm.data) + len(vm.dtb or b'') for vm in self.vms)
        total = HIGH_MEMORY + images + (4 << 20) + LARGE_PAGE
        total += sum(vm.memory for vm in self.vms) + (16 << 20)
        return (total + (16 << 20) - 1) // (16 << 20) * (16 << 20)


def read_image(vm, target):
    """Read the image of a VM: an ELF64 file for the target."""
    try:
        with open(vm.image, 'rb') as f:
            data = f.read()
    except OSError as e:
        raise ConfigError('%s: %s' % (vm.where(), e))
    if len(data) < 64 or data[:4] != b'\x7fELF':
        raise ConfigError('%s: %s is not an ELF file'
                          % (vm.where(), vm.image))
    machine = struct.unpack_from('<H', data, 18)[0]
    if data[4] != 2 or data[5] != 1 or machine != target.machine:
        raise ConfigError('%s: %s is not an %s image (build it for %s)'
                          % (vm.where(), vm.image, target.arch, target.bsp))
    return data


def load_multiboot(vm, data):
    """x86_64: keep the bytes a Multiboot loader loads (a.out kludge)."""
    header = None
    for offset in range(0, min(len(data), MULTIBOOT_SEARCH) - 11, 4):
        magic, flags, checksum = struct.unpack_from('<III', data, offset)
        if magic == MULTIBOOT_MAGIC and \
                (magic + flags + checksum) & 0xFFFFFFFF == 0:
            header = offset
            break
    if header is None:
        raise ConfigError('%s: %s has no Multiboot header'
                          % (vm.where(), vm.image))
    if flags & MULTIBOOT_AOUT_KLUDGE == 0 or header + 32 > len(data):
        raise ConfigError('%s: the Multiboot header of %s gives no load '
                          'addresses' % (vm.where(), vm.image))
    if flags & MULTIBOOT_VIDEO_MODE:
        print('hvconfig.py: %s: the requested video mode is ignored'
              % vm.where(), file=sys.stderr)
    header_addr, load_addr, load_end, bss_end, entry = \
        struct.unpack_from('<IIIII', data, header + 12)

    file_offset = header - (header_addr - load_addr)
    if load_end == 0:
        load_end = load_addr + len(data) - file_offset
    if bss_end == 0:
        bss_end = load_end
    if file_offset < 0 or load_end < load_addr or \
            file_offset + (load_end - load_addr) > len(data):
        raise ConfigError('%s: bad Multiboot load addresses in %s'
                          % (vm.where(), vm.image))
    if load_addr < HIGH_MEMORY:
        raise ConfigError('%s: the image is loaded below 1 MiB'
                          % vm.where())
    if bss_end < load_end:
        raise ConfigError('%s: bad Multiboot BSS end in %s'
                          % (vm.where(), vm.image))
    check_fits(vm, 0, bss_end)
    if not load_addr <= entry < load_end:
        raise ConfigError('%s: the entry point is outside the image'
                          % vm.where())
    vm.data = data[file_offset:file_offset + (load_end - load_addr)]
    vm.load_addr = load_addr
    vm.bss_end = bss_end
    vm.entry = entry


def check_fits(vm, ram_base, end):
    """The image, up to end, must fit into the VM's RAM."""
    if end > ram_base + vm.memory:
        raise ConfigError('%s: the image needs %d MiB of RAM, Memory is %d '
                          'MiB' % (vm.where(),
                                   (end - ram_base + (1 << 20) - 1) >> 20,
                                   vm.memory >> 20))


def load_elf(vm, data):
    """arm64: the PT_LOAD segments, as flat bytes from the lowest one."""
    entry, phoff = struct.unpack_from('<QQ', data, 24)
    phentsize, phnum = struct.unpack_from('<HH', data, 54)
    segments = []
    for i in range(phnum):
        offset = phoff + i * phentsize
        if offset + 56 > len(data):
            raise ConfigError('%s: bad program headers in %s'
                              % (vm.where(), vm.image))
        p_type, _, p_offset, _, p_paddr, p_filesz, p_memsz = \
            struct.unpack_from('<IIQQQQQ', data, offset)
        if p_type != PT_LOAD or p_memsz == 0:
            continue
        if p_filesz > p_memsz or p_offset + p_filesz > len(data):
            raise ConfigError('%s: bad segment in %s'
                              % (vm.where(), vm.image))
        segments.append((p_paddr, p_offset, p_filesz, p_memsz))
    if not segments:
        raise ConfigError('%s: %s has nothing to load'
                          % (vm.where(), vm.image))
    load_addr = min(s[0] for s in segments)
    load_end = max(s[0] + s[2] for s in segments)
    bss_end = max(s[0] + s[3] for s in segments)
    if load_addr < ARM64_RAM_BASE + ARM64_DTB_SPACE:
        raise ConfigError('%s: the image is loaded below 0x%X (RAM and '
                          'device tree)' % (vm.where(),
                                            ARM64_RAM_BASE + ARM64_DTB_SPACE))
    check_fits(vm, ARM64_RAM_BASE, bss_end)
    if not load_addr <= entry < load_end:
        raise ConfigError('%s: the entry point is outside the image'
                          % vm.where())
    flat = bytearray(load_end - load_addr)
    for paddr, offset, filesz, _ in segments:
        start = paddr - load_addr
        flat[start:start + filesz] = data[offset:offset + filesz]
    vm.data = bytes(flat)
    vm.load_addr = load_addr
    vm.bss_end = bss_end
    vm.entry = entry


class DeviceTree(object):
    """A flattened device tree (version 17) built node by node."""

    def __init__(self):
        self.struct = bytearray()
        self.strings = bytearray()
        self.offsets = {}

    @staticmethod
    def pad(data):
        return data + b'\0' * (-len(data) % 4)

    def begin(self, name):
        self.struct += struct.pack('>I', 1) + self.pad(name.encode() + b'\0')

    def end(self):
        self.struct += struct.pack('>I', 2)

    def prop(self, name, value=b''):
        if name not in self.offsets:
            self.offsets[name] = len(self.strings)
            self.strings += name.encode() + b'\0'
        if isinstance(value, str):
            value = value.encode() + b'\0'
        elif isinstance(value, (list, tuple)):
            if value and isinstance(value[0], str):
                value = b''.join(v.encode() + b'\0' for v in value)
            else:
                value = b''.join(struct.pack('>I', v) for v in value)
        self.struct += struct.pack('>III', 3, len(value),
                                   self.offsets[name]) + self.pad(value)

    def blob(self):
        body = self.struct + struct.pack('>I', 9)
        header_size = 40
        rsvmap = b'\0' * 16
        off_rsvmap = header_size
        off_struct = off_rsvmap + len(rsvmap)
        off_strings = off_struct + len(body)
        total = off_strings + len(self.strings)
        header = struct.pack('>10I', 0xD00DFEED, total, off_struct,
                             off_strings, off_rsvmap, 17, 16, 0,
                             len(self.strings), len(body))
        return bytes(header + rsvmap + body + self.strings)


def reg(address, size):
    """reg value with #address-cells = #size-cells = 2."""
    return [address >> 32, address & 0xFFFFFFFF, size >> 32,
            size & 0xFFFFFFFF]


def vm_device_tree(vm):
    """arm64: the device tree of a VM (what it sees, README.md)."""
    gic = 1                     # phandle of the interrupt controller
    level = 0x104               # PPI on CPU 0, level-sensitive, high
    t = DeviceTree()
    t.begin('')
    t.prop('#address-cells', [2])
    t.prop('#size-cells', [2])
    t.prop('compatible', 'linux,dummy-virt')
    t.prop('model', 'JetOS hypervisor VM')
    t.prop('interrupt-parent', [gic])

    t.begin('chosen')
    t.prop('stdout-path', '/pl011@%x' % ARM64_UART0)
    t.end()

    t.begin('memory@%x' % ARM64_RAM_BASE)
    t.prop('device_type', 'memory')
    t.prop('reg', reg(ARM64_RAM_BASE, vm.memory))
    t.end()

    t.begin('cpus')
    t.prop('#address-cells', [1])
    t.prop('#size-cells', [0])
    t.begin('cpu@0')
    t.prop('device_type', 'cpu')
    t.prop('compatible', 'arm,cortex-a53')
    t.prop('reg', [0])
    t.prop('enable-method', 'psci')
    t.end()
    t.end()

    t.begin('psci')
    t.prop('compatible', ['arm,psci-1.0', 'arm,psci-0.2'])
    t.prop('method', 'hvc')
    t.end()

    t.begin('intc@%x' % ARM64_GIC_DIST)
    t.prop('compatible', 'arm,cortex-a15-gic')
    t.prop('#interrupt-cells', [3])
    t.prop('interrupt-controller')
    t.prop('reg', reg(ARM64_GIC_DIST, 0x1000) + reg(ARM64_GIC_CPU, 0x2000))
    t.prop('phandle', [gic])
    t.end()

    # Secure, non-secure physical, virtual and hypervisor timer PPIs.
    t.begin('timer')
    t.prop('compatible', 'arm,armv8-timer')
    t.prop('interrupts', [1, 13, level, 1, 14, level, 1, 11, level,
                          1, 10, level])
    t.prop('always-on')
    t.end()

    uarts = [ARM64_UART0] + ([ARM64_UART1] if vm.gdb is not None else [])
    for address in uarts:
        t.begin('pl011@%x' % address)
        t.prop('compatible', ['arm,pl011', 'arm,primecell'])
        t.prop('reg', reg(address, 0x1000))
        t.end()

    t.begin('pl031@%x' % ARM64_RTC)
    t.prop('compatible', ['arm,pl031', 'arm,primecell'])
    t.prop('reg', reg(ARM64_RTC, 0x1000))
    t.end()

    t.end()
    data = t.blob()
    if len(data) > ARM64_DTB_SPACE:
        raise ConfigError('%s: the device tree is too large' % vm.where())
    return data


def write_if_changed(path, text, mode=None):
    old = None
    if os.path.exists(path):
        with open(path, 'rb') as f:
            old = f.read()
    data = text if isinstance(text, bytes) else text.encode()
    if old != data:
        with open(path, 'wb') as f:
            f.write(data)
    if mode is not None:
        os.chmod(path, mode)


def serial_uses(config):
    """What each used COM port is for, and its default QEMU -serial."""
    uses = {}
    if config.console is not None:
        uses[config.console] = ('the hypervisor console', 'stdio')
    gdb_index = 0
    for vm in config.vms:
        if vm.console is not None:
            uses[vm.console] = ('the console of %s' % vm.name,
                                'file:$dir/%s.log' % vm.name)
        if vm.gdb is not None:
            uses[vm.gdb] = ('the GDB server of %s' % vm.name,
                            'tcp:127.0.0.1:$((GDB_PORT + %d)),server=on,'
                            'wait=off' % gdb_index)
            gdb_index += 1
    return uses


def qemu_command(config):
    """Comment and option lines of qemu.sh, and the QEMU command. Each COM
    port can be redirected with HV_COM1..HV_COMn (QEMU -serial syntax)."""
    ports = config.target.ports
    uses = serial_uses(config)
    last = max([ports.index(p) for p in uses] + [0])
    if config.target.arch == 'arm64':
        last = max(last, 1)     # COM1 and COM2 are the two PL011s.
    lines = []
    serial = []
    for i in range(last + 1):
        port = ports[i]
        use, default = uses.get(port, ('unused', 'null'))
        lines.append('# %s: %s (default %s)' % (port, use, default))
        option = '    -serial "${HV_%s:-%s}"' % (port, default)
        if config.target.arch == 'arm64' and i >= 2:
            # COM3 and up: a 16550 on the PCI bus, in a fixed slot.
            option += ' -device pci-serial,chardev=serial%d,addr=0x%x' \
                % (i, ARM64_PCI_SERIAL_SLOT + i - 2)
        serial.append(option + ' \\')
    cpus = sum(vm.cpus for vm in config.vms)
    memory = config.qemu_memory() >> 20
    if config.target.arch == 'arm64':
        qemu = ['exec ${QEMU:-qemu-system-aarch64} '
                '-machine virt,virtualization=on,gic-version=2 \\',
                '    -cpu cortex-a53 -smp %d -m %d -display none '
                '-nodefaults \\' % (cpus, memory)]
    else:
        qemu = ['exec ${QEMU:-qemu-system-x86_64} -machine pc '
                '-cpu qemu64,+svm,+npt \\',
                '    -smp %d -m %d -display none -vga none -nodefaults \\'
                % (cpus, memory)]
    return lines, qemu + serial + ['    -kernel "$dir/hv.elf" "$@"']


def generate(config, outdir):
    os.makedirs(outdir, exist_ok=True)
    for vm in config.vms:
        vm.bin = os.path.join(outdir, 'image%d.bin' % vm.index)
        write_if_changed(vm.bin, vm.data)
        if vm.dtb is not None:
            vm.dtb_bin = os.path.join(outdir, 'dtb%d.bin' % vm.index)
            write_if_changed(vm.dtb_bin, vm.dtb)

    env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(os.path.join(HERE, 'templates')),
        keep_trailing_newline=True)
    text = env.get_template('hv_config.c.tpl').render(
        config=config, source=os.path.relpath(config.path, HERE))
    write_if_changed(os.path.join(outdir, 'hv_config.c'), text)

    images = ['/*', ' * GENERATED! DO NOT MODIFY!', ' *',
              ' * Loadable bytes of the VM images (hvconfig.py, from',
              ' * %s).' % os.path.relpath(config.path, HERE), ' */', '',
              '  .section .rodata.images, "a"']

    def blob(name, path):
        return ['', '  .balign 16', '  .globl %s' % name,
                '%s:' % name, '  .incbin "%s"' % path,
                '%s_end:' % name, '  .balign 8',
                '  .globl %s_size' % name, '%s_size:' % name,
                '  .quad %s_end - %s' % (name, name)]
    for vm in config.vms:
        images += blob('hv_image_%d' % vm.index, vm.bin)
        if vm.dtb is not None:
            images += blob('hv_dtb_%d' % vm.index, vm.dtb_bin)
    write_if_changed(os.path.join(outdir, 'hv_images.S'),
                     '\n'.join(images) + '\n')

    lines, command = qemu_command(config)
    script = ['#!/bin/sh', '#', '# GENERATED! DO NOT MODIFY!', '#',
              '# Runs the hypervisor of %s (%s) in QEMU'
              % (os.path.relpath(config.path, HERE), config.target.bsp),
              '# (hvconfig.py). Serial ports, in COM order:'] + lines + [
              '# HV_COM1..HV_COM%d replace a port\'s QEMU -serial value;'
              % len(lines),
              '# GDB_PORT is the first GDB server\'s TCP port. Further',
              '# arguments are passed to QEMU.', '',
              'dir=$(cd "$(dirname "$0")" && pwd)',
              'GDB_PORT=${GDB_PORT:-%d}' % DEFAULT_GDB_TCP_PORT, ''] \
        + command
    write_if_changed(os.path.join(outdir, 'qemu.sh'),
                     '\n'.join(script) + '\n', 0o755)


def image_paths(path, bsp='x86_64-qemu'):
    """Image files of a configuration (for build dependencies)."""
    return [vm.image for vm in Config(path, bsp).vms]


def main(argv):
    args = argv[1:]
    bsp = 'x86_64-qemu'
    if len(args) >= 2 and args[0] == '--bsp':
        bsp = args[1]
        args = args[2:]
    try:
        if len(args) == 2 and args[0] == '--projects':
            for image in image_paths(args[1], bsp):
                marker = os.sep + 'build' + os.sep
                if marker in image:
                    print(image[:image.rindex(marker)])
            return 0
        if len(args) != 2 or args[0].startswith('-'):
            print('usage: hvconfig.py [--bsp BSP] CONFIG.xml OUTDIR\n'
                  '       hvconfig.py [--bsp BSP] --projects CONFIG.xml\n'
                  '       (BSP: x86_64-qemu, the default, or arm64-qemu)',
                  file=sys.stderr)
            return 2
        config = Config(args[0], bsp)
        config.check()
        generate(config, args[1])
    except ConfigError as e:
        print('hvconfig.py: %s: %s' % (args[-1] if args[0] == '--projects'
                                       else args[0], e), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
