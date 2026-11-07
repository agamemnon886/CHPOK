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
# Hypervisor configuration generator.
#
#   hvconfig.py CONFIG.xml OUTDIR   check the configuration and its images
#                                   and write, into OUTDIR:
#       hv_config.c   the VM table (templates/hv_config.c.tpl)
#       hv_images.S   the images' loadable bytes (.incbin of imageN.bin)
#       imageN.bin    what a Multiboot loader would load of VM N's image
#       qemu.sh       the QEMU command that runs the hypervisor
#   hvconfig.py --projects CONFIG.xml
#                                   print the project directories that
#                                   build the images (the part of each
#                                   image path before its last /build/)
#
# The images are loaded like QEMU's Multiboot loader does it: the
# Multiboot header's a.out kludge addresses give the bytes of the file to
# load, where, the zeroed area after them and the entry point.

import os
import struct
import sys

from lxml import etree
import jinja2

HERE = os.path.dirname(os.path.abspath(__file__))

MAX_VMS = 8
MAX_CPUS = 16
LARGE_PAGE = 2 << 20
MAX_VM_MEMORY = 1 << 30
MAX_QEMU_MEMORY = 3 << 30       # RAM must stay below the uncached 4th GiB.
HIGH_MEMORY = 1 << 20

PORTS = {'COM1': 0x3F8, 'COM2': 0x2F8, 'COM3': 0x3E8, 'COM4': 0x2E8}
COM_ORDER = ['COM1', 'COM2', 'COM3', 'COM4']

MULTIBOOT_MAGIC = 0x1BADB002
MULTIBOOT_SEARCH = 8192
MULTIBOOT_AOUT_KLUDGE = 1 << 16
MULTIBOOT_VIDEO_MODE = 1 << 2
EM_X86_64 = 62

DEFAULT_GDB_TCP_PORT = 8100


class ConfigError(Exception):
    pass


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
    def __init__(self, index, element, config_dir):
        self.index = index
        self.name = element.get('Name')
        self.image = os.path.normpath(os.path.join(config_dir,
                                                   element.get('Image')))
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

    def where(self):
        return 'VM %s' % self.name


class Config(object):
    def __init__(self, path):
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
        self.vms = [VM(i, e, self.dir)
                    for i, e in enumerate(root.findall('VM'))]

    def check(self):
        if len(self.vms) > MAX_VMS:
            raise ConfigError('at most %d VMs' % MAX_VMS)
        names = {}
        for vm in self.vms:
            key = name_key(vm.name)
            if key in names:
                raise ConfigError('VM name %s is used twice' % vm.name)
            names[key] = vm
        users = {}
        if self.console is not None:
            users[self.console] = 'the hypervisor console'
        for vm in self.vms:
            for port, use in ((vm.console, 'console'), (vm.gdb, 'GDB')):
                if port is None:
                    continue
                if port in users:
                    raise ConfigError('%s is used by %s and by the %s of '
                                      'VM %s' % (port, users[port], use,
                                                 vm.name))
                users[port] = 'the %s of VM %s' % (use, vm.name)
        total_cpus = sum(vm.cpus for vm in self.vms)
        if total_cpus > MAX_CPUS:
            raise ConfigError('the VMs have %d CPUs, at most %d are '
                              'supported' % (total_cpus, MAX_CPUS))
        for vm in self.vms:
            if vm.memory % LARGE_PAGE != 0 or vm.memory == 0:
                raise ConfigError('%s: Memory must be a multiple of 2 MiB'
                                  % vm.where())
            if vm.memory > MAX_VM_MEMORY:
                raise ConfigError('%s: Memory is at most 1 GiB'
                                  % vm.where())
            load_image(vm)
        if self.qemu_memory() > MAX_QEMU_MEMORY:
            raise ConfigError('the VMs need more than 3 GiB of RAM')

    def qemu_memory(self):
        """QEMU -m in bytes: the hypervisor with its images, the VMs and
        some room, rounded up to 16 MiB."""
        images = sum(len(vm.data) for vm in self.vms)
        total = HIGH_MEMORY + images + (4 << 20) + LARGE_PAGE
        total += sum(vm.memory for vm in self.vms) + (16 << 20)
        return (total + (16 << 20) - 1) // (16 << 20) * (16 << 20)


