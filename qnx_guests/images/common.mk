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

# Rebuild the IFS when the .build changes OR any project file it stages changes.
#  - IFS_EXT_DEPS: sources the .build references by relative path (../...) — conf
#    files, in-repo app builds, oss libs, cross-referenced qnx_host installs. Only
#    end-of-line sources are matched, and ${...}/bare-name/SDP sources are skipped
#    (they rarely change; add them by hand if you need them tracked).
#  - IFS_INSTALL_DEPS: the local install/ tree (project-built drivers/libs that
#    mkifs pulls in via -r$(INSTALL)).
# NOTE: $$ escapes '$' for the shell; grep -o keeps only the matched source path.
IFS_EXT_DEPS := $(shell grep -oE '=[[:space:]]*\.\./[^ ]+[[:space:]]*$$' $(F_NAME).build 2>/dev/null | sed -E 's/^=[[:space:]]*//; s/[[:space:]]+$$//' | grep -v '[$$][{]')
IFS_INSTALL_DEPS := $(shell find $(INSTALL) -type f 2>/dev/null)

$(F_NAME).ifs: $(F_NAME).build $(IFS_EXT_DEPS) $(IFS_INSTALL_DEPS)
	$(HOST_MKIFS) -a$(F_NAME) -r$(INSTALL) -v $(MKIFSFLAGS) $< $@

-include $(BUILD_TEMPLATE)/template.mk

