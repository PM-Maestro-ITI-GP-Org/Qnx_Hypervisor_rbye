
SUBDIRS := qnx_host qnx_guests src

.PHONY: all clean $(SUBDIRS) qnx_install flash-sd submodules

PROJECT_DIR := $(dir $(realpath $(lastword $(MAKEFILE_LIST))))
CTI_QSC_URL ?= https://www.qnx.com/swcenter
SDP_PROFILE_ID := com.qnx.cti-$(shell stat -c '%d_%i' $(PROJECT_DIR))
QNX_SDP_PATH=$(PROJECT_DIR)/qnx800
BUILD_NAME := $(TARGET)
BUILD := $(PROJECT_DIR)$(BUILD_NAME)


ifneq (,$(filter qnx_install,$(MAKECMDGOALS)))
ifeq ("${QSC_CLT_PATH}","")
$(error QSC_CLT_PATH is not defined. Please set it to the qnxsoftwarecenter_clt binary))
else ifeq ($(wildcard $(QSC_CLT_PATH)),)
$(error QSC_CLT_PATH '$(QSC_CLT_PATH)' is invalid. Please set it to the qnxsoftwarecenter_clt binary))
endif
endif



all: submodules $(SUBDIRS)

qnx_host:  qnx_guests apps_src
	$(MAKE) -C qnx_host

apps_src: submodules
	$(MAKE) -C src

# -------------------------------------------------------------------
# Submodule initialisation — checks out the correct branch for app
# submodules (others stay at the pinned commit).
# -------------------------------------------------------------------
APP_SUBMODULES := src/qt_cluster src/motor_data_producer

submodules:
	@for mod in $(APP_SUBMODULES); do \
		git submodule update --init --remote "$$mod" 2>/dev/null | \
		  sed "s/^/  [submodule] /" || true; \
	done

.PHONY: submodules

qnx_install:

	echo "Installing QNX packages using qnxsoftwarecenter_clt..."
	echo "QNX_SDP_PATH: $(QNX_SDP_PATH)"
	echo "PROJECT_DIR: $(PROJECT_DIR)"
	# $(QSC_CLT_PATH) -url $(CTI_QSC_URL) -mirrorBaseline qnx800 @options_file
	$(QSC_CLT_PATH) -url $(CTI_QSC_URL) -cleanInstall -setExperimentalEnabled=true \
		        -setPolicy=conservative \
		        -destination $(QNX_SDP_PATH) \
			-importAndInstall $(BUILD)/qsc_install_packages.list \
			-profile $(SDP_PROFILE_ID) \
			$(CTI_QSC_EXTRA_OPTIONS) \
			@options_file
	mkdir -p qnx_host/install
	mkdir -p qnx_guests/install

qnx_guests: apps_src
	$(MAKE) -C qnx_guests

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

qnx_host/images/disk.img:
	$(MAKE) -C qnx_host/images disk.img

flash: qnx_host/images/disk.img
	@device="$(FLASH_DEVICE)"; \
	if [ -z "$$device" ]; then \
		read -p "Enter SD card device path (e.g. /dev/sdX or /dev/mmcblk0): " device; \
	fi; \
	if [ -z "$$device" ]; then \
		echo "No device path provided." >&2; \
		exit 1; \
	fi; \
	echo "Writing qnx_host/images/disk.img to $$device"; \
	sudo dd if=qnx_host/images/disk.img of="$$device" bs=4M conv=fsync status=progress && sync