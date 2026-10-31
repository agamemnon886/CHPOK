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
#   make clean           # clean all examples
#   make clean-<name>    # clean examples/<name> only
#   make list            # list available example names
#   make docker-image    # (re)build the Docker build image only
#   make help            # show this help
#
# Variables:
#   BSP=<board>          # target board: x86_64-qemu (default) or arm64-qemu
#   NATIVE=1             # skip Docker, run scons directly on the host with
#                        # whatever toolchain is on PATH (set PREFIX=... in
#                        # the environment for a prefixed cross-compiler)
#
# The syspart-* examples use x86_64-only drivers (PCI, virtio-net), so they
# are not built for arm64-qemu.
#
# POK_PATH, required by every example's SConstruct, is derived from this
# Makefile's location.

POK_PATH := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
export POK_PATH

EXAMPLES_DIR := examples
EXAMPLES := $(sort $(notdir $(patsubst %/,%,$(dir $(wildcard $(EXAMPLES_DIR)/*/SConstruct)))))

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
# owned by root.
DOCKER_RUN := $(DOCKER) run --rm \
	--user $(shell id -u):$(shell id -g) \
	-v "$(POK_PATH):$(POK_PATH)" \
	-e POK_PATH="$(POK_PATH)" \
	-e PREFIX="$(DOCKER_PREFIX)" \
	$(DOCKER_IMAGE)

# All examples compile the kernel and libpok into the same shared
# $(POK_PATH)/build/ directory, so example builds must not run
# concurrently.
.NOTPARALLEL:

.PHONY: all clean list help docker-image $(EXAMPLES) $(addprefix clean-,$(EXAMPLES))

all: $(BSP_EXAMPLES)

help:
	@echo "Usage:"
	@echo "  make / make all      Build all examples"
	@echo "  make <name>          Build one example (e.g. make memblocks)"
	@echo "  make clean           Clean all examples"
	@echo "  make clean-<name>    Clean one example"
	@echo "  make list            List available example names"
	@echo "  make docker-image    (Re)build the Docker build image"
	@echo "  make help            Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  BSP=<board>          Target board: x86_64-qemu (default) or arm64-qemu"
	@echo "  NATIVE=1             Run scons on the host instead of in Docker"
	@echo ""
	@echo "Examples:"
	@for e in $(EXAMPLES); do echo "  $$e"; done

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

docker-image:
	$(DOCKER) build -t $(DOCKER_IMAGE) $(DOCKER_DIR)

# Remove build artifacts directly instead of running `scons -c`: scons
# has to locate the compiler before it can compute what to clean, and
# cleaning should work without a toolchain or Docker. Everything removed
# here is untracked and covered by .gitignore.

# Besides every example, also removes what the examples share: the
# kernel/libpok objects under build/ and the scons/Python state that
# the build scatters across the source tree.
clean: $(addprefix clean-,$(EXAMPLES))
	rm -rf build misc/__pycache__
	find boards kernel libpok syspart misc -name '.sconsign*' -delete
	find kernel libpok syspart -name '*.o' -delete

$(addprefix clean-,$(EXAMPLES)):
	$(eval EX := $(patsubst clean-%,%,$@))
	rm -rf $(EXAMPLES_DIR)/$(EX)/build $(EXAMPLES_DIR)/$(EX)/*/build
	rm -f $(EXAMPLES_DIR)/$(EX)/sizes.c $(EXAMPLES_DIR)/$(EX)/*/deployment.c
	rm -f $(EXAMPLES_DIR)/$(EX)/scons_build.log
	find $(EXAMPLES_DIR)/$(EX) -name '.sconsign*' -delete

list:
	@echo $(EXAMPLES)
