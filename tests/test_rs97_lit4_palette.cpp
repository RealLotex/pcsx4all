/* Differential tests of the experimental 4bpp flat-light span driver.
 * Includes transparent and black-lit texels, mask bit, texture feedback,
 * palette changes, clipping guards and a full VRAM comparison. */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define USE_GPULIB
#define GPU_INLINE static inline __attribute__((always_inline))
#include <gpu_unai.h>
#include <gpu_inner.h>
#include <gpu_lit4_palette.h>

enum { WORDS = 1024 * 512, GUARD = 1024 };
static u16 baseline[WORDS + GUARD], candidate[WORDS + GUARD];
static u32 rng = 0x124f926bu;
static u32 random32() { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng; }

static void setup(u16 *vram, u32 texture, u32 clut, u32 u, u32 v,
                  u32 um, u32 vm, s32 du, s32 dv, u8 r, u8 g, u8 b)
{
    gpu_unai.vram = vram; gpu_unai.TBA = vram + texture;
    gpu_unai.CBA = vram + clut;
    gpu_unai.u = u; gpu_unai.v = v; gpu_unai.u_msk = um; gpu_unai.v_msk = vm;
    gpu_unai.u_inc = du; gpu_unai.v_inc = dv;
    gpu_unai.r5 = r; gpu_unai.g5 = g; gpu_unai.b5 = b;
}
static double now()
{
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec + t.tv_nsec / 1000000000.0;
}
static void bench()
{
    const unsigned polygons = 80000, spans = 14, width = 9;
    double times[2];
    for (unsigned mode = 0; mode < 2; ++mode) {
        setup(candidate, 128 * 1024, 480 * 1024, 0x17700, 0x25b00,
              0x3ffff, 0x3ffff, 777, 411, 11, 19, 24);
        candidate[480 * 1024] = 0;
        void (*volatile draw)(const gpu_unai_t &, u16 *, u32) =
            mode ? gpuLit4Span : gpuPolySpanDrivers[0x21];
        const double start = now();
        for (unsigned p = 0; p < polygons; ++p) {
            if (mode) gpuLit4Prepare(gpu_unai);
            for (unsigned s = 0; s < spans; ++s) {
                gpu_unai.u = (p * 73u + s * 903u) & 0x3ffffu;
                gpu_unai.v = (p * 17u + s * 381u) & 0x3ffffu;
                draw(gpu_unai, candidate + (s * 1024), width);
            }
        }
        times[mode] = now() - start;
        printf("BENCH %s %.6f s checksum %u\n",
               mode ? "cached" : "original", times[mode], candidate[0]);
    }
    printf("BENCH original/cached %.3f\n", times[0] / times[1]);
}
int main(int argc, char **argv)
{
    SetupLightLUT();
    SetupDitheringConstants();
    for (unsigned i = 0; i < WORDS + GUARD; ++i)
        baseline[i] = candidate[i] = (u16)random32();
    if (argc > 1 && !strcmp(argv[1], "--bench")) { bench(); return 0; }

    gpu_unai.vram = baseline;
    gpu_unai.DrawingArea[0] = 0; gpu_unai.DrawingArea[1] = 0;
    gpu_unai.DrawingArea[2] = 320; gpu_unai.DrawingArea[3] = 240;
    gpu_unai.CBA = baseline + 480 * 1024;
    if (!gpuLit4PaletteDisjoint(gpu_unai)) { puts("FAIL disjoint"); return 1; }
    gpu_unai.CBA = baseline + 200 * 1024 + 310;
    if (gpuLit4PaletteDisjoint(gpu_unai)) { puts("FAIL CLUT overlap"); return 1; }
    gpu_unai.CBA = baseline + 200 * 1024 - 8;
    if (gpuLit4PaletteDisjoint(gpu_unai)) { puts("FAIL CLUT boundary"); return 1; }
    gpu_unai.DrawingArea[3] = 513;
    if (gpuLit4PaletteDisjoint(gpu_unai)) { puts("FAIL invalid area"); return 1; }

    for (unsigned trial = 0; trial < 6000; ++trial) {
        const u32 clut = 480 * 1024;
        const u32 texture = (trial & 1) ? 128 * 1024 : 0;
        const u32 dst = (random32() % 200) * 1024 + (random32() % 400);
        const u32 count = 1 + random32() % 256;
        for (unsigned i = 0; i < 16; ++i) {
            u16 x = (u16)random32();
            if ((i + trial) % 7 == 0) x = 0;
            if ((i + trial) % 11 == 0) x = 0x8000;
            baseline[clut+i] = candidate[clut+i] = x;
        }
        const u32 um = ((random32() & 255u) << 10) | 1023u;
        const u32 vm = ((random32() & 255u) << 10) | 1023u;
        const u32 u = random32(), v = random32();
        const s32 du = (s32)(random32() % 65537u) - 32768;
        const s32 dv = (s32)(random32() % 65537u) - 32768;
        const u8 r = random32() & 31u, g = random32() & 31u, b = random32() & 31u;
        setup(baseline, texture, clut, u, v, um, vm, du, dv, r, g, b);
        gpuPolySpanDrivers[0x21](gpu_unai, baseline + dst, count);
        setup(candidate, texture, clut, u, v, um, vm, du, dv, r, g, b);
        gpuLit4Prepare(gpu_unai);
        gpuLit4Span(gpu_unai, candidate + dst, count);
        if (memcmp(baseline, candidate, sizeof baseline)) {
            printf("FAIL VRAM mismatch trial=%u\n", trial); return 1;
        }
    }
    puts("PASS 6000 flat-lit 4bpp spans and palette-overlap guards");
    return 0;
}
