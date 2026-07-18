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
# NOTE: $$ escapes '$' for the shell; commented-out lines are dropped first so a
# disabled entry never becomes a prerequisite, and grep -o keeps only the source path.
IFS_EXT_SRCS := $(sort $(shell grep -vE '^[[:space:]]*#' $(F_NAME).build 2>/dev/null | grep -oE '=[[:space:]]*\.\./[^ ]+[[:space:]]*$$' | sed -E 's/^=[[:space:]]*//; s/[[:space:]]+$$//' | grep -v '[$$][{]'))

# Only paths that exist become prerequisites. Nothing here knows how to build a
# missing app binary, so listing one would abort with make's opaque
#   *** No rule to make target '../../../src/.../MotorDataClient', needed by '....ifs'
# instead of saying what is actually wrong. Missing entries are collected and
# reported by name in the recipe below.
#
# Directory sources (a Qt deploy/ tree, someip's lib/) expand to the files inside
# them. Depending on the directory itself rebuilt the IFS on *every* run: a
# directory's mtime is bumped whenever anything is re-copied into it, even when the
# contents are byte-for-byte identical, which is exactly what the Qt deploy step does.
IFS_EXT_DEPS := $(foreach f,$(IFS_EXT_SRCS),\
                  $(if $(wildcard $(f)/.),$(shell find $(f) -type f 2>/dev/null),$(wildcard $(f))))
# $(strip) matters: foreach joins its results with spaces, so with nothing missing
# this would be a whitespace-only string, which "ifneq (...,)" reads as non-empty.
IFS_MISSING  := $(strip $(foreach f,$(IFS_EXT_SRCS),$(if $(wildcard $(f)),,$(f))))
IFS_INSTALL_DEPS := $(shell find $(INSTALL) -type f 2>/dev/null)

# These are timestamp triggers built elsewhere, never here. The empty recipe stops
# make from hunting for a built-in implicit rule to "remake" them (e.g. a meson
# <lib>.p directory beside <lib> matches the built-in Pascal rule "%: %.p").
# $(sort) dedupes: a .build may stage the same source file at two image paths.
$(sort $(IFS_EXT_DEPS) $(IFS_INSTALL_DEPS)): ;

# A missing source is deliberately not a prerequisite, so an already-built IFS would
# otherwise be reported "up to date" with no hint that a staged file has gone away.
ifneq ($(IFS_MISSING),)
$(warning $(F_NAME).build stages files that do not exist: $(IFS_MISSING))
endif

$(F_NAME).ifs: $(F_NAME).build $(IFS_EXT_DEPS) $(IFS_INSTALL_DEPS)
ifneq ($(IFS_MISSING),)
	@echo "ERROR: $(F_NAME).build stages files that have not been built:" >&2
	@for f in $(IFS_MISSING); do echo "         $$f" >&2; done
	@echo "       Build them first (e.g. 'make -C ../../../src'), then re-run." >&2
	@exit 1
endif
	$(HOST_MKIFS) -a$(F_NAME) -r$(INSTALL) -v $(MKIFSFLAGS) $< $@

-include $(BUILD_TEMPLATE)/template.mk

