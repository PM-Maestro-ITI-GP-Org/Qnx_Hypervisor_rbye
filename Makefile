SUBDIRS := qnx_host qnx_guests

.PHONY: all clean $(SUBDIRS) qnx_install

all: $(SUBDIRS)

qnx_host: qnx_install
	$(MAKE) -C qnx_host

qnx_install:
	mkdir -p qnx_host/install
	mkdir -p qnx_guests/install

qnx_guests:
	$(MAKE) -C qnx_guests

clean:
	for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done