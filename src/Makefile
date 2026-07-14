BUILD_ORDER = libepoxy virglrenderer vdev-virtio-gpu

.PHONY: all clean

all:
	@set -e; \
	for dir in $(BUILD_ORDER); do \
		echo "==> Building $$dir"; \
		$(MAKE) -C "$$dir"; \
	done; \
	for dir in */; do \
		dir=$$(echo $$dir | tr -d /); \
		case " $(BUILD_ORDER) " in *" $$dir "*) ;; *) \
			if [ -f "$$dir/Makefile" ] || [ -f "$$dir/GNUmakefile" ]; then \
				echo "==> Building $$dir"; \
				$(MAKE) -C "$$dir"; \
			fi ;; \
		esac; \
	done

clean:
	@set -e; \
	for dir in */; do \
		dir=$$(echo $$dir | tr -d /); \
		if [ -f "$$dir/Makefile" ] || [ -f "$$dir/GNUmakefile" ]; then \
			echo "==> Cleaning $$dir"; \
			$(MAKE) -C "$$dir" clean || true; \
		fi; \
	done
