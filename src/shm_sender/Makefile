# Makefile for shm_chunker -- cross-compiled for QNX aarch64le (Raspberry Pi 4/5)
# via the QNX Software Development Platform toolchain (qcc/q++).
# All build output (objects + binaries) goes into build/, source tree stays clean.
#
# PREREQUISITE: source the QNX SDP environment script first, so QNX_HOST /
# QNX_TARGET are set and qcc/q++ are on PATH, e.g.:
#   source ~/qnx800/qnxsdp-env.sh
# (path/version depend on your SDP install). This Makefile does NOT source it
# for you -- it just checks it's been done and fails loudly if not.
#
# Two languages by necessity: shm_reader_c.c is compiled as C11 because it's
# the only file that includes motor_shm.h (C11 _Atomic isn't standard C++).
# Everything else (main.cpp, chunker.hpp) is C++ and talks to it only through
# the plain-POD boundary in shm_reader_c.h.
#
# Targets:
#   make            -> builds build/shm_chunker (aarch64le QNX binary)
#   make test       -> also builds build/test_producer (verification tool --
#                      deploy + run this on the Pi too, not on the host)
#   make deploy PI_HOST=user@pi-hostname   -> scp's binaries to the Pi
#   make clean

QNX_VARIANT := gcc_ntoaarch64le

CC  := qcc
CXX := q++

CSTD    ?= -std=gnu11
# QNX SDP's bundled GCC may be older than what ships c++17 support depending
# on your SDP version. If q++ rejects -std=c++17, override on the command
# line: make CXXSTD=-std=c++14
CXXSTD  ?= -std=c++17
OPT     := -O2
WARN    := -Wall -Wextra

CFLAGS   := -V$(QNX_VARIANT) $(CSTD) $(OPT) $(WARN)
CXXFLAGS := -V$(QNX_VARIANT) $(CXXSTD) $(OPT) $(WARN)
LDFLAGS  := -V$(QNX_VARIANT)

# motor_shm.h's own header comment: QNX libc already provides shm_open/mmap/
# atomics -- do NOT link -lrt or -lpthread here.
LDLIBS :=

BUILD_DIR   := build
TARGET      := $(BUILD_DIR)/shm_chunker
TEST_TARGET := $(BUILD_DIR)/test_producer

.PHONY: all test clean check-env deploy

all: check-env $(TARGET)

test: check-env $(TARGET) $(TEST_TARGET)

# Fail with a clear message instead of a cryptic "qcc: not found" if the SDP
# env hasn't been sourced.
check-env:
	@if [ -z "$$QNX_HOST" ] || [ -z "$$QNX_TARGET" ]; then \
		echo "error: QNX_HOST/QNX_TARGET not set."; \
		echo "  Source the QNX SDP env script first, e.g.:"; \
		echo "    source ~/qnx800/qnxsdp-env.sh"; \
		exit 1; \
	fi
	@command -v $(CC)  >/dev/null 2>&1 || { echo "error: qcc not found on PATH"; exit 1; }
	@command -v $(CXX) >/dev/null 2>&1 || { echo "error: q++ not found on PATH"; exit 1; }

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# --- main app ----------------------------------------------------------
$(BUILD_DIR)/shm_reader_c.o: shm_reader_c.c shm_reader_c.h motor_wire.h motor_shm.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/main.o: main.cpp shm_reader_c.h chunker.hpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(TARGET): $(BUILD_DIR)/shm_reader_c.o $(BUILD_DIR)/main.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

# --- test producer (verification only, not shipped) ---------------------
$(TEST_TARGET): test_producer.c motor_wire.h motor_shm.h | $(BUILD_DIR)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS) $(LDLIBS)

# --- convenience: copy binaries to the Pi --------------------------------
# usage: make deploy PI_HOST=user@pi-hostname [PI_DIR=/path/on/pi]
PI_DIR ?= /tmp
deploy:
	@if [ -z "$(PI_HOST)" ]; then echo "usage: make deploy PI_HOST=user@pi-hostname"; exit 1; fi
	scp $(TARGET) $(if $(wildcard $(TEST_TARGET)),$(TEST_TARGET),) $(PI_HOST):$(PI_DIR)/

clean:
	rm -rf $(BUILD_DIR)
