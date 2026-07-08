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

.PHONY: all server client clean

all: server client

server:
	@if [ "${FORCE_REBUILD:-0}" != "1" ] \
	    && [ -f server/build/CMakeCache.txt ]; then \
	    echo "Server already built; skipping. (Set FORCE_REBUILD=1 to rebuild.)"; \
	else \
	    bash -c " \
		source ../commonapi-qnx/scripts/env.sh && \
		cd server && \
		cmake -B build -S . && \
		cmake --build build \
	"; \
	fi

client:
	@if [ "${FORCE_REBUILD:-0}" != "1" ] \
	    && [ -f client/build/CMakeCache.txt ]; then \
	    echo "Client already built; skipping. (Set FORCE_REBUILD=1 to rebuild.)"; \
	else \
	    bash -c " \
		source ../commonapi-qnx/scripts/env.sh && \
		cd client && \
		cmake -B build -S . && \
		cmake --build build \
	"; \
	fi

clean:
	rm -rf server/build client/build