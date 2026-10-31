/* Whole-display nearest sampling, also for CPU-uploaded images and RGB24
 * video. Rasterized primitives have already used the reduced render target. */
#ifndef PCSX4ALL_SCENE_SCANOUT_H
#define PCSX4ALL_SCENE_SCANOUT_H
#include <stdint.h>
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
#endif
