#!/bin/ksh
#
# Start the RPi5 HOST graphics stack: QNX Screen on the native V3D.
#
# `screen` loads the WFD/DRI driver (libWFDrpi5.so / v3d_dri.so) which spawns
# drm-rpi5 and creates /dev/screen -- so we only launch `screen` here; there's no
# separate drm-rpi5 command to run.
#
# Run this BEFORE launching any guest that uses vdev-virtio-gpu: the vdev gates its
# EGL/virgl context on the host's /dev/screen and the whole qvm process exits if an
# EGL call runs before Screen is up.
#
# Display modes come from graphics-host-rpi5.conf (display 1 = the guest's 1024x600
# HDMI panel). Edit that file to match your panels.

CONF=/lib/graphics/drm-rpi5/graphics-host-rpi5.conf

echo "Starting host graphics (QNX Screen, $CONF) ..."
screen -c $CONF &

if ! waitfor /dev/screen 10; then
    echo "ERROR: /dev/screen did not appear -- host graphics failed to start."
    echo "       Check $CONF and that the HDMI panels match its video-modes."
    exit 1
fi
echo "  Host graphics up: /dev/screen ready (display 1 = 1024x600)."
