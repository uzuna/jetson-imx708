MAJOR:=36
MINOR:=2.0


# env
PWD:=$(shell pwd)

# L4T download definition
L4T_BASE_URL:=https://developer.download.nvidia.com/embedded/L4T/r${MAJOR}_Release_v${MINOR}
RELEASE_ADDR:=${L4T_BASE_URL}/release
SOURCE_ADDR:=${L4T_BASE_URL}/sources
TOOL_CHAIN_ADDR:=${L4T_BASE_URL}/toolchain
