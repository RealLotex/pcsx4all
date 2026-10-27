/* Compare the actual production fast path with the original GPU Unai loop.
 * This is not firmware/OPK or MIPS hardware qualification. */
#include "../src/gpu/gpu_unai/gpu_sprite_indexed_fast.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint32_t seed = 0x71c76a2eu;
static uint32_t random32()
{
    seed ^= seed << 13;
    seed ^= seed >> 17;
    seed ^= seed << 5;
    return seed;
}

static void sprite4bppReference(uint16_t *dst, unsigned count,
                                const uint8_t *texture, uint32_t u0,
                                uint32_t mask, const uint16_t *clut)
{
    for (unsigned i = 0; i < count; ++i, ++u0) {
        const uint8_t byte = texture[(u0 & mask) >> 1];
        const uint16_t color = clut[(byte >> ((u0 & 1u) << 2)) & 0xfu];
        if (color)
            dst[i] = color;
    }
}

static int test_random_spans()
{
    uint8_t texture[2048];
    uint16_t clut[16];
    uint16_t reference[280] __attribute__((aligned(4)));
    uint16_t actual[280] __attribute__((aligned(4)));
    const uint32_t masks[] = { 0, 1, 2, 3, 7, 15, 31, 63, 127, 255 };
    unsigned fast_count = 0, fallback_count = 0;

    for (unsigned trial = 0; trial < 20000; ++trial) {
        for (unsigned k = 0; k < 2048; ++k)
            texture[k] = (uint8_t)random32();
        for (unsigned k = 0; k < 16; ++k)
            clut[k] = ((random32() & 3) == 0) ? 0 : (uint16_t)random32();
        for (unsigned k = 0; k < 280; ++k)
            reference[k] = actual[k] = (uint16_t)random32();

        const unsigned count = random32() % 256;
        const unsigned offset = random32() & 1;
        const uint32_t u0 = random32() & 511;
        const uint32_t mask = masks[random32() % 10];

        sprite4bppReference(reference + offset, count, texture, u0, mask, clut);
        if (gpuTryFastSprite4bppSpan(actual + offset, count,
                                    texture, u0, mask, clut)) {
            ++fast_count;
        } else {
            ++fallback_count;
            sprite4bppReference(actual + offset, count, texture, u0, mask, clut);
        }
        if (memcmp(reference, actual, sizeof(actual))) {
            printf("FAIL 4bpp iteration=%u length=%u offset=%u u0=%u mask=%u\n",
                   trial, count, offset, u0, mask);
            return 1;
        }
    }
    if (fast_count == 0 || fallback_count == 0) {
        puts("FAIL no fast/fallback path coverage");
        return 1;
    }
    printf("PASS 20000 randomized 4bpp sprites (%u fast / %u fallback)\n",
           fast_count, fallback_count);
    return 0;
}

static int test_alias_guards()
{
    uint16_t storage[2048] __attribute__((aligned(4)));
    uint8_t texture[2048] __attribute__((aligned(4)));
    for (unsigned i = 0; i < 2048; ++i) {
        storage[i] = (uint16_t)random32();
        texture[i] = (uint8_t)random32();
    }
    // Destination overlaps source texture (render-to-texture effects).
    if (gpuTryFastSprite4bppSpan(storage + 10, 128,
                                 (const uint8_t *)storage, 0, 255,
                                 storage + 1500)) {
        puts("FAIL texture overlap guard");
        return 1;
    }
    // Destination overlaps palette memory.
    if (gpuTryFastSprite4bppSpan(storage + 1500, 128,
                                 texture, 0, 255, storage + 1502)) {
        puts("FAIL CLUT overlap guard");
        return 1;
    }
    puts("PASS texture/CLUT overlap guards");
    return 0;
}

int main()
{
    return test_random_spans() || test_alias_guards();
}
