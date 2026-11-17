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
# Builds the examples under examples/ with scons. By default each build
# runs inside a Docker container (see misc/docker/Dockerfile), so the
# only host requirements are make and Docker. Works on macOS and Linux.
#
# Usage:
#   make / make all      # build all examples
#   make <name>          # build examples/<name> only (e.g. make memblocks)
#   make clean           # clean all builds: examples, tests, benchmarks,
#                        # hypervisor and documentation
#   make clean-<name>    # clean examples/<name> only
#   make list            # list available example names
#   make benchmarks      # build all benchmarks under benchmark/
#   make clean-benchmarks # clean all benchmarks
#   make tests           # build all tests under tests/
#   make run             # build and run all tests in QEMU on the host
#   make run-<name>      # build and run tests/<name> only (e.g. make run-mutex)
#   make clean-tests     # clean all tests
#   make docker-image    # (re)build the Docker build image only
#   make hv              # build the hypervisor and the images of HV
#   make run-hv          # build and run the hypervisor in QEMU
#   make BSP=arm64-qemu run-hv  # the same on arm64 (BSP= works for every
#                        # hypervisor target)
#   make hv-test         # run the hypervisor tests (tests/hypervisor)
#   make clean-hv        # clean the hypervisor builds
#   make docs            # build the documentation of the C functions, one
#                        # PDF per component: docs/boards.pdf, kernel.pdf,
#                        # shared.pdf, libpok.pdf, syspart.pdf,
#                        # hypervisor.pdf
#                        # (misc/docs, in Docker)
#                        # and the system requirements documents
#                        # docs/JetOS_SRQs.pdf and Hypervisor_SRQs.pdf
#                        # (docs/SRQs, DocBook 5)
#   make docs-<name>     # build docs/<name>.pdf only (e.g. make docs-kernel)
#   make docs-srqs       # build the system requirements documents only
#   make docs-trace      # check the traceability SRQ - HLR - LLR
#   make clean-docs      # remove the documentation build and the PDFs
#   make help            # show this help
#
# Variables:
#   BSP=<board>          # target board: x86_64-qemu (default) or arm64-qemu
#   NATIVE=1             # skip Docker, run scons directly on the host with
#                        # whatever toolchain is on PATH (set PREFIX=... in
#                        # the environment for a prefixed cross-compiler)
#   VERBOSE=0            # make run: show only the result of each test, not
#                        # the output of the running test (VERBOSE=1, default)
#   HV=<name>            # hypervisor configuration
#                        # examples/<name>/config.xml (default
#                        # hv-two-vms); HV_CONFIG=<file> gives any other one
#                        # in the repository (Docker mounts only that)
#
# The syspart-* examples use x86_64-only drivers (PCI, virtio-net), so they
# are not built for arm64-qemu.
#
# POK_PATH, required by every example's SConstruct, is derived from this
# Makefile's location.
#
# Modified by Kirill Brazhnikov, 2026-11-01: added the tests, run,
# run-<name> and clean-tests targets for the tests under tests/; make run
# shows the output of the running tests (VERBOSE).
#
# Modified by Kirill Brazhnikov, 2026-11-04: the gdb-test target runs
# tests/gdbserver with GDB.
#
# Modified by Kirill Brazhnikov, 2026-11-05: gdb-test works on both boards.
#
# Modified by Kirill Brazhnikov, 2026-11-06: hv, run-hv, hv-test and
# clean-hv targets for the hypervisor under hypervisor/; make clean also
# cleans the hypervisor builds.
#
# Modified by Kirill Brazhnikov, 2026-11-07: the hypervisor configurations
# are taken from examples/ (default hv-two-vms).
#
# Modified by Kirill Brazhnikov, 2026-11-08: the hypervisor is built for
# BSP too (x86_64-qemu or arm64-qemu), with its images, into build/<BSP>/
# next to the configuration; the usage and make help show
# make BSP=arm64-qemu run-hv. The docs and clean-docs targets build and
# remove the function documentation.
#
# Modified by Kirill Brazhnikov, 2026-11-10: make clean also cleans the
# tests, the benchmarks and the documentation, and removes every Python
# __pycache__ directory.
#
# Modified by Kirill Brazhnikov, 2026-11-11: the documentation is built in
# the build image (misc/docker/Dockerfile); make docs shows the progress of
# Doxygen and pdflatex, the warnings and a summary.
#
# Modified by Kirill Brazhnikov, 2026-11-12: make docs builds one document
# per component, docs/<name>.pdf (misc/docs/Doxyfile.<name>), instead of
# jetos_functions.pdf; make docs-<name> builds one of them.
#
# Modified by Kirill Brazhnikov, 2026-11-13: make docs also counts the types
# and macros, which may be none.
#
# Modified by Kirill Brazhnikov, 2026-11-15: make docs also builds the
# system requirements documents docs/JetOS_SRQs.pdf and
# docs/Hypervisor_SRQs.pdf from their DocBook 5 sources in docs/SRQs
# (make docs-srqs, make docs-<name>); make clean-docs keeps docs/.
# docs/shared.pdf documents the library that the kernel and the
# hypervisor share (common/ and include/). make clean no longer looks for
# boards/ (the board support packages are in kernel/bsps).
#
# Modified by Kirill Brazhnikov, 2026-11-16: make docs-trace (also run by make
# docs) checks the traceability between the system requirements and the high
# and low level requirements in the function headers; every function and every
# SRQ must be covered (TRACE_STRICT=1), except the SRQs marked "Not
# applicable"; and that every step of @design is linked to an LLR of its
# function (itself or through an enclosing step) and every LLR to a step. The
# SRQ books get the list of the HLRs and their documents, for the links.

