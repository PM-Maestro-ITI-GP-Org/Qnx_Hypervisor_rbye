# Host-side deps for vdev-virtio-gpu (libepoxy + virglrenderer) are built + staged
# ONCE by build-deps.sh, which also writes gpu/vdev-virtio-gpu/paths.txt. It's guarded
# by a stamp so it does NOT run on every `make`. Force a re-stage/rebuild with:
#   make deps-clean && make          (or: rm .deps-built.stamp)
DEPS_STAMP := .deps-built.stamp

# All GPU submodules live under gpu/ (vdev-virtio-gpu, virglrenderer, libepoxy).
# libepoxy/virglrenderer are built by build-deps.sh and the vdev is built explicitly
# below, so the generic app loop skips the whole gpu/ dir.
GPU_DIR = gpu
SKIP_DIRS = $(GPU_DIR)

.PHONY: all clean deps deps-clean

# Build order: deps (stamp) -> gpu/vdev-virtio-gpu (needs paths.txt) -> other app dirs.
all: $(DEPS_STAMP)
	@set -e; \
	echo "==> Building $(GPU_DIR)/vdev-virtio-gpu"; \
	$(MAKE) -C $(GPU_DIR)/vdev-virtio-gpu; \
	for dir in */; do \
		dir=$${dir%/}; \
		case " $(SKIP_DIRS) " in *" $$dir "*) continue ;; esac; \
		if [ -f "$$dir/Makefile" ] || [ -f "$$dir/GNUmakefile" ]; then \
			echo "==> Building $$dir"; \
			$(MAKE) -C "$$dir"; \
		fi; \
	done

# Run the dependency build/stage once. The stamp only re-fires if build-deps.sh
# itself changes; use `make deps-clean` to force a full re-stage after editing a lib.
deps: $(DEPS_STAMP)
$(DEPS_STAMP): build-deps.sh
	./build-deps.sh
	@touch $@

deps-clean:
	rm -f $(DEPS_STAMP)

# Clean the gpu/ submodules explicitly (they're nested, so the plain */ glob misses
# them), then every other app dir.
clean:
	@set -e; \
	for dir in $(GPU_DIR)/*/ */; do \
		dir=$${dir%/}; \
		if [ -f "$$dir/Makefile" ] || [ -f "$$dir/GNUmakefile" ]; then \
			echo "==> Cleaning $$dir"; \
			$(MAKE) -C "$$dir" clean || true; \
		fi; \
	done
	rm -f $(DEPS_STAMP) qnx-aarch64le.ini
	rm -rf $(GPU_DIR)/stage
