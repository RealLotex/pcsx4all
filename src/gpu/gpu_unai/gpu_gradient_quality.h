/* Optional lower-resolution Gouraud gradients on opaque 3D polygons. */
#ifndef PCSX4ALL_GPU_GRADIENT_QUALITY_H
#define PCSX4ALL_GPU_GRADIENT_QUALITY_H

template<unsigned GROUP>
static void gpuCoarseGradientSpan(const gpu_unai_t &g, u16 *dst, u32 count)
{
    u32 color=g.gCol;
    while (count) {
        const u32 n=count<GROUP ? count : GROUP;
        gpuWriteCoarseColor(dst,n,gpuLightingRGB(color));
        color+=g.gInc*n;
        dst+=n; count-=n;
    }
}

static void gpuFlatGradientSpan(const gpu_unai_t &g, u16 *dst, u32 count)
{
    gpuFillOpaquePixels(dst,count,(u16)(g.r5 | (g.g5<<5) | (g.b5<<10)));
}

static PP gpuChooseGradientPolygonDriver(u32 index, PtrUnion packet, bool quad, bool *protected_polygon_out=0)
{
    const PP normal=gpuPolySpanDrivers[index];
    const unsigned level=gpu_unai_config_ext.gradient_quality;
    const bool protected_polygon=(level || gpu_unai_config_ext.small_polygons) &&
        (index!=0x81u || gpuTextureLooksLike2D(packet,quad,false));
    if (protected_polygon_out) *protected_polygon_out=protected_polygon;
    if (!level || index!=0x81u || protected_polygon)
        return normal;
    if (level==1) return gpuCoarseGradientSpan<2>;
    if (level==2) return gpuCoarseGradientSpan<4>;
    return gpuFlatGradientSpan;
}
#endif
