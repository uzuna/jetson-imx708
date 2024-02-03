include l4t.mk

# L4T build target
KERNEL_TBZ:=kernel_src.tbz2

# L4T Source
L4T_SOURCE_TBZ:=public_sources.tbz2
TOOLCHAIN_TAR:=aarch64--glibc--stable-final.tar.gz

# l4t build toolchain
TOOLCHAIN_DIRNAME:=aarch64--glibc--stable-final
TOOLCHAIN_TAR:=${TOOLCHAIN_DIRNAME}.tar.bz2
TOOLCHAIN_DIR:=${PWD}/l4t-gcc

PWD:=$(shell pwd)

# relative path tar解凍にも使う
L4T_SOURCE_DIR:=Linux_for_Tegra/source/public
# absolute path
KERNEL_DIR:=${PWD}/${L4T_SOURCE_DIR}/kernel/kernel-5.10
KERNEL_OUT:=${PWD}/kernel_out

IMX708_SRC:=Linux_for_Tegra/source/public/kernel/nvidia/drivers/media/i2c/nv_imx708.c

export ARCH=arm64
export CROSS_COMPILE=${TOOLCHAIN_DIR}/bin/aarch64-buildroot-linux-gnu-

.PHONY: build extract toolchain check_dt

build: toolchain
	make -C ${KERNEL_DIR} tegra_defconfig
	make build_image
	make build_dtb
	make build_modules

build_image: toolchain
	make -C ${KERNEL_DIR} -j`nproc` Image

build_dtb: toolchain
	make -C ${KERNEL_DIR} -j`nproc` dtbs

build_modules: toolchain
	make -C ${KERNEL_DIR} -j`nproc` modules

apply_patch: ${IMX708_SRC}
${IMX708_SRC}:
	patch -p1 < patch/l4t-${MAJOR}.${MINOR}/imx708.patch

# extract kernel source and toolchain
extract: ${KERNEL_DIR}

${KERNEL_DIR}: ${L4T_SOURCE_DIR}/${KERNEL_TBZ}
	cd ${L4T_SOURCE_DIR} && tar -xjf ${KERNEL_TBZ}
	touch ${KERNEL_DIR}

${L4T_SOURCE_DIR}/${KERNEL_TBZ}: ${L4T_SOURCE_TBZ}
	tar -xjf ${L4T_SOURCE_TBZ} ${L4T_SOURCE_DIR}/${KERNEL_TBZ}
	touch ${L4T_SOURCE_DIR}/${KERNEL_TBZ}

${L4T_SOURCE_TBZ}:
	wget -c ${SOURCE_ADDR}/${L4T_SOURCE_TBZ}

toolchain: ${CROSS_COMPILE}gcc
${CROSS_COMPILE}gcc: ${TOOLCHAIN_DIR}/${TOOLCHAIN_TAR}
	cd ${TOOLCHAIN_DIR} && tar xf ${TOOLCHAIN_TAR}
	touch ${CROSS_COMPILE}gcc

${TOOLCHAIN_DIR}/${TOOLCHAIN_TAR}:
	mkdir -p ${TOOLCHAIN_DIR}
	cd ${TOOLCHAIN_DIR} && wget -O ${TOOLCHAIN_TAR} https://developer.nvidia.com/embedded/jetson-linux/bootlin-toolchain-gcc-93

.PHONY: setup
setup:
	sudo apt install -y wget bzip2 build-essential flex bison bc libssl-dev

.PHONY: clean
clean:
	rm -rf ${TOOLCHAIN_DIR}
	rm -rf ${L4T_SOURCE_DIR}
	rm -rf ${L4T_SOURCE_TBZ}

#
# for Debug and Development
#
BASE_NAME:=tegra234-p3767-0003-p3768-0000-a0
CAMERA_NAME:=tegra234-p3767-camera-p3768-imx708-dual
BASE_DTB:=${KERNEL_DIR}/arch/arm64/boot/dts/nvidia/${BASE_NAME}.dtb
CAMERA_DTBO:=${KERNEL_DIR}/arch/arm64/boot/dts/nvidia/${CAMERA_NAME}.dtbo

check_dt:
	dtc -I dtb -O dts -o ${BASE_NAME}.dts ${BASE_DTB}

check_overlay:
	fdtoverlay -i ${BASE_DTB} -o custom.dtb ${CAMERA_DTBO}
	dtc -I dtb -O dts -o custom.dts custom.dtb
	diff ${BASE_NAME}.dts custom.dts > diff.txt || true

#
# remote copy to orin-nano
#
JETSON_TARGET:=orin-nano
WORKDIR:=~/imx708

cp:
	rsync -av ${BASE_DTB} ${JETSON_TARGET}:${WORKDIR}
	rsync -av ${CAMERA_DTBO} ${JETSON_TARGET}:${WORKDIR}
	rsync -av ${KERNEL_DIR}/drivers/media/i2c/nv_imx708.ko ${JETSON_TARGET}:${WORKDIR}
	rsync -av scripts/Makefile ${JETSON_TARGET}:${WORKDIR}
