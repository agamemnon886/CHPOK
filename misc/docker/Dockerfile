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
# Modified by Kirill Brazhnikov, 2026-11-05: the kernel's GDB server works
# on arm64 too (a comment).
#
# Modified by Kirill Brazhnikov, 2026-11-11: the image also builds the
# function documentation (make docs): Doxygen, built from source in a
# stage of its own, the TeX packages of its PDF output, and make.
#
# Modified by Kirill Brazhnikov, 2026-11-15: the image also builds the
# system requirements documents (docs/SRQs, DocBook 5): dblatex, the
# DocBook 5 schema and xmllint.
#
# Build environment for CHPOK's x86_64-qemu and arm64-qemu examples, used
# by the top-level Makefile (`make all`), and of the function documentation
# and the system requirements documents (`make docs`). Works the same on
# macOS and Linux hosts.
#
# Uses Debian's native compiler for the host architecture and a genuine
# cross-compiler for the other one, so the image runs natively (no
# emulation) on both arm64 (Apple Silicon, ARM Linux) and amd64 hosts.
#
# The image contains only tools: the source tree is bind-mounted at
# run time, and the build runs as the invoking host user so that
# generated files are not owned by root.

# Doxygen: the function documentation needs 1.18 or later (Debian has
# 1.9.4). It is built from its source release (checked against its
# SHA-256) in this stage, so that its build tools stay out of the image.
FROM debian:bookworm-slim AS doxygen

ARG DOXYGEN_VERSION=1.18.0
ARG DOXYGEN_SHA256_A=a1deed70a6785bbec95a2b2a9e419dc7
ARG DOXYGEN_SHA256_B=f7b223a9d74a8644ae611c8e2dcdd354
RUN apt-get update && apt-get install -y --no-install-recommends \
        ca-certificates \
        curl \
        cmake \
        g++ \
        make \
        flex \
        bison \
        python3 \
    && rm -rf /var/lib/apt/lists/*
RUN curl -fsSL -o /tmp/doxygen.tar.gz \
        "https://www.doxygen.nl/files/doxygen-${DOXYGEN_VERSION}.src.tar.gz" \
    && echo "${DOXYGEN_SHA256_A}${DOXYGEN_SHA256_B}  /tmp/doxygen.tar.gz" \
        | sha256sum -c - \
    && tar -xzf /tmp/doxygen.tar.gz -C /tmp \
    && cmake -S "/tmp/doxygen-${DOXYGEN_VERSION}" -B /tmp/build \
        -DCMAKE_BUILD_TYPE=Release \
    && cmake --build /tmp/build -j"$(nproc)" \
    && install -s /tmp/build/bin/doxygen /usr/local/bin/doxygen

FROM debian:bookworm-slim

# Debian's native gcc/binutils already provide the <host-triplet>-*
# tools (e.g. aarch64-linux-gnu-gcc on arm64); the other target needs
# the cross packages. gdb-multiarch debugs both targets: through the
# kernel's GDB server, or QEMU's gdb stub (see README.md).
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
        gdb-multiarch \
        $cross_tools \
    && rm -rf /var/lib/apt/lists/* \
    && pip3 install --no-cache-dir --break-system-packages ipaddr

# The function documentation (make docs): make, and the TeX packages of
# Doxygen's PDF output (those of Debian's doxygen-latex, without its
# Doxygen).
RUN apt-get update && apt-get install -y --no-install-recommends \
        make \
        texlive-latex-extra \
        texlive-latex-recommended \
        texlive-fonts-recommended \
        texlive-plain-generic \
        texlive-font-utils \
        texlive-extra-utils \
        ghostscript \
        cm-super-minimal \
    && rm -rf /var/lib/apt/lists/*
COPY --from=doxygen /usr/local/bin/doxygen /usr/local/bin/doxygen

# The system requirements documents (make docs, docs/SRQs): DocBook 5
# sources, checked against the DocBook 5 RELAX NG schema (docbook5-xml)
# with xmllint (libxml2-utils) and turned into PDFs by dblatex, with the
# TeX packages above.
RUN apt-get update && apt-get install -y --no-install-recommends \
        dblatex \
        docbook5-xml \
        libxml2-utils \
        xsltproc \
    && rm -rf /var/lib/apt/lists/*

# The build runs under an arbitrary host uid that has no passwd entry
# or home directory, so give it a writable HOME.
ENV HOME=/tmp
