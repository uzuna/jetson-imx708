MAJOR:=35
MINOR:=3.1
L4T_SOURCE_TBZ:=public_sources.tbz2
KERNEL_TBZ:=kernel_src.tbz2
SOURCE_ADDR:=https://developer.download.nvidia.com/embedded/L4T/r${MAJOR}_Release_v${MINOR}/sources
TOOLCHAIN_TAR:=aarch64--glibc--stable-final.tar.gz

PWD:=$(shell pwd)
L4T_SOURCE_DIR:=Linux_for_Tegra/source/public
TOOLCHAIN_DIR:=${PWD}/l4t-gcc
KERNEL_DIR:=${PWD}/${L4T_SOURCE_DIR}/kernel/kernel-5.10
KERNEL_OUT:=${PWD}/kernel_out

export ARCH=arm64
export CROSS_COMPILE=${TOOLCHAIN_DIR}/bin/aarch64-buildroot-linux-gnu-

.PHONY: build extract toolchain check_dt

build: extract toolchain
	mkdir -p ${KERNEL_OUT}
	make -C ${KERNEL_DIR} O=${KERNEL_OUT} tegra_defconfig
	make -C ${KERNEL_DIR} O=${KERNEL_OUT} -j8 dtbs modules

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
	cd ${TOOLCHAIN_DIR} && wget https://developer.download.nvidia.com/embedded/L4T/bootlin/${TOOLCHAIN_TAR}

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
# for Debug and Development
#
BASE_NAME:=tegra234-p3767-0003-p3768-0000-a0
CAMERA_NAME:=tegra234-p3767-camera-p3768-imx708
BASE_DTB:=${KERNEL_OUT}/arch/arm64/boot/dts/nvidia/${BASE_NAME}.dtb
CAMERA_DTBO:=${KERNEL_OUT}/arch/arm64/boot/dts/nvidia/${CAMERA_NAME}.dtbo

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
	rsync -av ${KERNEL_OUT}/drivers/media/i2c/nv_imx708.ko ${JETSON_TARGET}:${WORKDIR}
	rsync -av remote_scripts/Makefile ${JETSON_TARGET}:${WORKDIR}
