ifndef QCONFIG
QCONFIG=qconfig.mk
endif

include $(QCONFIG)

BUILD_TEMPLATE=../../../Templates_build_sdp800
INSTALL=../../install
HOST_MKIFS := mkifs

.PHONY: all clean


all: $(BOARD).ifs

clean:
	$(RM_HOST) *.ifs *.sym

$(BOARD).ifs: $(BOARD).build
	$(HOST_MKIFS) -a$(BOARD) -r$(INSTALL) -v $(MKIFSFLAGS) $^ $@

-include $(BUILD_TEMPLATE)/template.mk

