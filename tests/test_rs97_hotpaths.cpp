// Test the ACTUAL production helpers included by GPU Unai/GPULib.
// Builds with C++03, as used by the RetroFW source.
#include "../src/gpu/gpu_unai/gpu_opaque_fill.h"
#include "../src/gpu/gpulib/vram_scanline.h"

#include <stdio.h>
#include <string.h>
#include <vector>

static uint32_t random_state = 0x79cab319u;
static uint32_t next_random()
{
    random_state ^= random_state << 13;
    random_state ^= random_state >> 17;
    random_state ^= random_state << 5;
    return random_state;
}

static int test_fills()
{
    uint16_t reference[530];
    uint16_t actual[530];

    for (unsigned int count = 0; count <= 512; ++count) {
        for (unsigned int off = 0; off != 2; ++off) {
            for (unsigned int trial = 0; trial != 12; ++trial) {
                const uint16_t color = (uint16_t)next_random();
                for (unsigned int k = 0; k < 530; ++k) {
                    reference[k] = actual[k] = (uint16_t)next_random();
                }

                // Test in-buffer offsets (0 and 2 bytes from 32-bit alignment).
                for (unsigned int k = 0; k < count; ++k)
                    reference[off + k] = color;
                gpuFillOpaquePixels(actual + off, count, color);
                if (memcmp(actual, reference, sizeof(actual)) != 0) {
                    printf("FAIL opaque fill: length=%u offset=%u trial=%u\n",
                           count, off, trial);
                    return 1;
                }
            }
        }
    }
    puts("PASS: production opaque span helper, lengths 0..512, both alignments");
    return 0;
}

static int check_scanline(const uint16_t *vram, unsigned width,
                          bool rgb24, uint32_t offset)
{
    // A stack buffer with a tail canary to detect overwritten scratch.
    uint16_t scratch[GPU_SCANLINE_SCRATCH_WORDS + 8]
        __attribute__((aligned(4)));
    for (unsigned k = 0; k < sizeof(scratch) / sizeof(scratch[0]); ++k)
        scratch[k] = 0xA55Au;
    const uint32_t words = rgb24 ? (width * 3u + 1u) / 2u : width;
    const uint32_t bytes = rgb24 ? width * 3u : width * 2u;
    const uint16_t *result =
        gpuVramScanline(vram, offset, width, rgb24, scratch);

    const bool crossing = words > GPU_VRAM_WORDS - offset;
    if (crossing ? (result != scratch) : (result != vram + offset)) {
        printf("FAIL scanline selection width=%u rgb24=%d offset=%u\n",
               width, (int)rgb24, offset);
        return 1;
    }

    for (uint32_t i = 0; i < words; ++i) {
        if (result[i] != vram[(offset + i) % GPU_VRAM_WORDS]) {
            printf("FAIL scanline pixels width=%u rgb24=%d offset=%u word=%u\n",
                   width, (int)rgb24, offset, i);
            return 1;
        }
    }

    // For 24-bit output also verify the complete byte stream, not just words.
    for (uint32_t i = 0; i < bytes; ++i) {
        const uint8_t *want = (const uint8_t *)vram;
        const uint8_t *got = (const uint8_t *)result;
        if (got[i] != want[(offset * 2u + i) % (GPU_VRAM_WORDS * 2u)]) {
            printf("FAIL 24-bit stream width=%u offset=%u byte=%u\n",
                   width, offset, i);
            return 1;
        }
    }

    const uint16_t *canary = scratch + GPU_SCANLINE_SCRATCH_WORDS;
    for (int j = 0; j < 8; ++j) {
        if (canary[j] != 0xA55Au) {
            puts("FAIL scratch buffer overflow");
            return 1;
        }
    }
    return 0;
}

static int test_scanlines()
{
    std::vector<uint16_t> vram(GPU_VRAM_WORDS);
    for (unsigned int i = 0; i < GPU_VRAM_WORDS; ++i)
        vram[i] = (uint16_t)next_random();

    const unsigned widths[] = { 256, 320, 368, 384, 512, 640 };
    for (unsigned int w = 0; w < sizeof(widths)/sizeof(widths[0]); ++w) {
        for (int rgb = 0; rgb != 2; ++rgb) {
            const unsigned width = widths[w];
            const uint32_t span = rgb ? (width * 3u + 1u) / 2u : width;
            const uint32_t cases[] = {
                0u, 1u, 13u, 1024u, GPU_VRAM_WORDS - span - 1,
                GPU_VRAM_WORDS - span, GPU_VRAM_WORDS - span + 1,
                GPU_VRAM_WORDS - 2, GPU_VRAM_WORDS - 1
            };
            for (unsigned k = 0; k < sizeof(cases)/sizeof(cases[0]); ++k)
                if (check_scanline(&vram[0], width, rgb != 0, cases[k]))
                    return 1;

            for (int trial = 0; trial < 300; ++trial) {
                // Exercise uncommon wrap crossings as well as normal rows.
                uint32_t offset = trial & 1
                    ? next_random() % GPU_VRAM_WORDS
                    : GPU_VRAM_WORDS - 1 - (next_random() % span);
                if (check_scanline(&vram[0], width, rgb != 0, offset))
                    return 1;
            }
        }
    }
    puts("PASS: production scanline helper, RGB15/RGB24, all six widths");
    puts("PASS: end-of-VRAM wrapping, byte stream and scratch canaries");
    return 0;
}

int main()
{
    if (test_fills())
        return 1;
    if (test_scanlines())
        return 1;
    return 0;
}
