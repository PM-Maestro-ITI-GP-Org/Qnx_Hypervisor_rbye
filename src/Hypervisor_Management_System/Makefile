QNX800_DIR := ../../qnx800
MQTT_LIBS := ../mqtt_libs/install_qnx
CJSON_LIBS := $(MQTT_LIBS)/cJSON

CFLAGS := -std=c11 -D_POSIX_C_SOURCE=200809L -D_ALL_SOURCE -Wall -Wextra -O2 \
	-I$(MQTT_LIBS)/include \
	-I$(CJSON_LIBS)/include

LDFLAGS := -L$(MQTT_LIBS)/lib -lmosquitto -lsocket -lm

BUILD_DIR = build
TARGET    = $(BUILD_DIR)/hms

SRCS := main.c \
        guest/guest.c guest/discoverer.c guest/lifecycle.c \
        ssh/client.c ssh/session_pool.c ssh/shell.c \
        config/config.c \
        utils/proc.c \
        mqtt/mqtt_client.c \
        ota/ota.c

.PHONY: all qnx clean deploy

all: qnx

qnx:
	@bash -c 'set -e; \
		. "$(QNX800_DIR)/qnxsdp-env.sh"; \
		mkdir -p $(BUILD_DIR)/guest $(BUILD_DIR)/ssh $(BUILD_DIR)/config $(BUILD_DIR)/utils $(BUILD_DIR)/mqtt $(BUILD_DIR)/ota; \
		for src in $(SRCS); do \
			obj=$(BUILD_DIR)/$${src%.c}.o; \
			"$$QNX_HOST/usr/bin/qcc" -Vgcc_ntoaarch64le $(CFLAGS) -c -o "$$obj" "$$src"; \
		done; \
		"$$QNX_HOST/usr/bin/qcc" -Vgcc_ntoaarch64le $(LDFLAGS) -o $(TARGET) $(SRCS:%.c=$(BUILD_DIR)/%.o)'

# Deploy hms + its MQTT runtime library to the QNX host.
# Usage: make deploy HOST=root@192.168.2.2
HOST ?= root@192.168.2.2

deploy: qnx
	scp $(TARGET) $(HOST):/bin/hms
	scp $(MQTT_LIBS)/lib/libmosquitto.so.2.1.2 $(HOST):/lib/libmosquitto.so.2.1.2
	ssh $(HOST) 'ln -sf libmosquitto.so.2.1.2 /lib/libmosquitto.so.1'
	scp config/hms.conf $(HOST):/etc/hms.conf
	@echo "Deployed. Run 'hms' on the host (verify with: ldd /bin/hms)"

clean:
	rm -rf $(BUILD_DIR)
