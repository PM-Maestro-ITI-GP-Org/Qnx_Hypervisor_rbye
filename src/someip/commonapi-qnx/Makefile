.PHONY: all download patch build clean

all: build

build:
	@if [ "${FORCE_REBUILD:-0}" != "1" ] \
	    && [ -f build-rpi/.commonapi-qnx-build-complete ] \
	    && [ -f build-rpi/toolchain.cmake ] \
	    && [ -d build-rpi/generators/core ] \
	    && [ -d build-rpi/generators/someip ] \
	    && find build-rpi/lib -maxdepth 1 -name '*.so' 2>/dev/null | grep -q .; then \
	    echo "Build already complete; skipping. (Set FORCE_REBUILD=1 to rebuild.)"; \
	else \
	    $(MAKE) patch && ./scripts/build.sh; \
	fi

patch: download
	./scripts/patch.sh

download:
	@if [ -d third_party/vsomeip ] \
	    && [ -d third_party/capicxx-core-runtime ] \
	    && [ -d third_party/capicxx-someip-runtime ] \
	    && [ -f third_party/commonapi_core_generator.zip ] \
	    && [ -f third_party/commonapi_someip_generator.zip ] \
	    && ls third_party/boost_* >/dev/null 2>&1; then \
	    echo "Download already complete; skipping."; \
	else \
	    ./scripts/download.sh; \
	fi

clean:
	rm -rf build-rpi third_party