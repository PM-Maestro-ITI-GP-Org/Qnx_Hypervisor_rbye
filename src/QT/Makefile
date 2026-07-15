QNX_ENV := /media/gemy/Linux_Workspace/ITI/QNX_WS/software_platform/qnxsdp-env.sh

.PHONY: all clean

all:
	@bash -c 'source $(QNX_ENV); \
		$(MAKE) -C qt6-qnx-libs; \
		for d in */; do \
			[ "$$d" = "qt6-qnx-libs/" ] && continue; \
			[ -f "$$d/Makefile" ] || continue; \
			echo "==> Building $$d"; \
			$(MAKE) -C "$$d"; \
		done'

clean:
	for d in */; do \
		[ -f "$$d/Makefile" ] || continue; \
		$(MAKE) -C "$$d" clean || true; \
	done
