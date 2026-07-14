PACKAGE_FILE ?= world.preinstall
ARCH ?= aarch64
BASE_URL ?= https://repo.oss.qnx.com
REPOS ?= 8.0.3/core 8.0.3/extra 8.0.4/qnx-extra 8.0.4/qnx-core

PACKAGE_LIST := $(shell grep -v '^\s*#' $(PACKAGE_FILE) | grep -v '^\s*$$')
PACKAGE_TARGETS := $(foreach pkg,$(PACKAGE_LIST),$(subst =,-,$(pkg)).tar.xz)

all: $(PACKAGE_TARGETS)

%.tar.xz:
	@pkgver=$$(basename $@ .tar.xz); \
	for repo in $(REPOS); do \
		url="$(BASE_URL)/$$repo/$(ARCH)/$$pkgver.apk"; \
		if wget -q -O "$@" "$$url" 2>/dev/null; then \
			echo "OK  $$url -> $@"; \
			exit 0; \
		fi; \
	done; \
	echo "FAIL $$pkgver"; \
	exit 1

clean:
	rm -f *.tar.xz
