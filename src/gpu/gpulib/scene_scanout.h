/* Whole-display nearest sampling, also for CPU-uploaded images and RGB24
 * video. Rasterized primitives have already used the reduced render target. */
#ifndef PCSX4ALL_SCENE_SCANOUT_H
#define PCSX4ALL_SCENE_SCANOUT_H
#include <stdint.h>
#include <string.h>
static inline uint16_t gpuSceneDisplayPixel(const uint16_t *vram,unsigned offset,
                                           unsigned x,bool rgb24,unsigned shift)
{
    const unsigned mask=(1u<<shift)-1u;
    const unsigned row=((offset>>10)&~mask)*1024u;
    if (!rgb24) {
        const unsigned col=((offset&1023u)+x)&~mask;
        return vram[(row+col)&(1024u*512u-1u)];
    }
    const unsigned byte=(row*2u+(offset&1023u)*2u+(x&~mask)*3u)&(1024u*1024u-1u);
    const uint8_t *p=(const uint8_t*)vram;
    const unsigned r=p[byte],g=p[(byte+1u)&1048575u],b=p[(byte+2u)&1048575u];
    return (uint16_t)((r>>3)|((g&248u)<<2)|((b&248u)<<7));
}

typedef uint32_t __attribute__((__may_alias__)) gpu_scene_pair_t;
static inline void gpuSceneRepeatPixel(uint16_t *dst,unsigned n,uint16_t color)
{
    if (((uintptr_t)dst&2u) && n) {*dst++=color;--n;}
    const uint32_t pair=(uint32_t)color|((uint32_t)color<<16);
    gpu_scene_pair_t *words=(gpu_scene_pair_t*)(void*)dst;
    while(n>=2u) {*words++=pair;n-=2u;}
    if(n) *(uint16_t*)(void*)words=color;
}
#ifdef GPU_SCENE_SCANOUT_TEST_COUNTERS
static uint64_t gpu_scene_sample_reads,gpu_scene_converted_rows,gpu_scene_copied_rows;
#endif
// Same nearest-neighbor output as the scalar scanout, including VRAM wrapping.
// Source and 320-pixel destination are disjoint; step is 1024 or 2048 words.
// Group identical horizontal samples, and copy repeated output rows instead
// of reading and converting the same source pixels again.
template<bool RGB24,bool BGR15,bool SURFACE>
static void gpuSceneGroupedScanout(const uint16_t *vram,unsigned offset,unsigned width,
                                 unsigned rows,unsigned step,unsigned shift,uint16_t *dst)
{
    unsigned samples[320],ends[320],runs=0,previous=~0u;
    const unsigned x0=offset&1023u;
    for(unsigned x=0;x<320u;++x) {
        const unsigned sx=x*width/320u;
        const unsigned key=(RGB24?sx:(x0+sx))>>shift;
        if(key!=previous) {samples[runs]=sx;ends[runs++]=x;previous=key;}
        ends[runs-1u]=x+1u;
    }
    previous=~0u;
    for(unsigned y=0;y<rows;++y) {
        const unsigned key=offset>>(10u+shift);
        if(key==previous) {
            memcpy(dst,dst-320u,320u*sizeof(uint16_t));
#ifdef GPU_SCENE_SCANOUT_TEST_COUNTERS
            ++gpu_scene_copied_rows;
#endif
        } else {
            unsigned x=0;
            for(unsigned i=0;i<runs;++i) {
                const uint16_t c=SURFACE ? vram[(offset>>(10u+shift))*1024u+((x0+samples[i])>>shift)] :
                                          gpuSceneDisplayPixel(vram,offset,samples[i],RGB24,shift);
                const uint16_t color=BGR15 ? c : (uint16_t)(((c&0x7c00u)>>10)|((c&0x03e0u)<<1)|((c&31u)<<11));
                gpuSceneRepeatPixel(dst+x,ends[i]-x,color);x=ends[i];
#ifdef GPU_SCENE_SCANOUT_TEST_COUNTERS
                ++gpu_scene_sample_reads;
#endif
            }
#ifdef GPU_SCENE_SCANOUT_TEST_COUNTERS
            ++gpu_scene_converted_rows;
#endif
        }
        previous=key;dst+=320u;offset=(offset+step)&524287u;
    }
}
template<bool RGB24,bool BGR15>
static void gpuSceneNativeScanout(const uint16_t *vram,unsigned offset,unsigned width,
                                 unsigned rows,unsigned step,unsigned shift,uint16_t *dst)
{
    gpuSceneGroupedScanout<RGB24,BGR15,false>(vram,offset,width,rows,step,shift,dst);
}
// Sparse reduced surface with 1024-word stride; the caller guarantees that
// the sampled viewport is inside a valid surface and does not wrap in X.
template<bool BGR15>
static void gpuSceneSurfaceScanout(const uint16_t *surface,unsigned offset,unsigned width,
                                  unsigned rows,unsigned step,unsigned shift,uint16_t *dst)
{
    gpuSceneGroupedScanout<false,BGR15,true>(surface,offset,width,rows,step,shift,dst);
}
#endif
