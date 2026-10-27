/*
 * Safely read a PS1 VRAM display scanline whose linear address wraps at 1 MiB.
 * The blitters require contiguous pixels; most scanlines can use VRAM directly.
 * C++03-compatible, standalone host regression-tested helper.
 */
#ifndef PCSX4ALL_GPU_VRAM_SCANLINE_H
#define PCSX4ALL_GPU_VRAM_SCANLINE_H

#include <stdint.h>
#include <string.h>

enum { GPU_VRAM_WORDS = 1024 * 512, GPU_SCANLINE_SCRATCH_WORDS = 1024 };

// Preconditions: offset_words < GPU_VRAM_WORDS, width is 256..640,
// scratch is at least GPU_SCANLINE_SCRATCH_WORDS words and word-aligned.
// 24-bit pixels use 3 bytes per pixel; odd bytes round up to a word.
static inline const uint16_t *gpuVramScanline(const uint16_t *vram,
                                               uint32_t offset_words,
                                               unsigned int width,
                                               bool rgb24,
                                               uint16_t *scratch)
{
    const uint32_t words = rgb24 ? (width * 3u + 1u) / 2u : width;
    const uint32_t until_end = GPU_VRAM_WORDS - offset_words;
    if (words <= until_end)
        return vram + offset_words;

    // Only the uncommon end-of-VRAM case copies data. Never read past VRAM.
    memcpy(scratch, vram + offset_words, until_end * sizeof(uint16_t));
    memcpy(scratch + until_end, vram,
           (words - until_end) * sizeof(uint16_t));
    return scratch;
}

#endif // PCSX4ALL_GPU_VRAM_SCANLINE_H