def load_image(vm):
    """Read the image of a VM: check it and keep its loadable bytes."""
    try:
        with open(vm.image, 'rb') as f:
            data = f.read()
    except OSError as e:
        raise ConfigError('%s: %s' % (vm.where(), e))
    if len(data) < 64 or data[:4] != b'\x7fELF':
        raise ConfigError('%s: %s is not an ELF file'
                          % (vm.where(), vm.image))
    machine = struct.unpack_from('<H', data, 18)[0]
    if data[4] != 2 or data[5] != 1 or machine != EM_X86_64:
        raise ConfigError('%s: %s is not an x86_64 image (build it for '
                          'x86_64-qemu)' % (vm.where(), vm.image))

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
    if bss_end > vm.memory:
        raise ConfigError('%s: the image needs %d MiB of RAM, Memory is %d '
                          'MiB' % (vm.where(), (bss_end + (1 << 20) - 1) >> 20,
                                   vm.memory >> 20))
    if not load_addr <= entry < load_end:
        raise ConfigError('%s: the entry point is outside the image'
                          % vm.where())
    vm.data = data[file_offset:file_offset + (load_end - load_addr)]
    vm.load_addr = load_addr
    vm.bss_end = bss_end
    vm.entry = entry


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


def qemu_command(config):
    """Lines of qemu.sh. Each COM port can be redirected with HV_COM1..4."""
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
    last = max([COM_ORDER.index(p) for p in uses] + [0])
    lines = []
    serial = []
    for i in range(last + 1):
        port = COM_ORDER[i]
        use, default = uses.get(port, ('unused', 'null'))
        lines.append('# %s: %s (default %s)' % (port, use, default))
        serial.append('    -serial "${HV_%s:-%s}" \\' % (port, default))
    cpus = sum(vm.cpus for vm in config.vms)
    memory = config.qemu_memory() >> 20
    return lines, serial, cpus, memory


def generate(config, outdir):
    os.makedirs(outdir, exist_ok=True)
    for vm in config.vms:
        vm.bin = os.path.join(outdir, 'image%d.bin' % vm.index)
        write_if_changed(vm.bin, vm.data)

    env = jinja2.Environment(
        loader=jinja2.FileSystemLoader(os.path.join(HERE, 'templates')),
        keep_trailing_newline=True)
    text = env.get_template('hv_config.c.tpl').render(
        config=config, ports=PORTS,
        source=os.path.relpath(config.path, HERE))
    write_if_changed(os.path.join(outdir, 'hv_config.c'), text)

    images = ['/*', ' * GENERATED! DO NOT MODIFY!', ' *',
              ' * Loadable bytes of the VM images (hvconfig.py, from',
              ' * %s).' % os.path.relpath(config.path, HERE), ' */', '',
              '  .section .rodata.images, "a"']
    for vm in config.vms:
        n = vm.index
        images += ['', '  .balign 16', '  .globl hv_image_%d' % n,
                   'hv_image_%d:' % n, '  .incbin "%s"' % vm.bin,
                   'hv_image_%d_end:' % n, '  .balign 8',
                   '  .globl hv_image_%d_size' % n, 'hv_image_%d_size:' % n,
                   '  .quad hv_image_%d_end - hv_image_%d' % (n, n)]
    write_if_changed(os.path.join(outdir, 'hv_images.S'),
                     '\n'.join(images) + '\n')

    lines, serial, cpus, memory = qemu_command(config)
    script = ['#!/bin/sh', '#', '# GENERATED! DO NOT MODIFY!', '#',
              '# Runs the hypervisor of %s in QEMU'
              % os.path.relpath(config.path, HERE),
              '# (hvconfig.py). Serial ports, in COM order:'] + lines + [
              '# HV_COM1..HV_COM4 replace a port\'s QEMU -serial value;',
              '# GDB_PORT is the first GDB server\'s TCP port. Further',
              '# arguments are passed to QEMU.', '',
              'dir=$(cd "$(dirname "$0")" && pwd)',
              'GDB_PORT=${GDB_PORT:-%d}' % DEFAULT_GDB_TCP_PORT, '',
              'exec ${QEMU:-qemu-system-x86_64} -machine pc '
              '-cpu qemu64,+svm,+npt \\',
              '    -smp %d -m %d -display none -vga none -nodefaults \\'
              % (cpus, memory)] + serial + [
              '    -kernel "$dir/hv.elf" "$@"']
    write_if_changed(os.path.join(outdir, 'qemu.sh'),
                     '\n'.join(script) + '\n', 0o755)


def image_paths(path):
    """Image files of a configuration (for build dependencies)."""
    return [vm.image for vm in Config(path).vms]


def main(argv):
    try:
        if len(argv) == 3 and argv[1] == '--projects':
            for image in image_paths(argv[2]):
                marker = os.sep + 'build' + os.sep
                if marker in image:
                    print(image[:image.rindex(marker)])
            return 0
        if len(argv) != 3:
            print('usage: hvconfig.py CONFIG.xml OUTDIR\n'
                  '       hvconfig.py --projects CONFIG.xml',
                  file=sys.stderr)
            return 2
        config = Config(argv[1])
        config.check()
        generate(config, argv[2])
    except ConfigError as e:
        print('hvconfig.py: %s: %s' % (argv[-1] if argv[1] == '--projects'
                                       else argv[1], e), file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
