# Host-side deps for vdev-virtio-gpu (libepoxy + virglrenderer) are built + staged
# ONCE by build-deps.sh, which also writes vdev-virtio-gpu/paths.txt. It's guarded by
# a stamp so it does NOT run on every `make`. Force a re-stage/rebuild with:
#   make deps-clean && make          (or: rm .deps-built.stamp)
DEPS_STAMP := .deps-built.stamp

# These are handled by build-deps.sh, so the generic loop below skips them.
DEP_DIRS = libepoxy virglrenderer

.PHONY: all clean deps deps-clean

# Build order: deps (stamp) -> vdev-virtio-gpu (needs paths.txt) -> every other app dir.
all: $(DEPS_STAMP)
	@set -e; \
	echo "==> Building vdev-virtio-gpu"; \
	$(MAKE) -C vdev-virtio-gpu; \
	for dir in */; do \
		dir=$$(echo $$dir | tr -d /); \
		case " $(DEP_DIRS) vdev-virtio-gpu " in *" $$dir "*) continue ;; esac; \
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

clean:
	@set -e; \
	for dir in */; do \
		dir=$$(echo $$dir | tr -d /); \
		if [ -f "$$dir/Makefile" ] || [ -f "$$dir/GNUmakefile" ]; then \
			echo "==> Cleaning $$dir"; \
			$(MAKE) -C "$$dir" clean || true; \
		fi; \
	done
	rm -f $(DEPS_STAMP) qnx-aarch64le.ini
	rm -rf stage
