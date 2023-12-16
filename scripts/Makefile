BASE_DTB:=tegra234-p3767-0003-p3768-0000-a0.dtb
CAMERA_DTB:=tegra234-p3767-camera-p3768-imx708.dtbo
OUT_DTB=/boot/kernel_tegra234-p3767-0003-p3768-0000-a0-user-custom.dtb

.PHONY: overlay
overlay:
	sudo fdtoverlay -i ${BASE_DTB} -o ${OUT_DTB} ${CAMERA_DTB}

PHONY: insmod
insmod:
	sudo insmod nv_imx708.ko
	v4l2-ctl --list-devices
	sudo media-ctl -p -d /dev/media0
	sudo systemctl restart nvargus-daemon

PHONY: rmmod
rmmod:
	sudo rmmod nv_imx708.ko
	sudo systemctl restart nvargus-daemon

.PHONY: setup
setup:
	sudo apt update
	sudo apt install -y nvidia-l4t-jetson-multimedia-api \
	cmake build-essential pkg-config libx11-dev libgtk-3-dev \
	libexpat1-dev libjpeg-dev libgstreamer1.0-dev v4l-utils tree
	cp -r /usr/src/jetson_multimedia_api .
	cd jetson_multimedia_api/argus/ && \
		mkdir -p build && \
		cd build && \
		cmake ..  && \
		make
	cp jetson_multimedia_api/argus/build/apps/camera/ui/camera/argus_camera .

.PHONY: check0
check0:
	v4l2-ctl -d /dev/video0 --set-ctrl bypass_mode=0 --stream-mmap --stream-count=1 --stream-to=test.raw

check_overlay:
	dtc -I dtb -O dts -o ref.dts /boot/dtb/kernel_tegra234-p3767-0003-p3768-0000-a0.dtb
	dtc -I dtb -O dts -o custom.dts ${OUT_DTB}
	diff ref.dts custom.dts > diff.txt || true

# need sudo
.PHONY: trace.enable
trace.enable:
	echo 1 > /sys/kernel/debug/tracing/tracing_on
	echo 30720 > /sys/kernel/debug/tracing/buffer_size_kb
	echo 1 > /sys/kernel/debug/tracing/events/tegra_rtcpu/enable
	echo 1 > /sys/kernel/debug/tracing/events/freertos/enable
	echo 2 > /sys/kernel/debug/camrtc/log-level
	echo > /sys/kernel/debug/tracing/trace

# need sudo
.PHONY: trace.disable
trace.disable:
	echo 0 > /sys/kernel/debug/tracing/events/tegra_rtcpu/enable	
	echo 0 > /sys/kernel/debug/tracing/events/freertos/enable
	echo 0 > /sys/kernel/debug/camrtc/log-level
	echo > /sys/kernel/debug/tracing/trace

.PHONY: trace.log
trace.log:
	sudo cat /sys/kernel/debug/tracing/trace > log.txt