POK_PATH := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
export POK_PATH

EXAMPLES_DIR := examples
EXAMPLES := $(sort $(notdir $(patsubst %/,%,$(dir $(wildcard $(EXAMPLES_DIR)/*/SConstruct)))))

# Benchmark projects under benchmark/: directories with an SConstruct but
# no SConscript (their partition directories have both).
BENCH_DIR := benchmark
BENCH_SCONSTRUCTS := $(wildcard $(BENCH_DIR)/*/SConstruct $(BENCH_DIR)/*/*/SConstruct \
	$(BENCH_DIR)/*/*/*/SConstruct)
BENCH_PARTITIONS := $(dir $(wildcard $(BENCH_DIR)/*/*/SConscript \
	$(BENCH_DIR)/*/*/*/SConscript $(BENCH_DIR)/*/*/*/*/SConscript))
BENCHMARKS := $(sort $(patsubst %/,%,$(filter-out $(BENCH_PARTITIONS),$(dir $(BENCH_SCONSTRUCTS)))))

# Tests under tests/: one project per directory, run in QEMU by
# tests/run_test.sh (see tests/README).
TESTS_DIR := tests
TESTS := $(sort $(notdir $(patsubst %/,%,$(dir $(wildcard $(TESTS_DIR)/*/SConstruct)))))

SCONS := scons
BSP ?= x86_64-qemu
SCONS_ARGS := bsp=$(BSP)

# Examples built by `make all` for the selected board.
ifeq ($(BSP),arm64-qemu)
BSP_EXAMPLES := $(filter-out syspart-%,$(EXAMPLES))
else
BSP_EXAMPLES := $(EXAMPLES)
endif

DOCKER ?= docker
DOCKER_IMAGE := chpok-build
DOCKER_DIR := misc/docker

# Toolchain prefix inside the Docker image, per board.
DOCKER_PREFIX_x86_64-qemu := x86_64-linux-gnu-
DOCKER_PREFIX_arm64-qemu := aarch64-linux-gnu-
DOCKER_PREFIX := $(DOCKER_PREFIX_$(BSP))

# The source tree is mounted at the same path inside the container, so
# absolute paths recorded by scons stay valid for native builds too. The
# build runs as the host user so that, on Linux, generated files are not
# owned by root. $(call DOCKER_RUN_WITH,<prefix>) runs with another
# toolchain prefix (the hypervisor is always built for x86_64).
DOCKER_RUN_WITH = $(DOCKER) run --rm \
	--user $(shell id -u):$(shell id -g) \
	-v "$(POK_PATH):$(POK_PATH)" \
	-e POK_PATH="$(POK_PATH)" \
	-e PREFIX="$(1)" \
	$(DOCKER_IMAGE)
DOCKER_RUN := $(call DOCKER_RUN_WITH,$(DOCKER_PREFIX))

# All examples compile the kernel and libpok into the same shared
# $(POK_PATH)/build/ directory, so example builds must not run
# concurrently.
.NOTPARALLEL:

.PHONY: all clean list help docker-image benchmarks clean-benchmarks \
	tests run gdb-test clean-tests $(addprefix run-,$(TESTS)) \
	$(EXAMPLES) $(addprefix clean-,$(EXAMPLES))

all: $(BSP_EXAMPLES)

help:
	@echo "Usage:"
	@echo "  make / make all      Build all examples"
	@echo "  make <name>          Build one example (e.g. make memblocks)"
	@echo "  make clean           Clean all builds (examples, tests,"
	@echo "                       benchmarks, hypervisor, docs)"
	@echo "  make clean-<name>    Clean one example"
	@echo "  make list            List available example names"
	@echo "  make benchmarks      Build all benchmarks (benchmark/)"
	@echo "  make clean-benchmarks Clean all benchmarks"
	@echo "  make tests           Build all tests (tests/)"
	@echo "  make run             Build and run all tests in QEMU"
	@echo "  make run-<name>      Build and run one test (e.g. make run-mutex)"
	@echo "  make gdb-test        Run tests/gdbserver with GDB"
	@echo "  make clean-tests     Clean all tests"
	@echo "  make docker-image    (Re)build the Docker build image"
	@echo "  make hv              Build the hypervisor (configuration HV)"
	@echo "  make run-hv          Build and run the hypervisor in QEMU"
	@echo "  make BSP=arm64-qemu run-hv"
	@echo "                       The same on arm64 (any hv target)"
	@echo "  make hv-test         Run the hypervisor tests"
	@echo "  make clean-hv        Clean the hypervisor builds"
	@echo "  make docs            Build the function docs: docs/boards.pdf,"
	@echo "                       kernel.pdf, shared.pdf, libpok.pdf,"
	@echo "                       syspart.pdf and hypervisor.pdf, and"
	@echo "                       the system requirements:"
	@echo "                       docs/JetOS_SRQs.pdf and"
	@echo "                       Hypervisor_SRQs.pdf"
	@echo "  make docs-<name>     Build one of them (e.g. make docs-kernel)"
	@echo "  make docs-srqs       Build the system requirements only"
	@echo "  make docs-trace      Check the traceability of the"
	@echo "                       requirements"
	@echo "  make clean-docs      Remove the documentation build and PDFs"
	@echo "  make help            Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  BSP=<board>          Target board: x86_64-qemu (default) or arm64-qemu"
	@echo "  NATIVE=1             Run scons on the host instead of in Docker"
	@echo "  VERBOSE=0            make run: show only the test results"
	@echo "  HV=<name>            Hypervisor config (default hv-two-vms)"
	@echo ""
	@echo "Examples:"
	@for e in $(EXAMPLES); do echo "  $$e"; done
	@echo ""
	@echo "Tests:"
	@for t in $(TESTS); do echo "  $$t"; done

ifdef NATIVE
$(EXAMPLES):
	cd $(EXAMPLES_DIR)/$@ && $(SCONS) $(SCONS_ARGS)
else
$(EXAMPLES): docker-image
ifeq ($(DOCKER_PREFIX),)
	$(error Unknown BSP=$(BSP); use x86_64-qemu or arm64-qemu)
endif
	$(DOCKER_RUN) sh -c 'cd "$(POK_PATH)/$(EXAMPLES_DIR)/$@" && $(SCONS) $(SCONS_ARGS)'
endif

# Benchmarks are built one after another, in one container, stopping at
# the first failure.
BENCH_BUILD := set -e; for b in $(BENCHMARKS); do \
	echo "== $$b"; (cd "$(POK_PATH)/$$b" && $(SCONS) $(SCONS_ARGS)); done

ifdef NATIVE
benchmarks:
	$(BENCH_BUILD)
else
benchmarks: docker-image
ifeq ($(DOCKER_PREFIX),)
	$(error Unknown BSP=$(BSP); use x86_64-qemu or arm64-qemu)
endif
	$(DOCKER_RUN) sh -c '$(BENCH_BUILD)'
endif

# Tests are built like the benchmarks, in one container; running them
# needs qemu-system-x86_64 / qemu-system-aarch64 on the host.
VERBOSE ?= 1
RUN_TEST := VERBOSE=$(VERBOSE) $(TESTS_DIR)/run_test.sh

TEST_BUILD = set -e; for t in $(1); do \
	echo "== building $(TESTS_DIR)/$$t"; (cd "$(POK_PATH)/$(TESTS_DIR)/$$t" && $(SCONS) $(SCONS_ARGS)); done

ifdef NATIVE
tests:
	$(call TEST_BUILD,$(TESTS))

$(addprefix run-,$(TESTS)):
	$(call TEST_BUILD,$(patsubst run-%,%,$@))
	$(RUN_TEST) $(patsubst run-%,%,$@) $(BSP)
else
tests: docker-image
ifeq ($(DOCKER_PREFIX),)
	$(error Unknown BSP=$(BSP); use x86_64-qemu or arm64-qemu)
endif
	$(DOCKER_RUN) sh -c '$(call TEST_BUILD,$(TESTS))'

$(addprefix run-,$(TESTS)): docker-image
ifeq ($(DOCKER_PREFIX),)
	$(error Unknown BSP=$(BSP); use x86_64-qemu or arm64-qemu)
endif
	$(DOCKER_RUN) sh -c '$(call TEST_BUILD,$(patsubst run-%,%,$@))'
	$(RUN_TEST) $(patsubst run-%,%,$@) $(BSP)
endif

# Runs every test, also after a failure, and fails if any test failed.
run: tests
	@status=0; for t in $(TESTS); do \
		$(RUN_TEST) $$t $(BSP) || status=1; done; exit $$status

# The GDB server test: GDB (in the build image, or on the host with
# NATIVE=1) debugs tests/gdbserver while it runs in QEMU.
ifneq ($(filter $(BSP),x86_64-qemu arm64-qemu),)
gdb-test: run-gdbserver
	VERBOSE=$(VERBOSE) NATIVE=$(NATIVE) BSP=$(BSP) \
		$(TESTS_DIR)/gdbserver/run_gdb_test.sh
else
gdb-test:
	$(error Unknown BSP=$(BSP); use x86_64-qemu or arm64-qemu)
endif

# The hypervisor (see README.md), for BSP: make hv builds the JetOS images
# that the configuration names (the projects in front of their /build/
# directories, {bsp} replaced by BSP) for BSP, then hv.elf and qemu.sh in
# build/<BSP>/ next to the configuration file. In Docker, only the
# repository is mounted, so the configuration and its images must be in
# it.
HV ?= hv-two-vms
HV_CONFIG ?= $(EXAMPLES_DIR)/$(HV)/config.xml
HV_CONFIG_PATH = $(abspath $(HV_CONFIG))
HV_BUILD_DIR = $(dir $(HV_CONFIG_PATH))build/$(BSP)
HV_TESTS_DIR := $(TESTS_DIR)/hypervisor
HV_TEST_CONFIGS := $(abspath $(wildcard $(HV_TESTS_DIR)/*/config.xml))

# Shell commands that build the configurations $(1) (absolute paths):
# the projects of their images, each once, then the hypervisor of each.
# The project list is assigned first, so that set -e stops on an error.
HV_BUILD = set -e; cd "$(POK_PATH)"; projects=; \
	for c in $(1); do \
		p=$$(python3 hypervisor/hvconfig.py --bsp $(BSP) \
			--projects "$$c"); \
		projects="$$projects $$p"; \
	done; \
	for p in $$(printf "%s\n" $$projects | sort -u); do \
		echo "== building $$p"; \
		(cd "$$p" && $(SCONS) bsp=$(BSP)); \
	done; \
	cd hypervisor; \
	for c in $(1); do \
		echo "== building the hypervisor for $$c"; \
		$(SCONS) config="$$c" bsp=$(BSP); \
	done

ifdef NATIVE
HV_RUN_BUILD = $(call HV_BUILD,$(1))
HV_DOCKER_IMAGE :=
else
HV_RUN_BUILD = $(call DOCKER_RUN_WITH,$(DOCKER_PREFIX)) \
	sh -c '$(call HV_BUILD,$(1))'
HV_DOCKER_IMAGE := docker-image
endif

.PHONY: hv run-hv hv-test clean-hv

hv: $(HV_DOCKER_IMAGE)
ifeq ($(DOCKER_PREFIX),)
	$(error Unknown BSP=$(BSP); use x86_64-qemu or arm64-qemu)
endif
	$(call HV_RUN_BUILD,$(HV_CONFIG_PATH))

run-hv: hv
	sh "$(HV_BUILD_DIR)/qemu.sh"

# Builds every configuration under tests/hypervisor (in one container),
# then runs them.
hv-test: $(HV_DOCKER_IMAGE)
ifeq ($(DOCKER_PREFIX),)
	$(error Unknown BSP=$(BSP); use x86_64-qemu or arm64-qemu)
endif
	$(call HV_RUN_BUILD,$(HV_TEST_CONFIGS))
	BSP=$(BSP) VERBOSE=$(VERBOSE) NATIVE=$(NATIVE) \
		$(HV_TESTS_DIR)/run_hv_test.sh

clean-hv:
	rm -rf $(EXAMPLES_DIR)/hv-*/build $(HV_TESTS_DIR)/*/build \
		hypervisor/__pycache__
	if [ -d hypervisor ]; then \
		find hypervisor -name '.sconsign*' -delete; fi

docker-image:
	$(DOCKER) build -t $(DOCKER_IMAGE) $(DOCKER_DIR)

# The function documentation (README.md, "Function documentation"), one
# document per component (DOCS): docs/<name>.pdf, configured by
# misc/docs/Doxyfile.<name>. Doxygen (1.18 or later) turns the header of
# every C function into LaTeX, which pdflatex makes into the PDF. Both run
# in the build image, or with NATIVE=1 on the host (doxygen and pdflatex
# needed). make docs builds the documents one after the other, and
# make docs-<name> one of them. For each, make shows Doxygen's progress
# (QUIET = NO in the Doxyfile), the start of each pdflatex pass (pdflatex
# itself runs in batch mode, silently), the warnings about the headers
# (build/docs/<name>/warnings.log) and a summary. The whole output is in
# build/docs/<name>/doxygen.log and latex.log.
DOCS := boards kernel shared libpok syspart hypervisor
DOCS_DIR := docs
DOCS_OUT := build/docs
DOCS_TARGETS := $(addprefix docs-,$(DOCS))
DOCS_SHOW_LATEX = | tee $$out/latex.log \
	| grep --line-buffered -E "^(pdflatex|makeindex) |^Rerunning" \
	| while read -r line; do \
		echo "  $$(date +%H:%M:%S) $${line%% ||*}"; \
	done
# The commands that build the document $d (a shell variable).
DOCS_BUILD = out=$(DOCS_OUT)/$$d && pdf=$(DOCS_DIR)/$$d.pdf && \
	rm -rf $$out && mkdir -p $$out $(DOCS_DIR) && \
	tex=$$out/latex && t0=$$(date +%s) && \
	echo "==== $$pdf" && \
	echo "== Doxygen $$(doxygen -v): misc/docs/Doxyfile.$$d -> $$out" && \
	{ doxygen misc/docs/Doxyfile.$$d 2>&1; \
	  echo $$? > $$out/doxygen.rc; } | tee $$out/doxygen.log; \
	t1=$$(date +%s); \
	if [ "$$(cat $$out/doxygen.rc)" != 0 ]; then \
		tail -20 $$out/doxygen.log; \
		echo "Doxygen failed: see $$out/doxygen.log"; exit 1; \
	fi; \
	if [ -s $$out/warnings.log ]; then \
		echo "== Doxygen warnings ($$out/warnings.log):"; \
		cat $$out/warnings.log; \
	fi; \
	echo "== pdflatex $$tex/refman.tex (several passes, in batch mode)"; \
	{ make -C $$tex 2>&1; echo $$? > $$out/latex.rc; } \
		$(DOCS_SHOW_LATEX); \
	t2=$$(date +%s); \
	if [ "$$(cat $$out/latex.rc)" != 0 ]; then \
		tail -20 $$out/latex.log; \
		grep -A3 "^!" $$tex/refman.log | head -20; \
		echo "pdflatex failed: see $$tex/refman.log"; exit 1; \
	fi; \
	cp $$tex/refman.pdf $$pdf && \
	funcs=$$(cat $$tex/*_8[ch].tex | \
		grep -c "doxysubsubsection{.texorpdfstring{[^}]*()}" || true) && \
	types=$$(cat $$tex/*_8[ch].tex $$(ls $$tex/group__*.tex 2>/dev/null) \
		| grep -c "jetostype" || true) && \
	macros=$$(cat $$tex/*_8[ch].tex $$(ls $$tex/group__*.tex 2>/dev/null) \
		| grep -c "jetosmacro" || true) && \
	warns=$$(wc -l < $$out/warnings.log) && \
	pages=$$(sed -n \
		"s/^Output written on refman.pdf (\([0-9]*\) pages.*/\1/p" \
		$$tex/refman.log) && \
	echo "== $$pdf built" && \
	echo "  Doxygen:    $$((t1 - t0)) s;" \
		"$$(grep -c "^Parsing file" $$out/doxygen.log) files read" && \
	echo "  Documented: $$(ls $$tex/*_8[ch].tex | wc -l) files," \
		"$$funcs functions, $$types types, $$macros macros" && \
	echo "  Warnings:   $$warns about the headers," \
		"$$(grep -c "warning:" $$out/doxygen.log) about the" \
		"configuration ($$out/doxygen.log)" && \
	echo "  pdflatex:   $$((t2 - t1)) s;" \
		"$$(grep -c "^pdflatex " $$out/latex.log) passes;" \
		"$$(grep -c "^Overfull" $$tex/refman.log) overfull lines" && \
	echo "  PDF:        $$pages pages, $$(du -h $$pdf | cut -f1)" && \
	printf "  %-20s %5s pages, %4s functions, %4s types, %4s macros," \
		$$pdf $$pages $$funcs $$types $$macros \
		> $$out/summary.txt && \
	printf " %s warnings\n" $$warns >> $$out/summary.txt

# The system requirements documents (README.md, "System requirements"),
# SRQS: docs/<name>.pdf, from the DocBook 5 book docs/SRQs/<name>.xml and
# the chapters it includes. xmllint checks the book against the DocBook 5
# RELAX NG schema (SRQS_RNG) and resolves its XIncludes, and dblatex
# (settings in misc/docs/srqs.xsl) makes the PDF with pdflatex. make docs
# builds them before the function documentation, make docs-srqs only
# them and make docs-<name> one of them. The output of dblatex is in
# build/docs/<name>/dblatex.log and its LaTeX files in
# build/docs/<name>/tex. build/docs/<name>/hlrdocs.xml lists each HLR of
# the C files of TRACE_DIRS with the document that has it (kernel/bsps:
# boards, the rest of kernel/: kernel, common/: shared, hypervisor/:
# hypervisor), for the links of the Traceability fields (srqs.xsl).
SRQS := JetOS_SRQs Hypervisor_SRQs
SRQS_DIR := $(DOCS_DIR)/SRQs
SRQS_RNG ?= /usr/share/xml/docbook/schema/rng/5.0/docbook.rng
SRQS_TARGETS := $(addprefix docs-,$(SRQS))
# The commands that build the document $d (a shell variable).
SRQS_BUILD = out=$(DOCS_OUT)/$$d && pdf=$(DOCS_DIR)/$$d.pdf && \
	src=$(SRQS_DIR)/$$d.xml && \
	rm -rf $$out && mkdir -p $$out $(DOCS_DIR) && t0=$$(date +%s) && \
	echo "==== $$pdf" && \
	echo "== xmllint: $$src, DocBook 5 ($(SRQS_RNG))" && \
	xmllint --xinclude --noout --relaxng $(SRQS_RNG) $$src && \
	xmllint --xinclude --output $$out/$$d.xml $$src && \
	{ echo "<hlrs>"; \
	  grep -rHo --include="*.c" "\\\\hlrid{HLR_[A-Z0-9_]*}" \
		$(TRACE_DIRS) | sed \
		-e "s,^kernel/bsps/[^:]*:,boards ," \
		-e "s,^kernel/[^:]*:,kernel ," \
		-e "s,^common/[^:]*:,shared ," \
		-e "s,^hypervisor/[^:]*:,hypervisor ," \
		-e "s,.hlrid{\(HLR_[A-Z0-9_]*\)}$$,\1," \
		-e "s,^\([a-z]*\) \(.*\)$$,<h id=\"\2\" d=\"\1\"/>,"; \
	  echo "</hlrs>"; } > $$out/hlrdocs.xml && \
	echo "== dblatex $$out/$$d.xml (pdflatex, several passes)" && \
	{ dblatex -d --tmpdir=$$out/tex -p misc/docs/srqs.xsl \
		-P jetos.hlrdocs=$$PWD/$$out/hlrdocs.xml \
		-o $$out/$$d.pdf $$out/$$d.xml > $$out/dblatex.log 2>&1 || \
	  { tail -20 $$out/dblatex.log; \
	    echo "dblatex failed: see $$out/dblatex.log"; exit 1; }; } && \
	t1=$$(date +%s) && \
	cp $$out/$$d.pdf $$pdf && \
	reqs=$$(grep -c "xml:id=\"SRQ_" $$out/$$d.xml || true) && \
	pages=$$(sed -n \
		"s/^Output written on .*pdf (\([0-9]*\) pages.*/\1/p" \
		$$out/tex/$$d.log | tail -1) && \
	echo "== $$pdf built" && \
	echo "  Requirements: $$reqs" && \
	echo "  dblatex:      $$((t1 - t0)) s;" \
		"$$(grep -c "^Overfull" $$out/tex/$$d.log) overfull lines" && \
	echo "  PDF:          $$pages pages, $$(du -h $$pdf | cut -f1)" && \
	printf "  %-24s %5s pages, %4s requirements\n" \
		$$pdf $$pages $$reqs > $$out/summary.txt

# Requirements traceability (README.md, "Requirements traceability"):
# make docs-trace reads the @hlr and @llr fields of the function headers in
# TRACE_DIRS and the Traceability fields of the SRQs in docs/SRQs, and
# fails if: an SRQ, HLR or LLR identifier is defined twice; an SRQ has no
# Traceability field; an HLR traces to no SRQ or to one that doesn't exist
# (SRQ_JET_* from kernel/, SRQ_HV_* from hypervisor/, either from common/);
# an LLR traces to no HLR or to one of another function; an HLR has no
# LLR; a step of @design is linked (\llrref, by itself or by a step that
# encloses it) to no LLR or to an LLR of another function; an LLR is linked
# to no step; or the Traceability fields and the HLRs' "Traces to:"
# disagree. The
# functions without requirements and the SRQs without HLRs are listed in
# build/docs/trace/trace.txt and are errors too with TRACE_STRICT=1 (the
# default); an SRQ whose Traceability is "Not applicable" (a constraint on
# the whole software) needs no HLR, and must have none.
# It runs on the host (find, awk, sort, comm) and is part of make docs.
TRACE_DIRS := kernel common hypervisor
TRACE_OUT := $(DOCS_OUT)/trace
TRACE_STRICT ?= 1

# The headers: one record per line (FUNC file name covered, HLR id file,
# LLR id file, HT hlr srq, LT llr hlr, ERR message).
define TRACE_HEADERS_AWK
function err(m) { print "ERR " FILENAME ":" FNR ": " m }
function end_item() {
	if (item != "" && ntr == 0) { err(item " traces to nothing") }
	item = ""; ntr = 0; intr = 0
}
function end_step(cov) {
	if (step == "") { return }
	cov = (pcov || nref > 0) ? 1 : 0
	if (nl > 0) {
		print "STEP", cov
		if (!cov) {
			err(fn ": step " step " of the design is linked to " \
			    "no LLR")
		}
	}
	sind[++sn] = sindent; scov[sn] = cov; snum[sn] = num; step = ""
}
function end_block(i) {
	end_item(); end_step()
	for (i = 1; i <= nh; i++) {
		if (!(hl[i] in lcount)) { err(hl[i] " has no LLR") }
	}
	for (i = 1; i <= nl; i++) {
		if (!(ll[i] in lused)) {
			err(ll[i] " is linked to no step of the design")
		}
	}
	if (fn != "") { print "FUNC", FILENAME, fn, (nh > 0) ? 1 : 0 }
	inblk = 0; sect = ""; fn = ""; nh = 0; nl = 0; sn = 0
}
/^\/\*\*/ { inblk = ($$0 !~ /\*\//); sect = ""; fn = ""; nh = 0; nl = 0;
	sn = 0; step = ""; item = ""; ntr = 0; blk++; next }
!inblk { next }
/\*\// { end_block(); next }
/^ \* @funcname/ { fn = $$3; gsub(/`/, "", fn) }
/^ \* @hlr/ { end_item(); sect = "H"; next }
/^ \* @llr/ { end_item(); sect = "L"; next }
/^ \* @design/ { end_item(); sect = "D"; sn = 0; step = ""; next }
/^ \* @/ { end_item(); end_step(); sect = ""; next }
sect == "" { next }
sect == "D" {
	line = $$0
	if (match(line, /^ \* +[0-9]+\. /)) {
		end_step()
		num = substr(line, 4, RLENGTH - 5); sub(/^ +/, "", num)
		match(line, /^ \* +/); sindent = RLENGTH
		while (sn > 0 && sind[sn] >= sindent) { sn-- }
		pcov = (sn > 0) ? scov[sn] : 0
		step = ""
		for (i = 1; i <= sn; i++) { step = step snum[i] "." }
		step = step num; nref = 0
	}
	while (match(line, /\\llrref\{[A-Z0-9_]+\}/)) {
		t = substr(line, RSTART + 8, RLENGTH - 9)
		line = substr(line, RSTART + RLENGTH)
		if (step == "") {
			err(t " is linked outside a step of the design")
		} else if (!(t in lblk) || lblk[t] != blk) {
			err(fn ": step " step " is linked to " t \
			    ", not an LLR of this function")
		} else {
			lused[t] = 1; nref++
		}
	}
	next
}
{
	line = $$0
	if (match(line, /^ \* - \\[hl]lrid\{[HL]LR_[A-Z0-9_]+\}:/)) {
		end_item()
		item = substr(line, 13, RLENGTH - 14)
		if (substr(item, 1, 1) != sect) {
			err(item " is in the wrong field")
		}
		if (substr(line, 7, 3) != tolower(substr(item, 1, 3))) {
			err(item " is written with the wrong command")
		}
		print substr(item, 1, 3), item, FILENAME
		if (sect == "H") { hl[++nh] = item; hblk[item] = blk }
		if (sect == "L") { ll[++nl] = item; lblk[item] = blk }
		line = substr(line, RSTART + RLENGTH)
	}
	if (item == "") {
		if (line ~ /[^ *]/) { err("text outside a requirement") }
		next
	}
	p = index(line, "Traces to:")
	if (p > 0) { intr = 1; line = substr(line, p + 10) }
	if (!intr) { next }
	while (match(line, /\\(srq|hlr|llr)ref\{[A-Z0-9_]+\}/)) {
		t = substr(line, RSTART + 8, RLENGTH - 9)
		if (substr(line, RSTART + 1, 3) != tolower(substr(t, 1, 3))) {
			err(t " is written with the wrong command")
		}
		line = substr(line, RSTART + RLENGTH)
		ntr++
		if (sect == "H") {
			if ((FILENAME ~ /^kernel\// && t !~ /^SRQ_JET_/) ||
			    (FILENAME ~ /^hypervisor\// && t !~ /^SRQ_HV_/) ||
			    t !~ /^SRQ_/) {
				err(item " traces to " t ", not an SRQ of " \
				    "this program")
			}
			print "HT", item, t
		} else if (!(t in hblk) || hblk[t] != blk) {
			err(item " traces to " t ", not an HLR of this " \
			    "function")
		} else {
			lcount[t]++
			print "LT", item, t
		}
	}
}
endef
export TRACE_HEADERS_AWK

# The SRQs: SRQ id file, TF id (a Traceability field), ST srq hlr, NA id
# (a Traceability that is "Not applicable").
define TRACE_SRQS_AWK
/xml:id="SRQ_/ {
	match($$0, /SRQ_[A-Z0-9_]+/); cur = substr($$0, RSTART, RLENGTH)
	print "SRQ", cur, FILENAME; tr = 0
}
/<title>Traceability<\/title>/ { print "TF", cur; tr = 1; next }
tr {
	if ($$0 ~ /<para>Not applicable/) { print "NA", cur }
	line = $$0
	while (match(line, /HLR_[A-Z0-9_]+/)) {
		print "ST", cur, substr(line, RSTART, RLENGTH)
		line = substr(line, RSTART + RLENGTH)
	}
	if ($$0 ~ /<\/para>/) { tr = 0 }
}
endef
export TRACE_SRQS_AWK

ifdef NATIVE
DOCS_RUN = cd "$(POK_PATH)" &&
DOCS_DOCKER_IMAGE :=
else
DOCS_RUN = $(DOCKER) run --rm \
	--user $(shell id -u):$(shell id -g) \
	-v "$(POK_PATH):$(POK_PATH)" -w "$(POK_PATH)" \
	$(DOCKER_IMAGE)
DOCS_DOCKER_IMAGE := docker-image
endif

.PHONY: docs docs-srqs docs-trace clean-docs $(DOCS_TARGETS) \
	$(SRQS_TARGETS)

docs: $(DOCS_DOCKER_IMAGE)
	@$(DOCS_RUN) sh -c 'for d in $(SRQS); do { $(SRQS_BUILD); } || exit 1; \
		done; \
		for d in $(DOCS); do { $(DOCS_BUILD); } || exit 1; \
		done; echo "==== Documents"; \
		for d in $(SRQS) $(DOCS); do \
			cat $(DOCS_OUT)/$$d/summary.txt; done'
	@$(MAKE) --no-print-directory docs-trace

docs-srqs: $(DOCS_DOCKER_IMAGE)
	@$(DOCS_RUN) sh -c 'for d in $(SRQS); do { $(SRQS_BUILD); } || exit 1; \
		done; echo "==== Documents"; \
		for d in $(SRQS); do cat $(DOCS_OUT)/$$d/summary.txt; done'

$(DOCS_TARGETS): docs-%: $(DOCS_DOCKER_IMAGE)
	@$(DOCS_RUN) sh -c 'd=$*; $(DOCS_BUILD)'

$(SRQS_TARGETS): docs-%: $(DOCS_DOCKER_IMAGE)
	@$(DOCS_RUN) sh -c 'd=$*; $(SRQS_BUILD)'

docs-trace:
	@set -e; o=$(TRACE_OUT); mkdir -p $$o; \
	find $(TRACE_DIRS) -name '*.c' | LC_ALL=C sort | \
		xargs awk "$$TRACE_HEADERS_AWK" > $$o/headers.txt; \
	awk "$$TRACE_SRQS_AWK" $(SRQS_DIR)/*/*.xml > $$o/srqs.txt; \
	cd $$o; \
	{ grep '^ERR ' headers.txt || true; } | cut -c5- > errors.txt; \
	awk '$$1 == "SRQ" { print $$2 }' srqs.txt | LC_ALL=C sort \
		> srq-ids.txt; \
	awk '$$1 == "TF" { print $$2 }' srqs.txt | LC_ALL=C sort > tf-ids.txt; \
	awk '$$1 == "HLR" || $$1 == "LLR" { print $$2 }' headers.txt | \
		LC_ALL=C sort > req-ids.txt; \
	uniq -d srq-ids.txt | sed 's/$$/ is defined twice/' >> errors.txt; \
	uniq -d req-ids.txt | sed 's/$$/ is defined twice/' >> errors.txt; \
	uniq -d tf-ids.txt | sed 's/$$/ has two Traceability fields/' \
		>> errors.txt; \
	LC_ALL=C comm -23 srq-ids.txt tf-ids.txt | \
		sed 's/$$/ has no Traceability field/' >> errors.txt; \
	awk '$$1 == "HT" { print $$3, $$2 }' headers.txt | LC_ALL=C sort -u \
		> pairs-headers.txt; \
	awk '$$1 == "ST" { print $$2, $$3 }' srqs.txt | LC_ALL=C sort -u \
		> pairs-srqs.txt; \
	cut -d" " -f1 pairs-headers.txt | LC_ALL=C sort -u | \
		LC_ALL=C comm -23 - srq-ids.txt | \
		sed 's/$$/ is traced to, but no such SRQ exists/' \
		>> errors.txt; \
	LC_ALL=C comm -23 pairs-headers.txt pairs-srqs.txt | \
		awk '{ print $$2 " traces to " $$1 \
			", whose Traceability does not list it" }' \
		>> errors.txt; \
	LC_ALL=C comm -13 pairs-headers.txt pairs-srqs.txt | \
		awk '{ print $$1 " lists " $$2 \
			" in Traceability, which does not trace to it" }' \
		>> errors.txt; \
	awk '$$1 == "FUNC" && $$4 == 0 { print $$2 ": " $$3 }' headers.txt \
		> uncovered.txt; \
	awk '$$1 == "NA" { print $$2 }' srqs.txt | LC_ALL=C sort -u \
		> na-ids.txt; \
	cut -d" " -f1 pairs-headers.txt | LC_ALL=C sort -u | \
		LC_ALL=C comm -12 - na-ids.txt | \
		sed 's/$$/ is "Not applicable", but HLRs trace to it/' \
		>> errors.txt; \
	{ cut -d" " -f1 pairs-srqs.txt; cat na-ids.txt; } | LC_ALL=C sort -u | \
		LC_ALL=C comm -13 - srq-ids.txt > srqs-without-hlr.txt; \
	funcs=$$(grep -c '^FUNC ' headers.txt || true); \
	covered=$$(grep -c '^FUNC .* 1$$' headers.txt || true); \
	{ echo "SRQs: $$(wc -l < srq-ids.txt | tr -d ' '), without HLRs:" \
		"$$(wc -l < srqs-without-hlr.txt | tr -d ' ')"; \
	  echo "HLRs: $$(grep -c '^HLR ' headers.txt || true)," \
		"LLRs: $$(grep -c '^LLR ' headers.txt || true)"; \
	  echo "Functions with requirements: $$covered of $$funcs"; \
	  echo "Design steps: $$(grep -c '^STEP ' headers.txt || true)," \
		"linked to LLRs: $$(grep -c '^STEP 1$$' headers.txt || true)"; \
	  echo "Errors: $$(wc -l < errors.txt | tr -d ' ')"; \
	  cat errors.txt; \
	  echo "== Functions without requirements"; cat uncovered.txt; \
	  echo "== SRQs without HLRs"; cat srqs-without-hlr.txt; \
	} > trace.txt; \
	echo "==== Traceability ($(TRACE_OUT)/trace.txt)"; \
	sed -n 1,5p trace.txt; cat errors.txt; \
	test ! -s errors.txt; \
	if [ "$(TRACE_STRICT)" = 1 ]; then \
		{ test ! -s uncovered.txt && \
		  test ! -s srqs-without-hlr.txt; } || \
		{ echo "TRACE_STRICT: functions or SRQs are not covered"; \
		  exit 1; }; \
	fi

# docs/ also holds the sources of the system requirements (docs/SRQs),
# so only the PDFs are removed.
clean-docs:
	rm -rf $(DOCS_OUT)
	rm -f $(addprefix $(DOCS_DIR)/,$(addsuffix .pdf,$(DOCS) $(SRQS)))

# Remove build artifacts directly instead of running `scons -c`: scons
# has to locate the compiler before it can compute what to clean, and
# cleaning should work without a toolchain or Docker. Everything removed
# here is untracked and covered by .gitignore.

# Besides every example, test, benchmark, the hypervisor builds and the
# documentation, also removes what the examples share: the kernel/libpok
# objects under build/ and the scons/Python state that the builds and
# scripts scatter across the source tree.
clean: $(addprefix clean-,$(EXAMPLES)) clean-tests clean-benchmarks \
		clean-hv clean-docs
	rm -rf build
	find . -path ./.git -prune -o -type d -name __pycache__ -prune \
		-exec rm -rf {} +
	find kernel libpok syspart misc -name '.sconsign*' -delete
	find kernel libpok syspart -name '*.o' -delete

$(addprefix clean-,$(EXAMPLES)):
	$(eval EX := $(patsubst clean-%,%,$@))
	rm -rf $(EXAMPLES_DIR)/$(EX)/build $(EXAMPLES_DIR)/$(EX)/*/build
	rm -f $(EXAMPLES_DIR)/$(EX)/sizes.c $(EXAMPLES_DIR)/$(EX)/*/deployment.c
	rm -f $(EXAMPLES_DIR)/$(EX)/scons_build.log
	find $(EXAMPLES_DIR)/$(EX) -name '.sconsign*' -delete

clean-benchmarks:
	find $(BENCH_DIR) -type d -name build -prune -exec rm -rf {} +
	find $(BENCH_DIR) \( -name '.sconsign*' -o -name scons_build.log \) -delete

clean-tests:
	find $(TESTS_DIR) -type d -name build -prune -exec rm -rf {} +
	find $(TESTS_DIR) \( -name '.sconsign*' -o -name scons_build.log \) -delete

list:
	@echo $(EXAMPLES)
