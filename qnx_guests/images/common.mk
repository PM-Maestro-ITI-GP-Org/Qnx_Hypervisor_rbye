ifndef QCONFIG
QCONFIG=qconfig.mk
endif

include $(QCONFIG)

BUILD_TEMPLATE=../../../Templates_build_sdp800
INSTALL=../../install
HOST_MKIFS := mkifs

.PHONY: all clean


all: $(F_NAME).ifs

clean:
	$(RM_HOST) *.ifs *.sym

$(F_NAME).ifs: $(F_NAME).build
	$(HOST_MKIFS) -a$(F_NAME) -r$(INSTALL) -v $(MKIFSFLAGS) $^ $@

-include $(BUILD_TEMPLATE)/template.mk

