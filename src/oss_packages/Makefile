ARCH ?= aarch64
PACKAGE_FILE ?= packages.txt

PACKAGE_LIST := $(shell grep -v '^\s*#' $(PACKAGE_FILE) | grep -v '^\s*$$')
PACKAGE_TARGETS := $(foreach pkg,$(PACKAGE_LIST),$(subst =,-,$(pkg)).tar.xz)

all: extract

$(PACKAGE_TARGETS):
	$(MAKE) -f ../../utils/download.mk ARCH=$(ARCH) PACKAGE_FILE=$(PACKAGE_FILE)

extract: $(PACKAGE_TARGETS)
	@for pkg in $(PACKAGE_TARGETS); do \
		dir=$$(basename $$pkg .tar.xz); \
		echo "==> Extracting $$pkg to $$dir/"; \
		mkdir -p $$dir; \
		tar -xf $$pkg -C $$dir; \
	done

.PHONY: all extract
