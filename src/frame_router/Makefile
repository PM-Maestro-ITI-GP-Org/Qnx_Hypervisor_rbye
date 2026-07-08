# Makefile for the shmem framebuffer guest/host pair.
#
# Usage:
#   make                # builds both fb_guest and fb_host
#   make guest          # builds only fb_guest
#   make host           # builds only fb_host
#   make clean
#
# Override variants if your targets differ from the defaults below, e.g.:
#   make GUEST_VARIANT=gcc_ntox86_64
#   make HOST_VARIANT=gcc_ntoaarch64le
#
# Defaults: both guest and host are aarch64 QNX (Raspberry Pi 4B host
# domain, per `uname -a`; guest per your hyp-guest-arm BSP).

GUEST_VARIANT ?= gcc_ntoaarch64le
HOST_VARIANT  ?= gcc_ntoaarch64le

QCC = qcc

CFLAGS = -Wall -Wextra -O2

BUILD_DIR = build
GUEST_BIN = $(BUILD_DIR)/fb_guest
HOST_BIN  = $(BUILD_DIR)/fb_host
TEST_BIN  = $(BUILD_DIR)/fb_test

.PHONY: all guest host test clean

all: guest host test

guest: $(GUEST_BIN)

host: $(HOST_BIN)

test: $(TEST_BIN)

$(BUILD_DIR):
	mkdir -p $@

$(GUEST_BIN): fb_guest.c shm_proto.h | $(BUILD_DIR)
	$(QCC) -V$(GUEST_VARIANT) $(CFLAGS) -o $@ fb_guest.c -lscreen

$(HOST_BIN): fb_host.c shm_proto.h | $(BUILD_DIR)
	$(QCC) -V$(HOST_VARIANT) $(CFLAGS) -o $@ fb_host.c -lhyp -lscreen

$(TEST_BIN): fb_test.c shm_proto.h | $(BUILD_DIR)
	$(QCC) -V$(GUEST_VARIANT) $(CFLAGS) -o $@ fb_test.c -lscreen

clean:
	rm -rf $(BUILD_DIR)
