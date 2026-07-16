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

.PHONY: all download patch build clean

all: build

build:
	@if [ "${FORCE_REBUILD:-0}" != "1" ] \
	    && [ -f build-rpi/toolchain.cmake ] \
	    && [ -d build-rpi/generators/core ] \
	    && [ -d build-rpi/generators/someip ] \
	    && find build-rpi/lib -maxdepth 1 -name '*.so' 2>/dev/null | grep -q .; then \
	    echo "Build already complete; skipping. (Set FORCE_REBUILD=1 to rebuild.)"; \
	else \
	    $(MAKE) patch && QNX_SDP_PATH=$(QNX_SDP_PATH) ./scripts/build.sh; \
	fi

patch: download
	QNX_SDP_PATH=$(QNX_SDP_PATH) ./scripts/patch.sh

download:
	@if [ -d third_party/vsomeip ] \
	    && [ -d third_party/capicxx-core-runtime ] \
	    && [ -d third_party/capicxx-someip-runtime ] \
	    && [ -f third_party/commonapi_core_generator.zip ] \
	    && [ -f third_party/commonapi_someip_generator.zip ] \
	    && ls third_party/boost_* >/dev/null 2>&1; then \
	    echo "Download already complete; skipping."; \
	else \
	    QNX_SDP_PATH=$(QNX_SDP_PATH) ./scripts/download.sh; \
	fi

clean:
	rm -rf build-rpi third_party