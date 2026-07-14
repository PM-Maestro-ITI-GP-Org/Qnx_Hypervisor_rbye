SUMMARY = "sysvinit script that launches kmscube at boot on /dev/dri/card0"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

SRC_URI = "file://kmscube.init"

S = "${WORKDIR}"

inherit update-rc.d
INITSCRIPT_NAME = "kmscube"
INITSCRIPT_PARAMS = "start 99 5 ."

do_install() {
    install -d ${D}${sysconfdir}/init.d
    install -m 0755 ${WORKDIR}/kmscube.init ${D}${sysconfdir}/init.d/kmscube
}

RDEPENDS:${PN} = "kmscube"
