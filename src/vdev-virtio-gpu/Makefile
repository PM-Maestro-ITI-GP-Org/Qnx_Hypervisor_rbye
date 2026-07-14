VIRGL = ../virglrenderer
STAGE = /home/maxmaster/test/stage

PATCHES = patches/repo.patch

all: vdev-virtio-gpu.so

vdev-virtio-gpu.c.patched: vdev-virtio-gpu.c.orig $(PATCHES)
	cp -f vdev-virtio-gpu.c.orig vdev-virtio-gpu.c
	patch -Np0 < patches/repo.patch 2>/dev/null || true
	@touch $@

vdev-virtio-gpu.so: vdev-virtio-gpu.c.patched vdev-virtio-gpu.c virtio_gpu.h edid_build.inc
	qcc -Vgcc_ntoaarch64le -shared -fPIC -Wall -o $@ vdev-virtio-gpu.c \
	  -I$(VIRGL)/src -I$(VIRGL)/build-qnx/src \
	  -L$(VIRGL)/build-qnx/src -lvirglrenderer \
	  -L$(STAGE)/lib -Wl,-rpath-link,$(STAGE)/lib \
	  -lscreen -lhypS

clean:
	rm -f vdev-virtio-gpu.so vdev-virtio-gpu.c.patched vdev-virtio-gpu.c.rej

.PHONY: all clean
