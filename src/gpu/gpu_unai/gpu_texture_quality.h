/* Optional coarse texture sampling for opaque 3D polygon spans.
 * Sprites, masked/blended/dithered polygons, and VRAM feedback use the
 * normal renderer. All polygon rows are drawn; this is not interlacing.
 */
#ifndef PCSX4ALL_GPU_TEXTURE_QUALITY_H
#define PCSX4ALL_GPU_TEXTURE_QUALITY_H

static bool gpuPotatoDisjoint(const gpu_unai_t &g, unsigned texmode)
{
    const u32 xmin=g.DrawingArea[0], ymin=g.DrawingArea[1];
    const u32 xmax=g.DrawingArea[2], ymax=g.DrawingArea[3];
    if (xmin>=xmax || ymin>=ymax || xmax>1024 || ymax>512)
        return false;
    const uintptr_t base=(uintptr_t)g.vram;
    const uintptr_t texture=(uintptr_t)g.TBA;
    if (texture<base || texture-base>=1024u*512u*2u || ((texture-base)&1u))
        return false;
    const u32 pos=(u32)((texture-base)>>1);
    const u32 tx=pos&1023u, ty=pos>>10;
    const u32 width=texmode==1 ? 64u : texmode==2 ? 128u : 256u;
    if (tx+width>1024u || ty+256u>512u)
        return false;
    if (xmin<tx+width && xmax>tx && ymin<ty+256u && ymax>ty)
        return false;
    if (texmode!=3) {
        const uintptr_t palette=(uintptr_t)g.CBA;
        if (palette<base || palette-base>=1024u*512u*2u || ((palette-base)&1u))
            return false;
        const u32 p=(u32)((palette-base)>>1);
        const u32 px=p&1023u, py=p>>10;
        const u32 entries=texmode==1 ? 16u : 256u;
        if (px+entries>1024u)
            return false;
        if (xmin<px+entries && xmax>px && ymin<=py && ymax>py)
            return false;
    }
    return true;
}

static inline void gpuWriteCoarseColor(u16 *dst, u32 n, u16 out)
{
    typedef u32 __attribute__((__may_alias__)) pixel_pair_t;
    if (n==4u) {
        const pixel_pair_t pair=(u32)out|((u32)out<<16);
        if ((uintptr_t)dst&2u) {
            dst[0]=out;
            *(pixel_pair_t *)(void *)(dst+1)=pair;
            dst[3]=out;
        } else {
            pixel_pair_t *p=(pixel_pair_t *)(void *)dst;
            p[0]=pair; p[1]=pair;
        }
    } else if (n==2u && !((uintptr_t)dst&2u)) {
        *(pixel_pair_t *)(void *)dst=(u32)out|((u32)out<<16);
    } else {
        for (u32 i=0;i<n;++i) dst[i]=out;
    }
}

template<int TM, bool LIGHT, bool GOURAUD, unsigned GROUP>
static void gpuPotatoSpan(const gpu_unai_t &g, u16 *dst, u32 count)
{
    const u8 *texture=(const u8 *)g.TBA;
    u32 u=g.u & g.u_msk, v=g.v & g.v_msk, color=g.gCol;
    const u32 ustep=(u32)g.u_inc, vstep=(u32)g.v_inc;
    while (count) {
        u16 source;
        const u32 tu=(u>>10)&~(GROUP-1u);
        const u32 tv=(v>>10)&~(GROUP-1u);
        if (TM==1) {
            const u8 packed=texture[(tv<<11)+(tu>>1)];
            source=g.CBA[(packed>>((tu&1u)<<2))&15u];
        } else if (TM==2) {
            source=g.CBA[texture[(tv<<11)+tu]];
        } else {
            source=g.TBA[(tv<<10)+tu];
        }
        const u32 n=count<GROUP ? count : GROUP;
        // The selector verified that every palette entry is opaque and has
        // a clear mask bit; palette writes cannot overlap this polygon.
        u16 out=source;
        if (LIGHT) {
            out=GOURAUD ? gpuLightingTXTGouraud(source,color)
                        : gpuLightingTXT(source,g.r5,g.g5,g.b5);
        }
        gpuWriteCoarseColor(dst,n,out);
        dst+=n; count-=n;
        u=(u+ustep*n)&g.u_msk;
        v=(v+vstep*n)&g.v_msk;
        if (LIGHT && GOURAUD) color+=g.gInc*n;
    }
}

static void gpuPlainSpan(const gpu_unai_t &g, u16 *dst, u32 count)
{
    (void)g;
    // GPU commands do not identify their originating model; use the same
    // neutral gray for each eligible polygon instead of guessing model IDs.
    gpuFillOpaquePixels(dst,count,0x4210u);
}

static bool gpuTexturePaletteIsOpaque(const gpu_unai_t &g, unsigned tm)
{
    // A missing/transparent texel must not turn into a solid block. Avoid
    // changing texture MSBs used by VRAM mask effects. Direct-color textures
    // cannot be proven opaque without reading their entire page.
    if (tm==3) return false;
    const unsigned entries=tm==1 ? 16u : 256u;
    if (!((uintptr_t)g.CBA&3u)) {
        typedef u32 __attribute__((__may_alias__)) palette_pair_t;
        const palette_pair_t *pairs=(const palette_pair_t *)(const void *)g.CBA;
        for (unsigned i=0;i<entries/2u;++i) {
            const u32 colors=pairs[i];
            if ((colors&0x80008000u) || !(colors&0xffffu) || !(colors>>16)) return false;
        }
        return true;
    }
    for (unsigned i=0;i<entries;++i)
        if (!g.CBA[i] || (g.CBA[i]&0x8000u)) return false;
    return true;
}

