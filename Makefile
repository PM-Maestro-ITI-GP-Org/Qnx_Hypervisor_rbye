
SUBDIRS := qnx_host qnx_guests src

.PHONY: all clean $(SUBDIRS) qnx_install 

all: $(SUBDIRS)

qnx_host: qnx_install qnx_guests apps_src
	$(MAKE) -C qnx_host

apps_src:
	$(MAKE) -C src

qnx_install:
	mkdir -p qnx_host/install
	mkdir -p qnx_guests/install

qnx_guests: apps_src
	$(MAKE) -C qnx_guests

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done