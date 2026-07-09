.PHONY: all clean

all:
	@set -e; \
	for dir in */; do \
		if [ -f "$$dir/Makefile" ] || [ -f "$$dir/GNUmakefile" ]; then \
			echo "==> Building $$dir"; \
			$(MAKE) -C "$$dir"; \
		fi; \
	done

clean:
	@set -e; \
	for dir in */; do \
		if [ -f "$$dir/Makefile" ] || [ -f "$$dir/GNUmakefile" ]; then \
			echo "==> Cleaning $$dir"; \
			$(MAKE) -C "$$dir" clean || true; \
		fi; \
	done
