include ../l4t.mk

# bsp definition
L4T_RELEASE_PACKAGE_NAME:=Jetson_Linux_R${MAJOR}.${MINOR}_aarch64.tbz2
L4T_RELEASE_PACKAGE:=${PWD}/${L4T_RELEASE_PACKAGE_NAME}
SAMPLE_FS_PACKAGE_NAME:=Tegra_Linux_Sample-Root-Filesystem_R${MAJOR}.${MINOR}_aarch64.tbz2
SAMPLE_FS_PACKAGE:=${PWD}/${SAMPLE_FS_PACKAGE_NAME}

# target device
STORAGE_DEVIVE:=nvme0n1p1  # NVMe SSD

BOARD:=jetson-orin-nano-devkit  # Orin Nano DevKit

# User settings
HOSTNAME?=orin-nano  # Need set your hostname
USERNAME:=jetson
PASSWORD:=jetson

.PHONY: pre-flash
pre-flash:
	make extract
	make add_user
	make apply_patch

# ターゲットへのflash
.PHONY: flash
flash:
	cd Linux_for_Tegra \
	&& sudo ./tools/kernel_flash/l4t_initrd_flash.sh --external-device ${STORAGE_DEVIVE} \
		-c tools/kernel_flash/flash_l4t_external.xml \
		-p "-c bootloader/generic/cfg/flash_t234_qspi.xml --no-systemimg" \
		--showlogs --network usb0 ${BOARD} internal

# L4Tにデフォルトユーザーを設定する
.PHONY: add_user
add_user:
	cd Linux_for_Tegra \
	&& sudo ./tools/l4t_create_default_user.sh -u ${USERNAME} -p ${PASSWORD} --autologin --accept-license --hostname ${HOSTNAME}

# BSPflashに関わるパッチを適用する
.PHONY: apply_patch
apply_patch:
	quilt push

.PHONY: extract	
extract: Linux_for_Tegra/rootfs

Linux_for_Tegra/tools: ${L4T_RELEASE_PACKAGE}
	tar xf ${L4T_RELEASE_PACKAGE}
	touch Linux_for_Tegra/tools

Linux_for_Tegra/rootfs: Linux_for_Tegra/tools ${SAMPLE_FS_PACKAGE}
	sudo tar xpf ${SAMPLE_FS_PACKAGE} -C Linux_for_Tegra/rootfs/
	cd Linux_for_Tegra/ \
		&& sudo ./apply_binaries.sh \
		&& sudo ./tools/l4t_flash_prerequisites.sh
	sudo touch Linux_for_Tegra/rootfs

.PHONY: download
download: ${L4T_RELEASE_PACKAGE} ${SAMPLE_FS_PACKAGE}

${L4T_RELEASE_PACKAGE}:
	wget ${RELEASE_ADDR}/${L4T_RELEASE_PACKAGE_NAME}

${SAMPLE_FS_PACKAGE}:
	wget ${RELEASE_ADDR}/${SAMPLE_FS_PACKAGE_NAME}

# need for flash
setup:
	sudo apt install -y sshpass abootimg nfs-kernel-server libxml2-utils quilt

clean:
	sudo rm -rf Linux_for_Tegra
	rm -rf .pc
