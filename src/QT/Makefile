
.PHONY: all clean

all:
	@bash -c 'set -e; $(MAKE) -C qt6-qnx-libs; \
		for d in */; do \
			[ "$$d" = "qt6-qnx-libs/" ] && continue; \
			[ -f "$$d/Makefile" ] || continue; \
			echo "==> Building $$d"; \
			$(MAKE) -C "$$d"; \
		done'

clean:
	for d in */; do \
		[ "$$d" = "qt6-qnx-libs/" ] && continue; \
		[ -f "$$d/Makefile" ] || continue; \
		$(MAKE) -C "$$d" clean || true; \
	done
