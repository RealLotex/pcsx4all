/* Experimental flat-lit 4bpp palette transform, scoped to one polygon.
 * The caller must rule out writes to the source palette for the entire
 * polygon. Texture reads themselves remain in their original order. */
#ifndef PCSX4ALL_GPU_LIT4_PALETTE_H
#define PCSX4ALL_GPU_LIT4_PALETTE_H

static u32 gpu_lit4_colors[16];

static void gpuLit4Prepare(const gpu_unai_t &g)
{
    for (unsigned i = 0; i != 16; ++i) {
        const u16 source = g.CBA[i];
        gpu_lit4_colors[i] = source
            ? (0x10000u | (source & 0x8000u) |
               (g.LightLUT[((source & 0x7c00u) >> 5) | g.b5] << 10) |
               (g.LightLUT[ (source & 0x03e0u)       | g.g5] << 5) |
                g.LightLUT[((source & 0x001fu) << 5) | g.r5])
            : 0;
    }
}

static void gpuLit4Span(const gpu_unai_t &g, u16 *dst, u32 count)
{
    u32 u = g.u & g.u_msk, v = g.v & g.v_msk;
    const u8 *texture = (const u8 *)g.TBA;
    do {
        const u32 tu = u >> 10;
        const u8 packed = texture[((v << 1) & (0xffu << 11)) + (tu >> 1)];
        const u32 color = gpu_lit4_colors[(packed >> ((tu & 1u) << 2)) & 15u];
        if (color) *dst = (u16)color;
        ++dst;
        u = (u + g.u_inc) & g.u_msk;
        v = (v + g.v_inc) & g.v_msk;
    } while (--count);
}

/* Guard the entire clipping area, rather than a vertex bounding box: fixed
 * point rounding and clipping must never permit CLUT feedback to be cached. */
static bool gpuLit4PaletteDisjoint(const gpu_unai_t &g)
{
    const u32 xmin = g.DrawingArea[0], ymin = g.DrawingArea[1];
    const u32 xmax = g.DrawingArea[2], ymax = g.DrawingArea[3];
    if (xmin >= xmax || ymin >= ymax || xmax > 1024 || ymax > 512)
        return false;
    const uintptr_t first = (uintptr_t)g.vram + 2u * (ymin * 1024u + xmin);
    const uintptr_t last = (uintptr_t)g.vram + 2u * ((ymax - 1u) * 1024u + xmax);
    const uintptr_t palette = (uintptr_t)g.CBA;
    return palette >= last || (palette < first && first - palette >= 32u);
}
#endif
