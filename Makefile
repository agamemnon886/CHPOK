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
#   make clean           # clean all examples and hypervisor builds
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
# make BSP=arm64-qemu run-hv.

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
	@echo "  make clean           Clean all examples and hypervisor builds"
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

# Remove build artifacts directly instead of running `scons -c`: scons
# has to locate the compiler before it can compute what to clean, and
# cleaning should work without a toolchain or Docker. Everything removed
# here is untracked and covered by .gitignore.

# Besides every example and the hypervisor builds, also removes what the
# examples share: the kernel/libpok objects under build/ and the
# scons/Python state that the build scatters across the source tree.
clean: $(addprefix clean-,$(EXAMPLES)) clean-hv
	rm -rf build misc/__pycache__
	find boards kernel libpok syspart misc -name '.sconsign*' -delete
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