static bool gpuTextureQualityIsSafe(u32 index, unsigned level)
{
    // Preserve blending, masks, dithering and display pixel skipping.
    if (index & (0x02u|0x04u|0x18u|0x100u|0x200u|0x400u))
        return false;
    if ((index & 0x80u) && !(index & 1u))
        return false;
    // Advancing four coordinates at once is equivalent to four masked
    // increments only when each mask consists of contiguous low bits.
    if (level!=3 && ((gpu_unai.u_msk & (gpu_unai.u_msk+1u)) ||
        (gpu_unai.v_msk & (gpu_unai.v_msk+1u))))
        return false;
    const unsigned tm=(index>>5)&3u;
    if (!tm || tm==3 || !gpuPotatoDisjoint(gpu_unai,tm))
        return false;
    // Texture alpha is frequently used for text and other UI, including
    // transformed glyphs. Never quantize a palette with transparency or mask
    // bits; direct-color texture alpha cannot be proved from a palette.
    return gpuTexturePaletteIsOpaque(gpu_unai,tm);
}

static PP gpuTextureQualitySafeDriver(u32 index, unsigned level)
{
    const PP normal=gpuPolySpanDrivers[index];
    if (level==3)
        return gpuPlainSpan;
#define POTATO_SELECT(tm, light, gouraud) \
    (level==1 ? gpuPotatoSpan<tm,light,gouraud,2> : gpuPotatoSpan<tm,light,gouraud,4>)
    switch (index) {
    case 0x20: return POTATO_SELECT(1,false,false);
    case 0x21: return POTATO_SELECT(1,true,false);
    case 0xa1: return POTATO_SELECT(1,true,true);
    case 0x40: return POTATO_SELECT(2,false,false);
    case 0x41: return POTATO_SELECT(2,true,false);
    case 0xc1: return POTATO_SELECT(2,true,true);
    default: return normal;
    }
#undef POTATO_SELECT
}

static PP gpuChoosePotatoDriver(u32 index)
{
    const unsigned level=gpu_unai_config_ext.texture_size;
    if (!level || !gpuTextureQualityIsSafe(index,level))
        return gpuPolySpanDrivers[index];
    return gpuTextureQualitySafeDriver(index,level);
}

// Text and HUD glyphs are commonly emitted as axis-aligned textured quads,
// or as the two axis-aligned triangles of such a quad. Keep them pixel-exact.
// Other games can draw text with arbitrary polygons, so this is deliberately
// a conservative heuristic rather than a claim of universal identification.
static bool gpuTextureLooksLike2D(PtrUnion packet, bool quad, bool gouraud)
{
    const unsigned n=quad ? 4u : 3u, stride=gouraud ? 3u : 2u;
    s32 x[4], y[4];
    s32 xmin=2048, xmax=-2048, ymin=2048, ymax=-2048;
    for (unsigned i=0;i<n;++i) {
        const u32 coord=packet.U4[1+i*stride];
        const u32 xx=coord&2047u, yy=(coord>>16)&2047u;
        x[i]=(s32)xx-((xx&1024u)?2048:0);
        y[i]=(s32)yy-((yy&1024u)?2048:0);
        if (x[i]<xmin) xmin=x[i];
        if (x[i]>xmax) xmax=x[i];
        if (y[i]<ymin) ymin=y[i];
        if (y[i]>ymax) ymax=y[i];
    }
    if (xmin==xmax || ymin==ymax) return false;
    unsigned corners=0;
    for (unsigned i=0;i<n;++i) {
        if ((x[i]!=xmin && x[i]!=xmax) || (y[i]!=ymin && y[i]!=ymax)) return false;
        const unsigned corner=(x[i]==xmax ? 1u : 0u)|(y[i]==ymax ? 2u : 0u);
        if (corners & (1u<<corner)) return false;
        corners|=1u<<corner;
    }
    return true;
}

static PP gpuChooseTexturePolygonDriver(u32 index, PtrUnion packet, bool quad, bool gouraud, bool *protected_polygon_out=0)
{
    const bool needs_classification=gpu_unai_config_ext.texture_size ||
        gpu_unai_config_ext.vertex_grid || gpu_unai_config_ext.small_polygons;
    bool protect=needs_classification && gpuTextureLooksLike2D(packet,quad,gouraud);
    if (needs_classification && !protect) {
        const unsigned level=gpu_unai_config_ext.texture_size;
        protect=!gpuTextureQualityIsSafe(index,level ? level : 3u);
    }
    if (protected_polygon_out) *protected_polygon_out=protect;
    if (!gpu_unai_config_ext.texture_size || protect)
        return gpuPolySpanDrivers[index];
    return gpuTextureQualitySafeDriver(index,gpu_unai_config_ext.texture_size);
}
#endif
