MAJOR:=35
MINOR:=3.1
L4T_RELEASE_PACKAGE:=Jetson_Linux_R${MAJOR}.${MINOR}_aarch64.tbz2
SAMPLE_FS_PACKAGE:=Tegra_Linux_Sample-Root-Filesystem_R${MAJOR}.${MINOR}_aarch64.tbz2
RELEASE_ADDR:=https://developer.download.nvidia.com/embedded/L4T/r${MAJOR}_Release_v${MINOR}/release

# STORAGE_DEVIVE:=sda1  # USB Memory or microSD
STORAGE_DEVIVE:=nvme0n1p1  # NVMe SSD

# BOARD:=p3737-0000+p3701-0000-maxn  # Jetson AGX Orin DK MAXN
# BOARD:=p3509-a02+p3767-0000  # Jetson OrinNX + XavierNX DK
BOARD:=jetson-orin-nano-devkit  # Jetson Orin Nano DK

HOSTNAME:=orin-nano  # Need set your hostname
USERNAME:=jetson
PASSWORD:=jetson

.PHONY: flush
flush:
	cd Linux_for_Tegra \
	&& sudo ./tools/kernel_flash/l4t_initrd_flash.sh --external-device ${STORAGE_DEVIVE} \
		-c tools/kernel_flash/flash_l4t_external.xml -p "-c bootloader/t186ref/cfg/flash_t234_qspi.xml" \
		--showlogs --network usb0 ${BOARD} internal

# L4Tにデフォルトユーザーを設定する
.PHONY: add_user
add_user:
	cd Linux_for_Tegra \
	&& sudo ./tools/l4t_create_default_user.sh -u ${USERNAME} -p ${PASSWORD} --autologin --accept-license --hostname ${HOSTNAME}

extract: Linux_for_Tegra/rootfs
	cd Linux_for_Tegra/ \
		&& sudo ./apply_binaries.sh \
		&& sudo ./tools/l4t_flash_prerequisites.sh

Linux_for_Tegra: download
	tar xf ${L4T_RELEASE_PACKAGE}

Linux_for_Tegra/rootfs: Linux_for_Tegra
	sudo tar xpf ${SAMPLE_FS_PACKAGE} -C Linux_for_Tegra/rootfs/

download: ${L4T_RELEASE_PACKAGE} ${SAMPLE_FS_PACKAGE}

${L4T_RELEASE_PACKAGE}:
	wget ${RELEASE_ADDR}/${L4T_RELEASE_PACKAGE}

${SAMPLE_FS_PACKAGE}:
	wget ${RELEASE_ADDR}/${SAMPLE_FS_PACKAGE}
