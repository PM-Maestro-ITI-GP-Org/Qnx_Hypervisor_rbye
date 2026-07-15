ROOT_DIR := $(notdir $(CURDIR))
ifndef QCONFIG
QCONFIG=qconfig.mk
endif
include $(QCONFIG)

HOST_MKIFS := mkifs
HOST_MKI := mki
HOST_SED := sed
.SUFFIXES := .build .bin .raw .ui

# NOTE: This value must match the '[image=<start_addr>]' value in the build file.
IMAGE_LOAD_ADDR = 0x80000

BUILD_TEMPLATE = $(CURDIR)/../../Templates_build_sdp800
BOARD=rpi5
INSTALL=../install

export ARCH = aarch64le
export PROCESSOR = aarch64le

.PHONY: all clean

#all: ifs-$(BOARD).bin ifs-$(BOARD).ui
all: ifs-$(BOARD)-hyp.bin disk.img

# to boot QNX ifs-rpi4.bin IFS image, add "kernel=ifs-rpi4.bin" to bootable microSD card config.txt file.
# ifs-$(BOARD).bin: $(BOARD).build
# 	$(HOST_MKIFS) -v -r$(INSTALL) $(MKIFSFLAGS) $^ $@

# ifs-$(BOARD).raw: $(BOARD).build
# 	$(CP_HOST) $(BOARD).build $(BOARD)-go.build
# 	$(HOST_SED) -i 's/u reg/u arg/' $(BOARD)-go.build
# 	$(HOST_MKIFS) -v -r$(INSTALL) $(MKIFSFLAGS)  $(BOARD)-go.build $@
# 	$(RM_HOST) $(BOARD)-go.build

# To boot from U-boot "bootm" command:
#  - load ifs-rpi4.ui IFS image to 0x80000
#  - load bcm2711-rpi-4-b.dtb to ${fdt_addr}
#  - run "bootm 0x80000 - ${fdt_addr}"
# ifs-$(BOARD).ui: ifs-$(BOARD).bin
# 	$(HOST_MKI) -a $(IMAGE_LOAD_ADDR) -A arm64 $^ $@
	
# Hypervisor (Hyp) Image Target
ifs-$(BOARD)-hyp.bin: $(BOARD)-hypervisor.build
	@echo "Creating image: $@ from build file: $<"
	$(HOST_MKIFS) -v -r$(INSTALL) $(MKIFSFLAGS) $^ $@

clean:
	@echo "Cleaning generated files for $(BOARD)"
	$(RM_HOST) ifs-$(BOARD).* *.sym
	$(RM_HOST) ifs-$(BOARD)-hyp.* *.sym
	$(RM_HOST) disk.img part_qnx_data.img part_dos_boot.img

# If the hypervisor boot image variant exists, it will be included in the
# disk image below.
part_dos_boot.img: $(BOARD)-part_dos_boot.build ifs-$(BOARD)-hyp.bin
	@echo "Creating DOS boot image: $@ from prerequisites: $^"
	# Create the DOS partition image for UEFI boot mode
	mkfatfsimg -vv $< $@

# The QNX6 data partition bakes in the guest IFS images + qvmconfs (see the
# "# copy the guests" section of part_qnx_data.build). Depend on them so a rebuilt
# guest triggers a data-partition -> disk.img rebuild. This is the fix for
# "I rebuilt the guest but the host image didn't update".
GUEST_DIR := ../../qnx_guests/images
GUEST_IMAGES := \
	$(GUEST_DIR)/guest-1/qnx800-guest-1.ifs \
	$(GUEST_DIR)/guest-1/qnx-guest-1.qvmconf \
	$(GUEST_DIR)/guest-2/qnx800-guest-2.ifs \
	$(GUEST_DIR)/guest-2/qnx-guest-2.qvmconf

# Safety net for host-only builds: materialize a guest IFS if it's missing by
# delegating to its own Makefile. (Staleness of an existing IFS is handled by the
# guest build itself; `make` at the repo root also builds qnx_guests before qnx_host.)
$(GUEST_DIR)/guest-1/qnx800-guest-1.ifs:
	$(MAKE) -C $(GUEST_DIR)/guest-1
$(GUEST_DIR)/guest-2/qnx800-guest-2.ifs:
	$(MAKE) -C $(GUEST_DIR)/guest-2

part_qnx_data.img: part_qnx_data.build $(GUEST_IMAGES)
	@echo "Creating QNX data image: $@ from prerequisite: $<"
	# Create the QNX6 partition image for data storage
	mkqnx6fsimg -vv $< $@

disk_image: disk.img
disk.img: disk.cfg part_qnx_data.img part_dos_boot.img
	@echo "Creating disk image: $@ from prerequisites: $^"
	# Create the boot disk image
	# diskimage -o $@ -c $< -s 131072
	diskimage -o $@ -c $<
	echo "Disk image created for $(BOARD): $@"

-include $(BUILD_TEMPLATE)/template.mk
