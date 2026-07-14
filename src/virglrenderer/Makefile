BUILD_DIR = build-qnx
QNX_SDP_ENV = /home/maxmaster/data/software/Qnx_Hypervisor_rbye/qnx800/qnxsdp-env.sh
STAGE = /home/maxmaster/test/stage
MESON_CROSS = /home/maxmaster/test/qnx-aarch64le.ini
TARGET_SO = $(BUILD_DIR)/src/libvirglrenderer.so.1.11.0
QNX_PATCH = $(abspath ../vdev-virtio-gpu/patches/virglrenderer-qnx.patch)

.PHONY: all clean

all: $(TARGET_SO)

patch-applied.stamp:
	@echo "==> Applying virglrenderer-qnx.patch"
	patch -Np1 < $(QNX_PATCH) 2>/dev/null || true
	@touch $@

$(TARGET_SO): patch-applied.stamp $(BUILD_DIR)/build.ninja
	. $(QNX_SDP_ENV) && \
	ninja -C $(BUILD_DIR) src/libvirglrenderer.so.1.11.0
	patchelf --replace-needed libdrm.so.1 libdrm.so.2 $(TARGET_SO) 2>/dev/null || true

$(BUILD_DIR)/build.ninja:
	. $(QNX_SDP_ENV) && \
	export PKG_CONFIG_LIBDIR=$(STAGE)/lib/pkgconfig && \
	meson setup $(BUILD_DIR) --cross-file $(MESON_CROSS) \
	  --prefix $(STAGE) \
	  -Dplatforms=egl -Drender-server-worker=thread \
	  -Dc_link_args="-lm -leventfd" \
	  -Ddefault_library=shared

install: all
	cp -f $(BUILD_DIR)/src/libvirglrenderer.so* $(STAGE)/lib/
	mkdir -p $(STAGE)/include
	cp -f $(BUILD_DIR)/src/virgl-version.h $(STAGE)/include/ 2>/dev/null || true

clean:
	rm -rf $(BUILD_DIR) patch-applied.stamp

.PHONY: all install clean
