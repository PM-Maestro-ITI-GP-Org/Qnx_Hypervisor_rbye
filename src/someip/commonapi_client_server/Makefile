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

SERVER_BIN := server/build/HelloWorldService
CLIENT_BIN := client/build/HelloWorldClient

.PHONY: all server client clean FORCE

all: server client

server: $(SERVER_BIN)
client: $(CLIENT_BIN)

# These both guarded on build/CMakeCache.txt, which cmake writes at *configure* time,
# so an interrupted or failed compile left the cache behind with no binary and every
# later build reported "already built" while the binary never appeared. Both are staged
# into the guest images (HelloWorldClient into guest-1, HelloWorldService into guest-2),
# so the failure surfaced far away as a broken mkifs. Depend on the binaries instead.
$(SERVER_BIN): server/CMakeLists.txt $(wildcard server/src/*)
	bash -c " \
		source ../commonapi-qnx/scripts/env.sh && \
		cd server && \
		cmake -B build -S . && \
		cmake --build build \
	"
	@test -f $@ || { echo "ERROR: build succeeded but $@ was not produced" >&2; exit 1; }

$(CLIENT_BIN): client/CMakeLists.txt $(wildcard client/src/*)
	bash -c " \
		source ../commonapi-qnx/scripts/env.sh && \
		cd client && \
		cmake -B build -S . && \
		cmake --build build \
	"
	@test -f $@ || { echo "ERROR: build succeeded but $@ was not produced" >&2; exit 1; }

# FORCE_REBUILD=1 forces a rebuild. Spelled ${FORCE_REBUILD:-0} inside a recipe this
# never worked: make parses that as a variable literally named "FORCE_REBUILD:-0" (a
# substitution reference needs an '='), so it always expanded to empty.
ifeq ($(FORCE_REBUILD),1)
$(SERVER_BIN) $(CLIENT_BIN): FORCE
endif

FORCE:

clean:
	rm -rf server/build client/build