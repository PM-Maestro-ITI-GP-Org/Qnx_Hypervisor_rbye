ifndef QCONFIG
QCONFIG=qconfig.mk
endif

include $(QCONFIG)

BUILD_TEMPLATE=../../../Templates_build_sdp800
INSTALL=../../install
HOST_MKIFS := mkifs
HOST_MKQNX6FSIMG := mkqnx6fsimg

.PHONY: all clean


# ------------------------------------------------------------------------------------
# Dependency extraction for mkifs / mkqnx6fsimg build files
#
# Rebuild an image when its .build changes OR any project file it stages changes.
# Both the IFS and the optional data disk need this, so it lives in these three
# $(call)-able functions.
#
#   build_srcs    <- sources a .build references by relative path (../...): conf files,
#                    in-repo app builds, oss libs, cross-referenced qnx_host installs.
#                    Only end-of-line sources match; ${...}/bare-name/SDP sources are
#                    skipped (they rarely change; add them by hand if you need them).
#                    Commented-out lines are dropped first, so a disabled entry never
#                    becomes a prerequisite.
#   build_deps    <- of those, the ones that exist. Directory sources (a Qt deploy/ tree,
#                    someip's lib/) expand to the files inside them: depending on the
#                    directory itself rebuilt the image on *every* run, because a
#                    directory's mtime is bumped whenever anything is re-copied into it
#                    even when the contents are byte-for-byte identical, which is exactly
#                    what the Qt deploy step does.
#   build_missing <- the ones that do not. Nothing here knows how to build a missing app
#                    binary, so listing it as a prerequisite would abort with make's opaque
#                    *** No rule to make target '../../../src/.../MotorDataClient'
#                    instead of naming the real problem. Reported explicitly instead.
#                    $(strip) matters: foreach joins with spaces, so with nothing missing
#                    this would be whitespace-only, which "ifneq (...,)" reads as non-empty.
#
# NOTE: $$ escapes '$' for the shell; grep -o keeps only the matched source path.
# ------------------------------------------------------------------------------------
build_srcs = $(sort $(shell grep -vE '^[[:space:]]*#' $(1) 2>/dev/null | grep -oE '=[[:space:]]*\.\./[^ ]+[[:space:]]*$$' | sed -E 's/^=[[:space:]]*//; s/[[:space:]]+$$//' | grep -v '[$$][{]'))
build_deps = $(foreach f,$(1),$(if $(wildcard $(f)/.),$(shell find $(f) -type f 2>/dev/null),$(wildcard $(f))))
build_missing = $(strip $(foreach f,$(1),$(if $(wildcard $(f)),,$(f))))


IFS_EXT_SRCS := $(call build_srcs,$(F_NAME).build)
IFS_EXT_DEPS := $(call build_deps,$(IFS_EXT_SRCS))
IFS_MISSING  := $(call build_missing,$(IFS_EXT_SRCS))

# The local install/ tree: project-built drivers/libs that mkifs pulls in via -r$(INSTALL).
IFS_INSTALL_DEPS := $(shell find $(INSTALL) -type f 2>/dev/null)


# ------------------------------------------------------------------------------------
# Optional data disk
#
# A guest IFS is RAM-resident, so a guest that would otherwise carry hundreds of MB of
# libraries and app payloads ships a rootfs.build alongside its IFS .build. That produces
# a QNX6 filesystem image which the guest attaches as a virtio-blk vdev and mounts at boot
# (see guest-1: rootfs.build, .rootfs-mount.sh, and the vdev in qnx-guest-1.qvmconf).
#
# Guests without a rootfs.build are unaffected: ROOTFS_IMG stays empty.
# ------------------------------------------------------------------------------------
ROOTFS_BUILD := $(wildcard rootfs.build)
ifneq ($(ROOTFS_BUILD),)
ROOTFS_IMG     := rootfs.img
ROOTFS_SRCS    := $(call build_srcs,$(ROOTFS_BUILD))
ROOTFS_DEPS    := $(call build_deps,$(ROOTFS_SRCS))
ROOTFS_MISSING := $(call build_missing,$(ROOTFS_SRCS))
endif


all: $(F_NAME).ifs $(ROOTFS_IMG)

clean:
	$(RM_HOST) *.ifs *.sym $(ROOTFS_IMG)


# These are timestamp triggers built elsewhere, never here. The empty recipe stops make
# from hunting for a built-in implicit rule to "remake" them (e.g. a meson <lib>.p
# directory beside <lib> matches the built-in Pascal rule "%: %.p").
# $(sort) dedupes: a .build may stage the same source file at two image paths, and the
# IFS and the data disk may both reference one.
$(sort $(IFS_EXT_DEPS) $(IFS_INSTALL_DEPS) $(ROOTFS_DEPS)): ;


# A missing source is deliberately not a prerequisite, so an already-built image would
# otherwise be reported "up to date" with no hint that a staged file has gone away.
ifneq ($(IFS_MISSING),)
$(warning $(F_NAME).build stages files that do not exist: $(IFS_MISSING))
endif
ifneq ($(ROOTFS_MISSING),)
$(warning $(ROOTFS_BUILD) stages files that do not exist: $(ROOTFS_MISSING))
endif

$(F_NAME).ifs: $(F_NAME).build $(IFS_EXT_DEPS) $(IFS_INSTALL_DEPS)
ifneq ($(IFS_MISSING),)
	@echo "ERROR: $(F_NAME).build stages files that have not been built:" >&2
	@for f in $(IFS_MISSING); do echo "         $$f" >&2; done
	@echo "       Build them first (e.g. 'make -C ../../../src'), then re-run." >&2
	@exit 1
endif
	$(HOST_MKIFS) -a$(F_NAME) -r$(INSTALL) -v $(MKIFSFLAGS) $< $@

$(ROOTFS_IMG): $(ROOTFS_BUILD) $(ROOTFS_DEPS)
ifneq ($(ROOTFS_MISSING),)
	@echo "ERROR: $(ROOTFS_BUILD) stages files that have not been built:" >&2
	@for f in $(ROOTFS_MISSING); do echo "         $$f" >&2; done
	@echo "       Build them first (e.g. 'make -C ../../../src'), then re-run." >&2
	@exit 1
endif
	$(HOST_MKQNX6FSIMG) -vv $< $@

-include $(BUILD_TEMPLATE)/template.mk

