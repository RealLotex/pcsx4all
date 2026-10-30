/* Experimental lower-resolution polygon rasterization. The native VRAM layout
 * and transfer paths stay intact; each coarse raster sample expands to a
 * native pixel block with destination-dependent effects applied per pixel. */
#ifndef PCSX4ALL_GPU_LOWRES_H
#define PCSX4ALL_GPU_LOWRES_H

struct gpu_lowres_state_t {
    unsigned shift, flags;
    u16 clip[4];
    u8 ilace;
#ifdef GPU_LOWRES_TEST_COUNTERS
    uint64_t samples, writes;
#endif
};
static gpu_lowres_state_t gpu_lowres;

static u16 gpuLowResBlend(u16 source,u16 dest,unsigned mode)
{
    switch(mode) {
    case 0: return gpuBlending<0,false>(source,dest);
    case 1: return gpuBlending<1,false>(source,dest);
    case 2: return gpuBlending<2,false>(source,dest);
    default: return gpuBlending<3,false>(source,dest);
    }
}
static u32 gpuLowResBlend24(u32 source,u16 dest,unsigned mode)
{
    switch(mode) {
    case 0: return gpuBlending24<0>(source,dest);
    case 1: return gpuBlending24<1>(source,dest);
    case 2: return gpuBlending24<2>(source,dest);
    default: return gpuBlending24<3>(source,dest);
    }
}

static void gpuLowResSpan(const gpu_unai_t &g,u16 *coarse,u32 count)
{
    const unsigned cf=gpu_lowres.flags, tm=(cf>>5)&3u;
    const bool light=(cf&1u)!=0, blend=(cf&2u)!=0, mask=(cf&4u)!=0;
    const bool gouraud=(cf&128u)!=0;
    const bool hq=(cf&512u) && ((tm && light) || (!tm && gouraud));
    const bool simple=!(cf&(2u|4u|512u|1024u));
    const unsigned scale=1u<<gpu_lowres.shift;
    const uintptr_t offset=((uintptr_t)coarse-(uintptr_t)g.vram)/2u;
    const unsigned y=(unsigned)(offset>>10)*scale;
    unsigned x=(unsigned)(offset&1023u)*scale;
    u32 u=g.u&g.u_msk,v=g.v&g.v_msk,gcol=g.gCol;
    const u8 *texture=(const u8 *)g.TBA;
    while (count--) {
#ifdef GPU_LOWRES_TEST_COUNTERS
        ++gpu_lowres.samples;
#endif
        u16 raw=g.PixelData;
        if (tm==1) {
            const unsigned tu=u>>10;
            const u8 packed=texture[((v<<1)&(255u<<11))+(tu>>1)];
            raw=g.CBA[(packed>>((tu&1u)*4))&15u];
        } else if (tm==2) {
            raw=g.CBA[texture[((v<<1)&(255u<<11))+(u>>10)]];
        } else if (tm==3) {
            raw=g.TBA[(v&(255u<<10))+(u>>10)];
        }
        if (!tm || raw) {
            const u16 msb=tm ? (raw&0x8000u) : 0;
            u16 shaded=raw;
            u32 shaded24=0;
            if (hq) {
                if (!tm) shaded24=gpuLightingRGB24(gcol);
                else shaded24=gouraud ? gpuLightingTXT24Gouraud(raw,gcol)
                                     : gpuLightingTXT24(raw,g.r8,g.g8,g.b8);
            } else if (tm && light) {
                shaded=gouraud ? gpuLightingTXTGouraud(raw,gcol)
                               : gpuLightingTXT(raw,g.r5,g.g5,g.b5);
            } else if (!tm && gouraud) shaded=gpuLightingRGB(gcol);
            if (simple) {
                u16 out=shaded;
                if (cf&256u) out|=0x8000u;
                else if (tm && light) out|=msb;
                const unsigned left=x<gpu_lowres.clip[0] ? gpu_lowres.clip[0] : x;
                const unsigned right=x+scale>gpu_lowres.clip[2] ? gpu_lowres.clip[2] : x+scale;
                if (right>left) for (unsigned dy=0;dy<scale;++dy) {
                    const unsigned yy=y+dy;
                    if (yy<gpu_lowres.clip[1] || yy>=gpu_lowres.clip[3] || (yy&gpu_lowres.ilace)) continue;
                    gpuWriteCoarseColor(g.vram+yy*1024u+left,right-left,out);
#ifdef GPU_LOWRES_TEST_COUNTERS
                    gpu_lowres.writes+=right-left;
#endif
                }
            } else {
            for (unsigned dy=0;dy<scale;++dy) {
                const unsigned yy=y+dy;
                if (yy<gpu_lowres.clip[1] || yy>=gpu_lowres.clip[3] || (yy&gpu_lowres.ilace)) continue;
                for (unsigned dx=0;dx<scale;++dx) {
                    const unsigned xx=x+dx;
                    if (xx<gpu_lowres.clip[0] || xx>=gpu_lowres.clip[2]) continue;
                    u16 *dst=g.vram+yy*1024u+xx;
                    if ((cf&1024u) && tm && ((g.blit_mask>>(((uintptr_t)dst>>1)&7u))&1u)) continue;
                    const u16 old=(blend||mask) ? *dst : 0;
                    if (mask && (old&0x8000u)) continue;
                    const bool do_blend=blend && (!tm || msb);
                    u16 out;
                    if (hq) {
                        const u32 col=do_blend ? gpuLowResBlend24(shaded24,old,(cf>>3)&3u) : shaded24;
                        out=gpuColorQuantization24<1>(col,dst);
                    } else out=do_blend ? gpuLowResBlend(shaded,old,(cf>>3)&3u) : shaded;
                    if (cf&256u) out|=0x8000u;
                    else if (tm && (light||blend)) out|=msb;
                    *dst=out;
#ifdef GPU_LOWRES_TEST_COUNTERS
                    ++gpu_lowres.writes;
#endif
                }
            }
            }
        }
        x+=scale;
        u=(u+(u32)g.u_inc)&g.u_msk; v=(v+(u32)g.v_inc)&g.v_msk;
        if (gouraud && (!tm||light)) gcol+=g.gInc;
    }
}

