# The raspberrypi mesa recipe builds vc4/v3d/zink but not virgl. kmscube in this
# guest renders through the qvm virtio-gpu vdev (virglrenderer on the host V3D), so
# force the virgl gallium driver in. GL_RENDERER then reads "virgl".
GALLIUMDRIVERS:append = ",virgl"
