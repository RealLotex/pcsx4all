/*
 * Optional paired stores for opaque, unlit GPU Unai 4bpp CLUT sprites.
 * C++03 compatible. Conservative overlap guards: PS1 texture, palette
 * and render destination may all reside in the same VRAM.
 */
#ifndef PCSX4ALL_GPU_SPRITE_INDEXED_FAST_H
#define PCSX4ALL_GPU_SPRITE_INDEXED_FAST_H

#include <stdint.h>

static inline bool gpuSpriteMemoryRangesOverlap(uintptr_t a, uint32_t a_size,
                                                uintptr_t b, uint32_t b_size)
{
    // Unsigned address subtraction avoids end-pointer arithmetic overflow.
    return (a >= b) ? (a - b < b_size) : (b - a < a_size);
}

static inline bool gpuTryFastSprite4bppSpan(uint16_t *dst, uint32_t count,
                                           const uint8_t *texture,
                                           uint32_t u0, uint32_t u0_mask,
                                           const uint16_t *clut)
{
    if (count < 8 || ((uintptr_t)dst & 3u))
        return false;
    if (u0 & 1u)
        return false;

    // Self-texturing or rendering into CLUT depends on pixel write order.
    // Leave those operations to the exact original scalar implementation.
    const uintptr_t dst_ptr = (uintptr_t)dst;
    const uint32_t dst_bytes = count * 2u;
    if (gpuSpriteMemoryRangesOverlap(dst_ptr, dst_bytes,
                                     (uintptr_t)texture, 2048u) ||
        gpuSpriteMemoryRangesOverlap(dst_ptr, dst_bytes,
                                     (uintptr_t)clut, 32u))
        return false;

    typedef uint32_t __attribute__((__may_alias__)) packed_pixels_t;
    packed_pixels_t *dst32 = (packed_pixels_t *)(void *)dst;
    const uint32_t pairs = count >> 1;
    for (uint32_t i = 0; i < pairs; ++i, u0 += 2) {
        const uint8_t nibble_pair = texture[(u0 & u0_mask) >> 1];
        const uint16_t p0 = clut[nibble_pair & 0x0fu];
        const uint16_t p1 = clut[nibble_pair >> 4];
        if (p0 && p1) {
            dst32[i] = (uint32_t)p0 | ((uint32_t)p1 << 16);
        } else {
            uint16_t *pair = dst + i * 2u;
            if (p0) pair[0] = p0;
            if (p1) pair[1] = p1;
        }
    }
    if (count & 1u) {
        const uint8_t index = texture[(u0 & u0_mask) >> 1];
        const uint16_t last = clut[(index >> ((u0 & 1u) * 4u)) & 0x0fu];
        if (last)
            dst[count - 1] = last;
    }
    return true;
}

#endif