template<unsigned TM,bool LIGHT,bool GOURAUD,unsigned SHIFT>
static void gpuLowResOpaqueSpan(const gpu_unai_t &g,u16 *coarse,u32 count)
{
    const unsigned scale=1u<<SHIFT;
    const uintptr_t offset=((uintptr_t)coarse-(uintptr_t)g.vram)/2u;
    const unsigned y=(unsigned)(offset>>10)*scale;
    const unsigned top=y<gpu_lowres.clip[1]?gpu_lowres.clip[1]:y;
    const unsigned bottom=y+scale>gpu_lowres.clip[3]?gpu_lowres.clip[3]:y+scale;
    unsigned x=(unsigned)(offset&1023u)*scale;
    u32 u=g.u&g.u_msk,v=g.v&g.v_msk,color=g.gCol;
    const u8 *texture=(const u8 *)g.TBA;
    while(count--) {
#ifdef GPU_LOWRES_TEST_COUNTERS
        ++gpu_lowres.samples;
#endif
        u16 source;
        const unsigned tu=u>>10;
        if(TM==1) {
            const u8 packed=texture[((v<<1)&(255u<<11))+(tu>>1)];
            source=g.CBA[(packed>>((tu&1u)*4))&15u];
        } else if(TM==2) source=g.CBA[texture[((v<<1)&(255u<<11))+tu]];
        else source=g.TBA[(v&(255u<<10))+tu];
        if(source) {
            u16 out=source;
            if(LIGHT) {
                out=GOURAUD?gpuLightingTXTGouraud(source,color):gpuLightingTXT(source,g.r5,g.g5,g.b5);
                out|=source&0x8000u;
            }
            const unsigned left=x<gpu_lowres.clip[0]?gpu_lowres.clip[0]:x;
            const unsigned right=x+scale>gpu_lowres.clip[2]?gpu_lowres.clip[2]:x+scale;
            if(right>left) for(unsigned yy=top;yy<bottom;++yy) {
                if(yy&gpu_lowres.ilace) continue;
                gpuWriteCoarseColor(g.vram+yy*1024u+left,right-left,out);
#ifdef GPU_LOWRES_TEST_COUNTERS
                gpu_lowres.writes+=right-left;
#endif
            }
        }
        x+=scale;u=(u+(u32)g.u_inc)&g.u_msk;v=(v+(u32)g.v_inc)&g.v_msk;
        if(LIGHT&&GOURAUD) color+=g.gInc;
    }
}

static PP gpuLowResDriver(unsigned flags,unsigned shift)
{
#define LOWRES_OPAQUE(tm,l,g) (shift==1 ? gpuLowResOpaqueSpan<tm,l,g,1> : \
    (shift==2 ? gpuLowResOpaqueSpan<tm,l,g,2> : gpuLowResOpaqueSpan<tm,l,g,3>))
    switch(flags) {
    case 0x20:return LOWRES_OPAQUE(1,false,false);
    case 0x21:return LOWRES_OPAQUE(1,true,false);
    case 0xa1:return LOWRES_OPAQUE(1,true,true);
    case 0x40:return LOWRES_OPAQUE(2,false,false);
    case 0x41:return LOWRES_OPAQUE(2,true,false);
    case 0xc1:return LOWRES_OPAQUE(2,true,true);
    case 0x60:return LOWRES_OPAQUE(3,false,false);
    case 0x61:return LOWRES_OPAQUE(3,true,false);
    case 0xe1:return LOWRES_OPAQUE(3,true,true);
    default:return gpuLowResSpan;
    }
#undef LOWRES_OPAQUE
}

static bool gpuLowResBegin(unsigned shift,unsigned flags)
{
    if (gpu_lowres.shift || shift<1 || shift>3 || ProgressiveInterlaceEnabled()) return false;
    const unsigned tm=(flags>>5)&3u;
    if (tm && !gpuPotatoDisjoint(gpu_unai,tm)) return false;
    const unsigned scale=1u<<shift;
    for (unsigned i=0;i<4;++i) gpu_lowres.clip[i]=gpu_unai.DrawingArea[i];
    if (gpu_lowres.clip[0]>=gpu_lowres.clip[2] || gpu_lowres.clip[1]>=gpu_lowres.clip[3] ||
        gpu_lowres.clip[2]>1024 || gpu_lowres.clip[3]>512) return false;
    gpu_lowres.flags=flags; gpu_lowres.shift=shift;
    gpu_lowres.ilace=gpu_unai.ilace_mask; gpu_unai.ilace_mask=0;
    gpu_unai.DrawingArea[0]=gpu_lowres.clip[0]>>shift;
    gpu_unai.DrawingArea[1]=gpu_lowres.clip[1]>>shift;
    gpu_unai.DrawingArea[2]=(gpu_lowres.clip[2]+scale-1)>>shift;
    gpu_unai.DrawingArea[3]=(gpu_lowres.clip[3]+scale-1)>>shift;
    return true;
}
static void gpuLowResEnd()
{
    if (!gpu_lowres.shift) return;
    for (unsigned i=0;i<4;++i) gpu_unai.DrawingArea[i]=gpu_lowres.clip[i];
    gpu_unai.ilace_mask=gpu_lowres.ilace;
    gpu_lowres.shift=0;
}
#endif
