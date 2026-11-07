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
# Part of tests/hypervisor (reset case): connects to the console of a
# JetOS VM (a QEMU TCP serial port), waits for the kernel to boot, opens
# the monitor and types "reset", then waits for the kernel to boot again.
# Everything received is written to LOG.
#
# Usage: monitor_reset.py PORT LOG TIMEOUT

import socket
import sys
import time

BOOTED = b'JET OS kernel initialized'


def main(port, log_path, timeout):
    deadline = time.time() + timeout
    sock = None
    while sock is None and time.time() < deadline:
        try:
            sock = socket.create_connection(('127.0.0.1', port), timeout=1)
        except OSError:
            time.sleep(0.2)
    if sock is None:
        print('monitor_reset.py: no connection to port %d' % port)
        return 1
    sock.settimeout(0.2)
    received = b''

    def wait_for(text, count=1):
        nonlocal received
        while received.count(text) < count and time.time() < deadline:
            try:
                data = sock.recv(4096)
            except socket.timeout:
                continue
            if not data:
                break
            received += data
            log.write(data)
            log.flush()
        return received.count(text) >= count

    with open(log_path, 'wb') as log:
        steps = [(BOOTED, 1, b'\n'), (b'K> ', 1, b'reset\n'),
                 (b'Rebooting', 1, None), (BOOTED, 2, None)]
        for text, count, answer in steps:
            if not wait_for(text, count):
                print('monitor_reset.py: no "%s" (%d) on the console'
                      % (text.decode(), count))
                return 1
            if answer is not None:
                time.sleep(0.5)
                sock.sendall(answer)
    sock.close()
    return 0


if __name__ == '__main__':
    sys.exit(main(int(sys.argv[1]), sys.argv[2], float(sys.argv[3])))
