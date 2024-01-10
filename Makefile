include l4t.mk

# L4T Source
L4T_SOURCE_TBZ:=public_sources.tbz2

# L4T build target
KERNEL_TBZ:=kernel_src.tbz2
KERNEL_OOT_TBZ:=kernel_oot_modules_src.tbz2
KERNEL_DISP_TBZ:=nvidia_kernel_display_driver_source.tbz2

# l4t build toolchain
TOOLCHAIN_DIRNAME:=aarch64--glibc--stable-2022.08-1
TOOLCHAIN_TAR:=${TOOLCHAIN_DIRNAME}.tar.bz2
TOOLCHAIN_DIR:=${PWD}/l4t-gcc

PWD:=$(shell pwd)

# relative path tar解凍にも使う
L4T_SOURCE_DIR:=Linux_for_Tegra/source
# absolute path
KERNEL_DIR:=${PWD}/${L4T_SOURCE_DIR}/kernel/kernel-jammy-src
KERNEL_OUT:=${PWD}/kernel_out

# IMX708
IMX708_SRC:=Linux_for_Tegra/source/nvidia-oot/drivers/media/i2c/nv_imx708.c

export ARCH=arm64
export CROSS_COMPILE=${TOOLCHAIN_DIR}/bin/aarch64-buildroot-linux-gnu-
export KERNEL_HEADERS=${KERNEL_DIR}
export KERNEL_DEF_CONFIG=defconfig

.PHONY: build extract toolchain check_dt

build: extract toolchain
	make build_kernel
	make build_dtb
	make build_modules

build_kernel: extract toolchain
# Reference: https://docs.nvidia.com/jetson/archives/r36.2/DeveloperGuide/SD/Kernel/KernelCustomization.html#building-the-jetson-linux-kernel
	make -C ${L4T_SOURCE_DIR}/kernel

build_dtb: extract toolchain
	make -C ${L4T_SOURCE_DIR} dtbs

build_modules: extract toolchain
	make -C ${L4T_SOURCE_DIR} modules

apply_patch: ${IMX708_SRC}
${IMX708_SRC}: build_modules build_dtb
	patch -p1 < patch/l4t-${MAJOR}.${MINOR}/imx708.patch

# extract kernel source and toolchain
extract: ${KERNEL_DIR}

${KERNEL_DIR}: ${L4T_SOURCE_DIR}/${KERNEL_TBZ} ${L4T_SOURCE_DIR}/${KERNEL_OOT_TBZ} ${L4T_SOURCE_DIR}/${KERNEL_DISP_TBZ}
	cd ${L4T_SOURCE_DIR} && find ./ -type f -name "*.tbz2" -exec tar xjf {} \;
	touch ${KERNEL_DIR}

${L4T_SOURCE_DIR}/${KERNEL_TBZ}: ${L4T_SOURCE_TBZ}
	tar -xjf ${L4T_SOURCE_TBZ} ${L4T_SOURCE_DIR}/${KERNEL_TBZ}
	touch ${L4T_SOURCE_DIR}/${KERNEL_TBZ}

${L4T_SOURCE_DIR}/${KERNEL_OOT_TBZ}: ${L4T_SOURCE_TBZ}
	tar -xjf ${L4T_SOURCE_TBZ} ${L4T_SOURCE_DIR}/${KERNEL_OOT_TBZ}
	touch ${L4T_SOURCE_DIR}/${KERNEL_OOT_TBZ}

${L4T_SOURCE_DIR}/${KERNEL_DISP_TBZ}: ${L4T_SOURCE_TBZ}
	tar -xjf ${L4T_SOURCE_TBZ} ${L4T_SOURCE_DIR}/${KERNEL_DISP_TBZ}
	touch ${L4T_SOURCE_DIR}/${KERNEL_DISP_TBZ}

${L4T_SOURCE_TBZ}:
	wget -c ${SOURCE_ADDR}/${L4T_SOURCE_TBZ}

toolchain: ${CROSS_COMPILE}gcc
${CROSS_COMPILE}gcc: ${TOOLCHAIN_DIR}/${TOOLCHAIN_TAR}
	cd ${TOOLCHAIN_DIR} && tar xf ${TOOLCHAIN_TAR} --strip-components 1 ${TOOLCHAIN_DIRNAME}
	touch ${CROSS_COMPILE}gcc

${TOOLCHAIN_DIR}/${TOOLCHAIN_TAR}:
	mkdir -p ${TOOLCHAIN_DIR}
	cd ${TOOLCHAIN_DIR} && wget ${TOOL_CHAIN_ADDR}/${TOOLCHAIN_TAR}

.PHONY: setup
setup:
	sudo apt install -y wget bzip2 build-essential flex bison bc libssl-dev

.PHONY: clean
clean:
	rm -rf ${KERNEL_OUT}
	rm -rf ${TOOLCHAIN_DIR}
	rm -rf ${L4T_SOURCE_DIR}
	rm -rf ${L4T_SOURCE_TBZ}

#
# remote copy to orin-nano
#
JETSON_TARGET?=orin-nano
WORKDIR:=~/imx708
CAMERA_DTBO:=tegra234-p3767-camera-p3768-imx708-dual.dtbo
DTBS_DIR:=Linux_for_Tegra/source/nvidia-oot/device-tree/platform/generic-dts/dtbs
IMX708_KO:=nv_imx708.ko
CAMERA_MODULE_DIR=Linux_for_Tegra/source/nvidia-oot/drivers/media/i2c

.PHONY: cp
cp:
	rsync -av ${DTBS_DIR}/${CAMERA_DTBO} ${JETSON_TARGET}:${WORKDIR}/
	rsync -av ${CAMERA_MODULE_DIR}/${IMX708_KO} ${JETSON_TARGET}:${WORKDIR}/
	rsync -av imx708/l4t-${MAJOR}.${MINOR}/scripts/* ${JETSON_TARGET}:${WORKDIR}/
