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
# CRC-32 of the hypervisor image.
#
#   hvcrc.py HV.elf           write the CRC into HV.elf (after linking)
#   hvcrc.py --check HV.elf   compare the stored CRC with the image's;
#                             exit status 1 if they differ
#
# The CRC covers the loaded bytes from hv_image_begin up to the 4-byte
# word at hv_image_crc, which holds the CRC (little-endian); see hv.lds.
# hv.elf is one PT_LOAD segment that QEMU's Multiboot loader copies
# unchanged, so these are file bytes, and src/crc.c computes the same CRC
# over the loaded image at boot. The CRC is zlib's (IEEE 802.3).

import struct
import sys
import zlib

EM_X86_64 = 62
PT_LOAD = 1
SHT_SYMTAB = 2
CRC_SIZE = 4

ELF_HEADER = struct.Struct('<16sHHIQQQIHHHHHH')
PROGRAM_HEADER = struct.Struct('<IIQQQQQQ')
SECTION_HEADER = struct.Struct('<IIQQQQIIQQ')
SYMBOL = struct.Struct('<IBBHQQ')


class ImageError(Exception):
    pass


def c_string(data, offset):
    end = data.find(b'\0', offset)
    if end < 0:
        raise ImageError('bad string table')
    return data[offset:end].decode('ascii', 'replace')


def symbols(data, shoff, shnum, shentsize):
    """Values of the symbols in .symtab, by name."""
    sections = [SECTION_HEADER.unpack_from(data, shoff + i * shentsize)
                for i in range(shnum)]
    values = {}
    for sh in sections:
        if sh[1] != SHT_SYMTAB:
            continue
        strtab = sections[sh[6]]
        names = data[strtab[4]:strtab[4] + strtab[5]]
        for off in range(sh[4], sh[4] + sh[5], SYMBOL.size):
            st_name, _, _, _, st_value, _ = SYMBOL.unpack_from(data, off)
            values[c_string(names, st_name)] = st_value
    return values


def covered_range(data):
    """File offsets of the checked bytes (start, end); the CRC word is at
    end."""
    if len(data) < ELF_HEADER.size:
        raise ImageError('not an ELF file')
    (ident, _, machine, _, _, phoff, shoff, _, _, phentsize, phnum,
     shentsize, shnum, _) = ELF_HEADER.unpack_from(data)
    if ident[:4] != b'\x7fELF' or ident[4] != 2 or ident[5] != 1:
        raise ImageError('not a little-endian ELF64 file')
    if machine != EM_X86_64:
        raise ImageError('not an x86-64 ELF file')
    loads = []
    for i in range(phnum):
        ph = PROGRAM_HEADER.unpack_from(data, phoff + i * phentsize)
        if ph[0] == PT_LOAD:
            loads.append(ph)
    if len(loads) != 1:
        raise ImageError('%d loadable segments, hv.lds makes one'
                         % len(loads))
    _, _, p_offset, p_vaddr, _, p_filesz, _, _ = loads[0]
    values = symbols(data, shoff, shnum, shentsize)
    offsets = []
    for name in ('hv_image_begin', 'hv_image_crc'):
        if name not in values:
            raise ImageError('no symbol %s (see hv.lds)' % name)
        offsets.append(values[name] - p_vaddr + p_offset)
    start, end = offsets
    if not (p_offset <= start <= end
            and end + CRC_SIZE <= p_offset + p_filesz <= len(data)):
        raise ImageError('the CRC word is not in the loaded bytes')
    return start, end


def main(argv):
    check = len(argv) == 3 and argv[1] == '--check'
    if len(argv) != 2 and not check:
        sys.stderr.write('usage: hvcrc.py [--check] HV.elf\n')
        return 2
    path = argv[-1]
    try:
        with open(path, 'rb') as f:
            data = bytearray(f.read())
        start, end = covered_range(data)
    except (OSError, ImageError, struct.error) as e:
        sys.stderr.write('hvcrc.py: %s: %s\n' % (path, e))
        return 1
    computed = zlib.crc32(data[start:end]) & 0xFFFFFFFF
    stored = struct.unpack_from('<I', data, end)[0]
    if check:
        print('%s: CRC-32 of %d bytes at offset %d: stored 0x%08x, '
              'computed 0x%08x' % (path, end - start, start, stored,
                                   computed))
        return 0 if stored == computed else 1
    if stored != computed:
        struct.pack_into('<I', data, end, computed)
        with open(path, 'r+b') as f:
            f.seek(end)
            f.write(data[end:end + CRC_SIZE])
    print('%s: CRC-32 0x%08x of %d bytes' % (path, computed, end - start))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
