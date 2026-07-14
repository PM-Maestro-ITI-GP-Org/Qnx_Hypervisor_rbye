#!/bin/sh
# Host-side demo autostart (QNX). Call from the IFS startup script, or by hand.
#
#   screen 2 (HDMI2): host gles2-gears  -> proves QNX owns the V3D + display
#   screen 1 (HDMI1): guest kmscube     -> renders through vdev-virtio-gpu/virgl
#
# The vdev gates EGL on the host Screen, so wait for /dev/screen before launching
# anything (graphics_start.sh must have started `screen` already).

DIR=$(dirname "$0")

if ! waitfor /dev/screen 30; then
	echo "start-demo: /dev/screen never came up - is screen running?"
	exit 1
fi

# Host GPU demo on screen 2.
gles2-gears -display=2 &

# Guest on screen 1. LD_LIBRARY_PATH must resolve vdev-virtio-gpu.so + virgl libs.
export LD_LIBRARY_PATH=/lib/dll:$LD_LIBRARY_PATH
exec qvm @"$DIR/guest.qvmconf"
