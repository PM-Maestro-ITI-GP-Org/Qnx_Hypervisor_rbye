# All paths are relative to this Makefile's directory
# No hardcoded absolute paths — project is fully relocatable

QT6_QNX_ENV ?= ../../../qnx800/qnxsdp-env.sh

# Keep build.sh's dirs in sync with the paths the Makefile / Demo_APP look in.
QT6_OUTPUT_DIR ?= $(CURDIR)/output_dir
QT6_WORKING_DIR ?= $(CURDIR)/work_dir
QT6_SOURCE_DIR ?= $(CURDIR)/qt6-source

export QT6_QNX_ENV QT6_OUTPUT_DIR QT6_WORKING_DIR QT6_SOURCE_DIR

.PHONY: all config download build rebuild status

all: download build

download:
	@if ls output_dir/lib/libQt6Core.so* >/dev/null 2>&1; then \
		echo "Qt already built, nothing to do"; \
	elif [ -d qt6-source/qtbase ]; then \
		echo "source already extracted, skipping download"; \
	else \
		bash scripts/download.sh; \
	fi

rebuild:
	QT6_FORCE_REBUILD=1 bash scripts/build.sh

config:
	@if [ ! -d qt6-source/qtbase ]; then \
		echo "source not found — run 'make download' first"; exit 1; \
	fi
	bash scripts/configure.sh

build:
	@[ -f ".build-config" ] || { echo "==> no .build-config, generating default preset (run 'make config' to customize)"; bash scripts/configure.sh --preset default; }
	@if ls output_dir/lib/libQt6Core.so* >/dev/null 2>&1; then \
		echo "Qt already built at output_dir, skipping build"; \
		echo "run 'make rebuild' to force a rebuild"; \
	else \
		if [ ! -d qt6-source/qtbase ]; then \
			echo "source not found — run 'make download' first"; exit 1; \
		fi; \
		bash scripts/build.sh; \
	fi

status:
	@echo "QT6_SOURCE_DIR    = $(QT6_SOURCE_DIR)"
	@echo "QT6_OUTPUT_DIR    = $(QT6_OUTPUT_DIR)"
	@echo "QT6_WORKING_DIR   = $(QT6_WORKING_DIR)"
	@echo "QT6_QNX_ENV       = $(QT6_QNX_ENV)"
	@echo "QNX_HOST          = $(QNX_HOST)"
	@echo "QNX_TARGET        = $(QNX_TARGET)"
	@echo ""
	@cat ".build-config" 2>/dev/null || echo "no .build-config"
