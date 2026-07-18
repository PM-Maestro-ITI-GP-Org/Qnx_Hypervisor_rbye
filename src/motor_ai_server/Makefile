ifndef QNX_SDP_PATH
ifneq ($(strip $(QNX_HOST)),)
ifneq ($(findstring /host/linux/x86_64,$(QNX_HOST)),)
QNX_SDP_PATH := $(patsubst %/host/linux/x86_64,%,$(QNX_HOST))
else
$(error QNX_HOST is set but does not contain the expected /host/linux/x86_64 suffix)
endif
else
$(error QNX_SDP_PATH is not set and QNX_HOST is not set. Please source qnxsdp-env.sh first or set QNX_HOST)
endif
endif

export QNX_SDP_PATH

SOMEIP_DIR := ../someip

SERVER_BIN  := server/build/MotorDataService
SERVER_DEPS := server/CMakeLists.txt $(wildcard server/src/*) $(wildcard interface/*)

.PHONY: all server clean FORCE

all: server

server: $(SERVER_BIN)

# The old guard skipped the build whenever server/build/CMakeCache.txt existed, but
# cmake writes that cache at *configure* time. An interrupted or failed compile left
# the cache behind with no binary, and every later build then reported "already built"
# while the IFS build died with "No rule to make target .../MotorDataService".
# Depending on the binary itself keeps the check honest and gives incremental rebuilds.
$(SERVER_BIN): $(SERVER_DEPS)
	bash -c " \
		source $(SOMEIP_DIR)/commonapi-qnx/scripts/env.sh && \
		cd server && \
		cmake -B build -S . && \
		cmake --build build \
	"
	@test -f $@ || { echo "ERROR: build succeeded but $@ was not produced" >&2; exit 1; }

# FORCE_REBUILD=1 forces a rebuild. This used to be spelled ${FORCE_REBUILD:-0} inside
# the recipe, which make parses as a variable literally named "FORCE_REBUILD:-0" (a
# substitution reference needs an '='), so it always expanded to empty and the flag
# never had any effect.
ifeq ($(FORCE_REBUILD),1)
$(SERVER_BIN): FORCE
endif

FORCE:

clean:
	rm -rf server/build
