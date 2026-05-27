#!/usr/bin/env bash
set -e

build() {
  make build
}

clean() {
  make clean
}

run() {
  build
  qemu-system-x86_64 \
    -drive file=boot.bin,format=raw \
    -debugcon stdio \
    "$@"
}

gdb_connect() {
  local port="${1:-1234}"
  build
  gdb -ex "target remote :$port" -ex "symbol-file boot.elf" -ex "set architecture i386:x86-64"
}

install() {
  local img="$1"

  if [ -z "$img" ]; then
    echo "Usage: $0 install <disk-image>"
    exit 1
  fi

  build

  local size
  size=$(stat -c '%s' boot.bin)

  # boot.bin 最多允许占用 MBR + 后续 2047 个扇区。
  # 传统MBR分区常从1MiB开始，也就是LBA 2048；LBA 1~2047这段MBR gap通常可供bootloader存放后续阶段。
  # 这里覆盖 LBA 0 ~ LBA 2047，不能超过第一个分区起始位置。
  local max_size=$((512 * 2048))

  if [ "$size" -gt "$max_size" ]; then
    echo "Error: boot.bin too large: $size bytes, max is $max_size bytes"
    exit 1
  fi

  # 1. 只写 MBR 前 446 字节，保留目标镜像里的分区表
  dd if=boot.bin of="$img" bs=1 count=446 conv=notrunc

  # 2. 从 boot.bin 的第 512 字节开始，写入目标镜像 LBA 1 开始的位置
  #    最多写 2047 个扇区
  if [ "$size" -gt 512 ]; then
    dd if=boot.bin of="$img" bs=512 skip=1 seek=1 count=2047 conv=notrunc
  fi
}

case "${1:-build}" in
build)
  build
  ;;
clean)
  clean
  ;;
run)
  shift
  run "$@"
  ;;
gdb)
  gdb_connect "$2"
  ;;
install)
  install "$2"
  ;;
rebuild)
  make rebuild
  ;;
*)
  echo "Usage: $0 [build|clean|run [qemu-args...]|gdb [port]|install <disk-image>|rebuild]"
  exit 1
  ;;
esac
