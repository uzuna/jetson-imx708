CAMERA_DTBO:=tegra234-p3767-camera-p3768-imx708-dual.dtbo

VIDEOS = 0 1

check: $(addprefix check-, $(VIDEOS))
check-fps: $(addprefix check-, $(VIDEOS))

.PHONY: overlay
overlay:
	sudo cp tegra234-p3767-camera-p3768-imx708-dual.dtbo /boot/
	sudo /opt/nvidia/jetson-io/jetson-io.py

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

check.%:
	v4l2-ctl -d /dev/video${@:check.%=%} --set-ctrl bypass_mode=0 --stream-mmap --stream-count=1 --stream-to=test.raw

check-fps.%:
	v4l2-ctl -d /dev/video${@:check-fps.%=%} --set-ctrl bypass_mode=0 --stream-mmap --stream-count=60

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

PHONY: setup
setup:
	sudo apt update
	sudo apt install v4l-utils
