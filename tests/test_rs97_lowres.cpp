#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define USE_GPULIB
#define INLINE static inline
#define GPU_INLINE static inline __attribute__((always_inline))
#define GPU_UNAI_LOWRES_EXPERIMENT
#define GPU_LOWRES_TEST_COUNTERS
#include <gpu_unai.h>
#include <gpu_fixedpoint.h>
#include <gpu_inner.h>
#include <gpu_raster_polygon.h>
static u16 pixels[1024u*512u] __attribute__((aligned(16384)));
static u16 expected[1024u*512u] __attribute__((aligned(16384)));
static u32 seed=0x35ad347bu;
static u32 rnd() { seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed; }
static void memory(u16 *p) {
    gpu_unai.vram=p;gpu_unai.TBA=p+256u*1024u+512u;gpu_unai.CBA=p+500u*1024u;
}
static uint64_t native_samples;
static void count_native(const gpu_unai_t &g,u16 *p,u32 n) {
    native_samples+=n;gpuPolySpanDrivers[0x21](g,p,n);
}
int main() {
    SetupLightLUT();SetupDitheringConstants();
    if(gpuLowResBegin(0,0x21) || gpuLowResBegin(4,0x21)) return puts("FAIL unsupported scale"),1;
    gpu_unai.DrawingArea[0]=5;gpu_unai.DrawingArea[1]=7;
    gpu_unai.DrawingArea[2]=51;gpu_unai.DrawingArea[3]=27;
    gpu_unai.u_msk=gpu_unai.v_msk=0x3ffffu;
    gpu_unai.TextureWindow[2]=gpu_unai.TextureWindow[3]=255;
    for (u32 i=0;i<1024u*512u;++i) pixels[i]=(u16)rnd();
    // Every instantiated normal path is an oracle for the per-native-pixel
    // destination effects of a shared coarse source sample.
    unsigned cases=0;
    for(unsigned shift=1;shift<=3;++shift) for (unsigned cf=0;cf<2048;++cf) {
        if (gpuPolySpanDrivers[cf]==PolyNULL) continue;
        const unsigned scale=1u<<shift;
        gpu_unai.DrawingArea[0]=2*scale+1;gpu_unai.DrawingArea[1]=4*scale+1;
        gpu_unai.DrawingArea[2]=11*scale-1;gpu_unai.DrawingArea[3]=5*scale;
        memory(pixels);
        for(unsigned i=0;i<256;++i) gpu_unai.CBA[i]=(i%7==0)?0:(u16)rnd();
        for(unsigned y=0;y<32;++y) for(unsigned x=0;x<64;++x) pixels[y*1024+x]=(u16)rnd();
        memcpy(expected,pixels,sizeof pixels);
        gpu_unai.u=rnd()&0x3ffff;gpu_unai.v=rnd()&0x3ffff;
        gpu_unai.u_inc=(s32)(rnd()%8192)-4096;gpu_unai.v_inc=(s32)(rnd()%8192)-4096;
        gpu_unai.gCol=rnd();gpu_unai.gInc=rnd();
        gpu_unai.PixelData=(u16)(rnd()&0x7fffu);
        gpu_unai.r5=rnd()&31;gpu_unai.g5=rnd()&31;gpu_unai.b5=rnd()&31;
        gpu_unai.r8=(u8)rnd();gpu_unai.g8=(u8)rnd();gpu_unai.b8=(u8)rnd();
        gpu_unai.blit_mask=(u8)rnd();gpu_unai.ilace_mask=(cf&8)?1:0;
        const gpu_unai_t initial=gpu_unai;
        if (!gpuLowResBegin(shift,cf)) return puts("FAIL begin"),1;
        gpuLowResDriver(cf,shift)(gpu_unai,pixels+4u*1024u+2u,11);
        gpuLowResEnd();
        if (memcmp(gpu_unai.DrawingArea,initial.DrawingArea,sizeof initial.DrawingArea) || gpu_unai.ilace_mask!=initial.ilace_mask)
            return puts("FAIL restore"),1;
        gpu_unai=initial;memory(expected);
        for (unsigned cell=0;cell<11;++cell) {
            for(unsigned dy=0;dy<scale;++dy) for(unsigned dx=0;dx<scale;++dx) {
                const unsigned x=(2+cell)*scale+dx,y=4*scale+dy;
                if(x<initial.DrawingArea[0] || x>=initial.DrawingArea[2] ||
                   y<initial.DrawingArea[1] || y>=initial.DrawingArea[3] || (y&initial.ilace_mask)) continue;
                gpuPolySpanDrivers[cf](gpu_unai,expected+y*1024u+x,1);
            }
            gpu_unai.u=(gpu_unai.u+(u32)gpu_unai.u_inc)&gpu_unai.u_msk;
            gpu_unai.v=(gpu_unai.v+(u32)gpu_unai.v_inc)&gpu_unai.v_msk;
            if ((cf&128u) && (cf&1u)) gpu_unai.gCol+=gpu_unai.gInc;
        }
        if(memcmp(pixels,expected,sizeof pixels)) {printf("FAIL flags=%x\n",cf);return 1;}
        ++cases;
        gpu_unai=initial;memory(pixels);
    }
    // Exercise the actual triangle scan converter, not only its span shader.
    gpu_unai.DrawingArea[0]=gpu_unai.DrawingArea[1]=0;
    gpu_unai.DrawingArea[2]=320;gpu_unai.DrawingArea[3]=240;gpu_unai.ilace_mask=0;
    u32 words[7]={0x00808080u,10u|(10u<<16),0u,150u|(10u<<16),127u,40u|(120u<<16),127u<<8};
    PtrUnion packet;packet.U4=words;
    native_samples=0;gpuDrawPolyFT(packet,count_native,false);
    for(unsigned shift=1;shift<=3;++shift) {
        gpu_lowres.samples=0;
        if(!gpuLowResBegin(shift,0x21)) return puts("FAIL polygon begin"),1;
        gpuDrawPolyFT(packet,gpuLowResSpan,false);
        gpuLowResEnd();
        const uint64_t samples=gpu_lowres.samples;
        if(!samples || samples>native_samples/(1u<<(2*shift))+150) return puts("FAIL raster sample reduction"),1;
        printf("Raster samples native=%llu shift=%u reduced=%llu\n",(unsigned long long)native_samples,shift,(unsigned long long)samples);
    }
    if(!gpuTryLowResPolygon(packet,0x21,false,POLYTYPE_FT,1) || gpu_lowres.shift)
        return puts("FAIL polygon dispatch and restore"),1;
    words[5]=10u|(120u<<16);
    if(gpuTryLowResPolygon(packet,0x21,false,POLYTYPE_FT,1)) return puts("FAIL 2D glyph bypass"),1;
    memory(pixels);gpu_unai.TBA=pixels;
    if(gpuLowResBegin(1,0x21)) return puts("FAIL feedback rejection"),1;
    if(!gpuLowResBegin(1,0x81) || gpuLowResBegin(2,0x81)) return puts("FAIL nested begin"),1;
    PolyVertex limit[3];memset(limit,0,sizeof limit);
    limit[0].x=0;limit[0].y=0;limit[0].native_x=0;limit[0].native_y=0;
    limit[1].x=512;limit[1].y=0;limit[1].native_x=1024;limit[1].native_y=0;
    limit[2].x=0;limit[2].y=20;limit[2].native_x=0;limit[2].native_y=40;
    const PolyVertex *ordered[3];
    if(polyUseTriangle(limit,0,ordered)) return puts("FAIL original polygon size limit"),1;
    gpuLowResEnd();
    printf("PASS %u low-resolution shader cases, native destination effects and triangle raster reduction\n",cases);
}
