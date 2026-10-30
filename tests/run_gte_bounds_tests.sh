#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
flags=(-std=c++03 -O2 -ffunction-sections -fdata-sections -Isrc -Isrc/port/sdl -Isrc/plugin_lib -Isrc/gpu/gpu_unai
       -DINLINE="static inline" -DGCW_ZERO -DGPU_UNAI -DUSE_GPULIB)
tmpdir=$(mktemp -d)
trap 'rm -rf -- "$tmpdir"' EXIT
for mode in baseline fast; do
    define=()
    if [ "$mode" = fast ]; then define=(-DGTE_SIGNED32_BOUNDS_EXPERIMENT); fi
    g++ "${flags[@]}" "${define[@]}" -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_gte_signed32_bounds.cpp -Wl,--gc-sections -o "$tmpdir/bounds-san"
    UBSAN_OPTIONS=halt_on_error=1 "$tmpdir/bounds-san"
done
g++ "${flags[@]}" tests/test_gte_bounds_state.cpp tests/gte_bounds_variant.cpp src/gte.cpp -Wl,--gc-sections -o "$tmpdir/gte-state-host"
"$tmpdir/gte-state-host"
if [ -n "${CXX_MIPS:-}" ]; then
    sysroot=$("${CC_MIPS:-${CXX_MIPS%g++}gcc}" --print-sysroot)
    for mode in baseline fast; do
        define=()
        if [ "$mode" = fast ]; then define=(-DGTE_SIGNED32_BOUNDS_EXPERIMENT); fi
        "$CXX_MIPS" "${flags[@]}" "${define[@]}" -mips32 -mno-abicalls -fno-PIC tests/test_gte_signed32_bounds.cpp -Wl,--gc-sections -o "$tmpdir/bounds-mips"
        qemu-mipsel -L "$sysroot" "$tmpdir/bounds-mips"
    done
    "$CXX_MIPS" "${flags[@]}" -mips32 -mno-abicalls -fno-PIC tests/test_gte_bounds_state.cpp tests/gte_bounds_variant.cpp src/gte.cpp -Wl,--gc-sections -o "$tmpdir/gte-state-mips"
    qemu-mipsel -L "$sysroot" "$tmpdir/gte-state-mips"
fi
