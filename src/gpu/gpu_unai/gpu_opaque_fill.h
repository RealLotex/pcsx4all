/*
 * Shared opaque, non-blended GPU span fill.
 * No rendering shortcuts: mask checks and blending must use the normal path.
 * C++03-compatible so the RetroFW toolchain can compile this header.
 */
#ifndef PCSX4ALL_GPU_OPAQUE_FILL_H
#define PCSX4ALL_GPU_OPAQUE_FILL_H

#include <stdint.h>

static inline void gpuFillOpaquePixels(uint16_t *dst, uint32_t count,
                                       uint16_t color)
{
    if (count == 0)
        return;

    if (count < 8) {
        do {
            *dst++ = color;
        } while (--count);
        return;
    }

    // VRAM pixels are 16-bit aligned. Align paired 32-bit stores first.
    if ((uintptr_t)dst & 2u) {
        *dst++ = color;
        --count;
    }

    // may_alias permits the 32-bit write to alias the uint16_t VRAM pixels.
    typedef uint32_t __attribute__((__may_alias__)) gpu_pixel_pair_t;
    gpu_pixel_pair_t *pairs = (gpu_pixel_pair_t *)(void *)dst;
    const gpu_pixel_pair_t packed = (uint32_t)color |
                                    ((uint32_t)color << 16);
    uint32_t n = count >> 1;
    while (n--) {
        *pairs++ = packed;
    }
    if (count & 1u) {
        *(uint16_t *)(void *)pairs = color;
    }
}

#endif // PCSX4ALL_GPU_OPAQUE_FILL_H
