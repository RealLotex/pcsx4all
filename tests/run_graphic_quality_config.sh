#!/bin/bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cxx=${CXX_MIPS:-/opt/retrofw-sdk/bin/mipsel-linux-g++}
gcc=${CC_MIPS:-/opt/retrofw-sdk/bin/mipsel-linux-gcc}
sysroot=$($gcc --print-sysroot)
tmpdir=$(mktemp -d)
trap 'rm -rf -- "$tmpdir"' EXIT
flags=(-std=c++03 -O2 -mips32 -mno-abicalls -fno-PIC -ffunction-sections -fdata-sections
       -I"$root/src" -I"$root/src/port/sdl" -I"$root/src/plugin_lib"
       -I"$root/src/gpu/gpu_unai" -I"$sysroot/usr/include/SDL"
       -DGCW_ZERO -DPSXREC -DGPU_UNAI -DUSE_GPULIB -DINLINE="static inline")
"$cxx" "${flags[@]}" -Dmain=pcsx4all_unused_main -c "$root/src/port/sdl/port.cpp" -o "$tmpdir/port.o"
"$cxx" "${flags[@]}" "$root/tests/test_graphic_quality_config.cpp" "$tmpdir/port.o" -Wl,--gc-sections -o "$tmpdir/test-config"
cd "$tmpdir"
qemu-mipsel -L "$sysroot" ./test-config
