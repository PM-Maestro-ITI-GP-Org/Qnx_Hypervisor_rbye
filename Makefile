
SUBDIRS := qnx_host qnx_guests src

.PHONY: all clean $(SUBDIRS) qnx_install flash-sd

all: $(SUBDIRS)

qnx_host: qnx_install qnx_guests apps_src
	$(MAKE) -C qnx_host

apps_src:
	$(MAKE) -C src

qnx_install:
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