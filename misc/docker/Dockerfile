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
# Build environment for CHPOK's x86_64-qemu and arm64-qemu examples, used
# by the top-level Makefile (`make all`). Works the same on macOS and
# Linux hosts.
#
# Uses Debian's native compiler for the host architecture and a genuine
# cross-compiler for the other one, so the image runs natively (no
# emulation) on both arm64 (Apple Silicon, ARM Linux) and amd64 hosts.
#
# The image contains only tools: the source tree is bind-mounted at
# run time, and the build runs as the invoking host user so that
# generated files are not owned by root.

FROM debian:bookworm-slim

# Debian's native gcc/binutils already provide the <host-triplet>-*
# tools (e.g. aarch64-linux-gnu-gcc on arm64); the other target needs
# the cross packages.
RUN case "$(dpkg --print-architecture)" in \
        arm64) cross_tools="gcc-x86-64-linux-gnu binutils-x86-64-linux-gnu" ;; \
        amd64) cross_tools="gcc-aarch64-linux-gnu binutils-aarch64-linux-gnu" ;; \
        *) echo "Unsupported host architecture" >&2; exit 1 ;; \
    esac \
    && apt-get update && apt-get install -y --no-install-recommends \
        scons \
        python3 \
        python3-pip \
        python3-jinja2 \
        python3-lxml \
        python3-yaml \
        gcc \
        binutils \
        $cross_tools \
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install --no-cache-dir --break-system-packages ipaddr

# The build runs under an arbitrary host uid that has no passwd entry
# or home directory, so give it a writable HOME.
ENV HOME=/tmp
