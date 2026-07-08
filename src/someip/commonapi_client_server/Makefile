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