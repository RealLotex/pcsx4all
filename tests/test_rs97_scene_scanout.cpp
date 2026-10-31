#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define GPU_SCENE_SCANOUT_TEST_COUNTERS
#include "../src/gpu/gpulib/scene_scanout.h"
static uint16_t vram[524288],actual[320u*240u+16u],expected[320u*240u+16u];
static uint32_t seed=0x91ab391u;
static uint32_t rnd() {seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
template<bool RGB24,bool BGR15>
static bool compare(unsigned offset,unsigned width,unsigned rows,unsigned step,unsigned shift,unsigned alignment)
{
    memset(actual,0xa5,sizeof actual);memset(expected,0xa5,sizeof expected);
    gpuSceneNativeScanout<RGB24,BGR15>(vram,offset,width,rows,step,shift,actual+alignment);
    for(unsigned y=0;y<rows;++y) {
        for(unsigned x=0;x<320;++x) {
            const uint16_t c=gpuSceneDisplayPixel(vram,offset,x*width/320u,RGB24,shift);
            expected[alignment+y*320u+x]=BGR15?c:(uint16_t)(((c&0x7c00u)>>10)|((c&0x03e0u)<<1)|((c&31u)<<11));
        }
        offset=(offset+step)&524287u;
    }
    if(memcmp(actual,expected,sizeof actual)) {
        printf("FAIL rgb24=%u bgr15=%u width=%u rows=%u step=%u shift=%u align=%u\n",RGB24,BGR15,width,rows,step,shift,alignment);return false;
    }
    return true;
}
template<bool BGR15>
static bool compareSurface(unsigned offset,unsigned width,unsigned rows,unsigned step,unsigned shift,unsigned alignment)
{
    memset(actual,0xa5,sizeof actual);memset(expected,0xa5,sizeof expected);
    gpuSceneSurfaceScanout<BGR15>(vram,offset,width,rows,step,shift,actual+alignment);
    for(unsigned y=0;y<rows;++y) {
        for(unsigned x=0;x<320;++x) {
            const unsigned sx=((offset&1023u)+x*width/320u)>>shift;
            const unsigned sy=(offset>>10)>>shift;
            const uint16_t c=vram[sy*1024u+sx];
            expected[alignment+y*320u+x]=BGR15?c:(uint16_t)(((c&0x7c00u)>>10)|((c&0x03e0u)<<1)|((c&31u)<<11));
        }
        offset=(offset+step)&524287u;
    }
    if(memcmp(actual,expected,sizeof actual)) {
        printf("FAIL surface bgr15=%u width=%u rows=%u step=%u shift=%u align=%u\n",BGR15,width,rows,step,shift,alignment);return false;
    }
    return true;
}
int main() {
    for(unsigned i=0;i<524288u;++i) vram[i]=(uint16_t)rnd();
    const unsigned widths[]={256,320,368,384,512,640};
    const unsigned offsets[]={0,1,7u*1024u+3u,239u*1024u,240u*1024u+1u,255u*1024u+1015u,510u*1024u+1023u,511u*1024u+1015u};
    const unsigned heights[]={1,7,33,240};unsigned cases=0;
    for(unsigned s=1;s<=3;++s) for(unsigned wi=0;wi<6;++wi) for(unsigned oi=0;oi<8;++oi)
    for(unsigned hi=0;hi<4;++hi) for(unsigned twice=0;twice<2;++twice) for(unsigned a=0;a<2;++a) {
        const unsigned off=offsets[oi],w=widths[wi],rows=heights[hi],step=1024u<<twice;
        if(!compare<false,false>(off,w,rows,step,s,a) || !compare<true,false>(off,w,rows,step,s,a) ||
           !compare<false,true>(off,w,rows,step,s,a) || !compare<true,true>(off,w,rows,step,s,a)) return 1;
        cases+=4;
        if((off&1023u)+w<=1024u) {
            if(!compareSurface<false>(off,w,rows,step,s,a) || !compareSurface<true>(off,w,rows,step,s,a)) return 1;
            cases+=2;
        }
    }
    for(unsigned s=1;s<=3;++s) {
        gpu_scene_sample_reads=gpu_scene_converted_rows=gpu_scene_copied_rows=0;
        gpuSceneNativeScanout<false,false>(vram,0,320,240,1024,s,actual);
        const uint64_t samples=(320u>>s)*(240u>>s);
        if(gpu_scene_sample_reads!=samples || gpu_scene_converted_rows!=(240u>>s) ||
           gpu_scene_copied_rows!=240u-(240u>>s)) return puts("FAIL duplicated work"),1;
        printf("shift=%u samples=%llu converted rows=%llu copied rows=%llu\n",s,
               (unsigned long long)samples,(unsigned long long)gpu_scene_converted_rows,(unsigned long long)gpu_scene_copied_rows);
        gpu_scene_sample_reads=gpu_scene_converted_rows=gpu_scene_copied_rows=0;
        gpuSceneSurfaceScanout<false>(vram,0,320,240,1024,s,actual);
        if(gpu_scene_sample_reads!=samples || gpu_scene_converted_rows!=(240u>>s) ||
           gpu_scene_copied_rows!=240u-(240u>>s)) return puts("FAIL duplicated surface work"),1;
    }
    printf("PASS scene scanout equivalence: %u cases, RGB15/RGB24, output formats, clipping origins and VRAM wrap\n",cases);
}
