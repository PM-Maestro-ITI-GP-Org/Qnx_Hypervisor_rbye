.PHONY: all clean

all:
	@set -e; \
	$(MAKE) -C spi_loopback; \
	for dir in */; do \
		if [ -f "$$dir/Makefile" ] || [ -f "$$dir/GNUmakefile" ]; then \
			case "$$dir" in \
				./frame_router/ | ./someip/ | ./someip/commonapi_client_server/ | ./someip/commonapi-qnx/) \
					echo "==> Building $$dir"; \
					$(MAKE) -C "$$dir"; \
					;; \
				esac; \
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
