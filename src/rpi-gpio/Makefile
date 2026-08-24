QNX800_DIR := ../../qnx800

.PHONY: all qnx clean

all: qnx

qnx:
	@[ -n "$(QNX_HOST)" ] || [ -f "$(QNX800_DIR)/qnxsdp-env.sh" ] || { echo "QNX SDP not found at $(QNX800_DIR)/qnxsdp-env.sh"; exit 1; }
	@bash -c 'set -e; \
		[ -n "$$QNX_HOST" ] || . "$(QNX800_DIR)/qnxsdp-env.sh"; \
		cmake -S resmgr -B build-qnx -DCMAKE_TOOLCHAIN_FILE=$(CURDIR)/qnx/build/aarch64-qnx.cmake; \
		cmake --build build-qnx'

clean:
	rm -rf build build-qnx
