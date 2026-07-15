WORK_DIR := /media/gemy/Extra/ITI_GP/Qnx_Hypervisor_rbye/src/QT/qt6-qnx-libs
SCRIPTS_DIR := $(WORK_DIR)/scripts
QT6_OUTPUT_DIR := /media/gemy/Extra/ITI_GP/Qnx_Hypervisor_rbye/src/QT/qt6-qnx-libs/output_dir
QT6_WORKING_DIR := /media/gemy/Extra/ITI_GP/Qnx_Hypervisor_rbye/src/QT/qt6-qnx-libs/work_dir
QT6_SOURCE_DIR := /media/gemy/Extra/ITI_GP/Qnx_Hypervisor_rbye/src/QT/qt6-qnx-libs/qt6-source
QT6_QNX_ENV ?= /media/gemy/Linux_Workspace/ITI/QNX_WS/software_platform/qnxsdp-env.sh

export QT6_OUTPUT_DIR
export QT6_WORKING_DIR
export QT6_SOURCE_DIR
export QT6_QNX_ENV

.PHONY: all config download build rebuild status

all: download build

download:
	@if ls "$(QT6_OUTPUT_DIR)"/lib/libQt6Core.so* >/dev/null 2>&1; then \
		echo "Qt already built, nothing to do"; \
	elif [ -d "$(QT6_SOURCE_DIR)/qtbase" ]; then \
		echo "source already extracted, skipping download"; \
	else \
		bash $(SCRIPTS_DIR)/download.sh; \
	fi

rebuild:
	rm -rf $(QT6_WORKING_DIR) $(QT6_OUTPUT_DIR)
	bash $(SCRIPTS_DIR)/build.sh

config:
	@if [ ! -d "$(QT6_SOURCE_DIR)/qtbase" ]; then \
		echo "source not found — run 'make download' first"; exit 1; \
	fi
	bash $(SCRIPTS_DIR)/configure.sh

build:
	@[ -f "$(WORK_DIR)/.build-config" ] || { echo "no .build-config — run 'make config' first"; exit 1; }
	@if ls "$(QT6_OUTPUT_DIR)"/lib/libQt6Core.so* >/dev/null 2>&1; then \
		echo "Qt already built at $(QT6_OUTPUT_DIR), skipping build"; \
		echo "run 'make rebuild' to force a rebuild"; \
	else \
		if [ ! -d "$(QT6_SOURCE_DIR)/qtbase" ]; then \
			echo "source not found — run 'make download' first"; exit 1; \
		fi; \
		bash $(SCRIPTS_DIR)/build.sh; \
	fi

status:
	@echo "WORK_DIR      = $(WORK_DIR)"
	@echo "OUTPUT_DIR    = $(QT6_OUTPUT_DIR)"
	@echo "WORKING_DIR   = $(QT6_WORKING_DIR)"
	@echo "SOURCE_DIR    = $(QT6_SOURCE_DIR)"
	@echo "QNX_HOST      = $(QNX_HOST)"
	@echo "QNX_TARGET    = $(QNX_TARGET)"
	@echo ""
	@cat "$(WORK_DIR)/.build-config" 2>/dev/null || echo "no .build-config"
