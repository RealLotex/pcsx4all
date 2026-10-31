/* Lossy, opt-in whole-scene render target. Texture storage stays native.
 * Draws accumulate at reduced resolution; expansion happens at VRAM barriers,
 * rather than after every fragment. All primitive families use this target. */
#ifndef PCSX4ALL_GPU_SCENE_H
#define PCSX4ALL_GPU_SCENE_H

static struct {
    u16 *target, *native;
    u16 clip[4];
    unsigned shift;
    bool valid, dirty;
    u8 ilace, blit;
#ifdef GPU_SCENE_TEST_COUNTERS
    uint64_t resolves, expanded, seeded, draws, fragments;
    unsigned driver_flags;
#endif
} gpu_scene;

#ifdef GPU_SCENE_TEST_COUNTERS
static void gpuSceneCountedSpan(const gpu_unai_t &g,u16 *dst,u32 count)
{
    gpu_scene.fragments+=count;
    gpuPolySpanDrivers[gpu_scene.driver_flags](g,dst,count);
}
#endif

static void gpuSceneFlush()
{
    if (!gpu_scene.dirty) return;
    const unsigned s=gpu_scene.shift, scale=1u<<s;
    for (unsigned y=gpu_scene.clip[1];y<gpu_scene.clip[3];++y) {
        u16 *dst=gpu_scene.native+y*1024u;
        const u16 *src=gpu_scene.target+(y>>s)*1024u;
        unsigned x=gpu_scene.clip[0];
        while (x<gpu_scene.clip[2]) {
            const unsigned end=Min2((x| (scale-1u))+1u,(unsigned)gpu_scene.clip[2]);
            gpuWriteCoarseColor(dst+x,end-x,src[x>>s]);
#ifdef GPU_SCENE_TEST_COUNTERS
            gpu_scene.expanded+=end-x;
#endif
            x=end;
        }
    }
    gpu_scene.dirty=false;
#ifdef GPU_SCENE_TEST_COUNTERS
    ++gpu_scene.resolves;
#endif
}

static void gpuSceneInvalidate()
{
    gpu_scene.valid=false;
    gpu_scene.dirty=false;
}

static bool gpuSceneOverlaps(int x,int y,int w,int h)
{
    if (!gpu_scene.valid) return false;
    // Wrapped transfers can touch either edge; resolve conservatively.
    if (x<0 || y<0 || w<0 || h<0 || x+w>1024 || y+h>512) return true;
    return x<(int)gpu_scene.clip[2] && x+w>(int)gpu_scene.clip[0] &&
           y<(int)gpu_scene.clip[3] && y+h>(int)gpu_scene.clip[1];
}

static bool gpuSceneBegin(unsigned flags)
{
    const unsigned shift=gpu_unai_config_ext.internal_resolution;
    if (!shift || shift>3) {
        gpuSceneFlush();gpuSceneInvalidate();return false;
    }
    if (!gpu_scene.target) {
        gpu_scene.target=(u16*)calloc(1024u*512u,sizeof(u16));
        if (!gpu_scene.target) {
            fprintf(stderr,"Reduced render target allocation failed; using original resolution\n");
            gpu_unai_config_ext.internal_resolution=0;return false;
        }
    }
    if (gpu_unai.DrawingArea[0]>=gpu_unai.DrawingArea[2] ||
        gpu_unai.DrawingArea[1]>=gpu_unai.DrawingArea[3] ||
        gpu_unai.DrawingArea[2]>1024 || gpu_unai.DrawingArea[3]>512) return false;
    if (shift!=gpu_scene.shift || memcmp(gpu_scene.clip,gpu_unai.DrawingArea,sizeof gpu_scene.clip)) {
        gpuSceneFlush();gpuSceneInvalidate();
        gpu_scene.shift=shift;
        memcpy(gpu_scene.clip,gpu_unai.DrawingArea,sizeof gpu_scene.clip);
    }
    gpu_scene.native=gpu_unai.vram;
    // Render-to-texture/CLUT dependencies must see previous primitives.
    const unsigned tm=(flags>>5)&3u;
    if (tm && !gpuPotatoDisjoint(gpu_unai,tm)) gpuSceneFlush();
    const unsigned scale=1u<<shift;
    if (!gpu_scene.valid) {
        for (unsigned y=gpu_scene.clip[1]>>shift;y<(gpu_scene.clip[3]+scale-1u)>>shift;++y)
            for (unsigned x=gpu_scene.clip[0]>>shift;x<(gpu_scene.clip[2]+scale-1u)>>shift;++x) {
                const unsigned nx=Max2(x*scale,(unsigned)gpu_scene.clip[0]);
                const unsigned ny=Max2(y*scale,(unsigned)gpu_scene.clip[1]);
                gpu_scene.target[y*1024u+x]=gpu_scene.native[ny*1024u+nx];
#ifdef GPU_SCENE_TEST_COUNTERS
                ++gpu_scene.seeded;
#endif
            }
        gpu_scene.valid=true;
    }
    gpu_lowres.shift=shift;
    gpu_scene.ilace=gpu_unai.ilace_mask;gpu_unai.ilace_mask=0;
    gpu_scene.blit=gpu_unai.blit_mask;gpu_unai.blit_mask=0;
    gpu_unai.vram=gpu_scene.target;
    gpu_unai.DrawingArea[0]=gpu_scene.clip[0]>>shift;
    gpu_unai.DrawingArea[1]=gpu_scene.clip[1]>>shift;
    gpu_unai.DrawingArea[2]=(gpu_scene.clip[2]+scale-1u)>>shift;
    gpu_unai.DrawingArea[3]=(gpu_scene.clip[3]+scale-1u)>>shift;
    gpu_scene.dirty=true;
#ifdef GPU_SCENE_TEST_COUNTERS
    ++gpu_scene.draws;
#endif
    return true;
}

static void gpuSceneEnd()
{
    gpu_unai.vram=gpu_scene.native;
    memcpy(gpu_unai.DrawingArea,gpu_scene.clip,sizeof gpu_scene.clip);
    gpu_unai.ilace_mask=gpu_scene.ilace;gpu_unai.blit_mask=gpu_scene.blit;
    gpu_lowres.shift=0;
}
#endif
