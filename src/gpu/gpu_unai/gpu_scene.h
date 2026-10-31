/* Opt-in lossy render surfaces. Keep both PS1 framebuffers coarse across
 * flips; expand only for native VRAM consumers. Texture/CLUT storage is native. */
#ifndef PCSX4ALL_GPU_SCENE_H
#define PCSX4ALL_GPU_SCENE_H
struct gpu_scene_cache_t {
    u16 clip[4];
    bool valid,dirty;
};
static struct {
    u16 *target,*native,clip[4];
    gpu_scene_cache_t cache[2];
    unsigned active,shift;
    u8 ilace,blit;
#ifdef GPU_SCENE_TEST_COUNTERS
    uint64_t resolves,expanded,seeded,draws,fragments;
    unsigned driver_flags;
#endif
} gpu_scene;
static gpu_scene_cache_t &gpuSceneActive() { return gpu_scene.cache[gpu_scene.active]; }

#ifdef GPU_SCENE_TEST_COUNTERS
static void gpuSceneCountedSpan(const gpu_unai_t &g,u16 *dst,u32 count)
{
    gpu_scene.fragments+=count;
    gpuPolySpanDrivers[gpu_scene.driver_flags](g,dst,count);
}
#endif

static void gpuSceneResolve(gpu_scene_cache_t &c)
{
    if (!c.dirty) return;
    const unsigned s=gpu_scene.shift,scale=1u<<s;
    for (unsigned y=c.clip[1];y<c.clip[3];++y) {
        u16 *dst=gpu_scene.native+y*1024u;
        const u16 *src=gpu_scene.target+(y>>s)*1024u;
        unsigned x=c.clip[0];
        while (x<c.clip[2]) {
            const unsigned end=Min2((x|(scale-1u))+1u,(unsigned)c.clip[2]);
            gpuWriteCoarseColor(dst+x,end-x,src[x>>s]);
#ifdef GPU_SCENE_TEST_COUNTERS
            gpu_scene.expanded+=end-x;
#endif
            x=end;
        }
    }
    c.dirty=false;
#ifdef GPU_SCENE_TEST_COUNTERS
    ++gpu_scene.resolves;
#endif
}
static void gpuSceneFlush()
{
    for (unsigned i=0;i<2;++i) gpuSceneResolve(gpu_scene.cache[i]);
}
static void gpuSceneInvalidate()
{
    for (unsigned i=0;i<2;++i) gpu_scene.cache[i].valid=gpu_scene.cache[i].dirty=false;
}
static bool gpuSceneOverlaps(const gpu_scene_cache_t &c,int x,int y,int w,int h)
{
    if (!c.valid) return false;
    if (x<0 || y<0 || w<0 || h<0 || x>1024 || y>512 || w>1024-x || h>512-y) return true;
    return x<(int)c.clip[2] && x+w>(int)c.clip[0] &&
           y<(int)c.clip[3] && y+h>(int)c.clip[1];
}
static void gpuSceneWriteBegin(int x,int y,int w,int h)
{
    for (unsigned i=0;i<2;++i)
        if (gpuSceneOverlaps(gpu_scene.cache[i],x,y,w,h)) gpuSceneResolve(gpu_scene.cache[i]);
}
static void gpuSceneWriteEnd(int x,int y,int w,int h)
{
    for (unsigned i=0;i<2;++i) {
        gpu_scene_cache_t &c=gpu_scene.cache[i];
        if (gpuSceneOverlaps(c,x,y,w,h)) c.valid=c.dirty=false;
    }
}
static bool gpuSceneBegin(unsigned flags,bool clearing=false)
{
    const unsigned s=gpu_unai_config_ext.internal_resolution;
    if (!s || s>3) {gpuSceneFlush();gpuSceneInvalidate();return false;}
    if (!gpu_scene.target) {
        gpu_scene.target=(u16*)calloc(1024u*512u,sizeof(u16));
        if (!gpu_scene.target) {
            fprintf(stderr,"Reduced render target allocation failed; using original resolution\n");
            gpu_unai_config_ext.internal_resolution=0;return false;
        }
    }
    const u16 *clip=gpu_unai.DrawingArea;
    if (clip[0]>=clip[2] || clip[1]>=clip[3] || clip[2]>1024 || clip[3]>512) return false;
    if (s!=gpu_scene.shift) {gpuSceneFlush();gpuSceneInvalidate();gpu_scene.shift=s;}
    gpu_scene.native=gpu_unai.vram;
    // Texture reads may depend on either framebuffer, not just the draw area.
    const unsigned tm=(flags>>5)&3u;
    if (tm) {
        u16 saved[4];memcpy(saved,clip,sizeof saved);
        for (unsigned i=0;i<2;++i) if (gpu_scene.cache[i].dirty) {
            memcpy(gpu_unai.DrawingArea,gpu_scene.cache[i].clip,sizeof saved);
            if (!gpuPotatoDisjoint(gpu_unai,tm)) gpuSceneResolve(gpu_scene.cache[i]);
        }
        memcpy(gpu_unai.DrawingArea,saved,sizeof saved);
    }
    unsigned chosen=2;
    for (unsigned i=0;i<2;++i)
        if (gpu_scene.cache[i].valid && !memcmp(gpu_scene.cache[i].clip,clip,sizeof gpu_scene.clip)) chosen=i;
    if (chosen==2) {
        // Coarse cells, not only native rectangles, must be disjoint.
        const unsigned bias=(1u<<s)-1u;
        for (unsigned i=0;i<2;++i) {
            gpu_scene_cache_t &c=gpu_scene.cache[i];
            if (c.valid && (clip[0]>>s)<((c.clip[2]+bias)>>s) &&
                ((clip[2]+bias)>>s)>(c.clip[0]>>s) &&
                (clip[1]>>s)<((c.clip[3]+bias)>>s) &&
                ((clip[3]+bias)>>s)>(c.clip[1]>>s)) {
                gpuSceneResolve(c);c.valid=false;
            }
            if (!c.valid) chosen=i;
        }
        if (chosen==2) {chosen=gpu_scene.active^1u;gpuSceneResolve(gpu_scene.cache[chosen]);}
        gpu_scene_cache_t &c=gpu_scene.cache[chosen];
        memcpy(c.clip,clip,sizeof c.clip);c.valid=true;c.dirty=false;
        if (!clearing) for (unsigned y=clip[1]>>s;y<(clip[3]+bias)>>s;++y)
            for (unsigned x=clip[0]>>s;x<(clip[2]+bias)>>s;++x) {
                gpu_scene.target[y*1024u+x]=gpu_scene.native[Max2(y<<s,(unsigned)clip[1])*1024u+Max2(x<<s,(unsigned)clip[0])];
#ifdef GPU_SCENE_TEST_COUNTERS
                ++gpu_scene.seeded;
#endif
            }
    }
    gpu_scene.active=chosen;memcpy(gpu_scene.clip,clip,sizeof gpu_scene.clip);
    gpu_lowres.shift=s;
    gpu_scene.ilace=gpu_unai.ilace_mask;gpu_unai.ilace_mask=0;
    gpu_scene.blit=gpu_unai.blit_mask;gpu_unai.blit_mask=0;
    gpu_unai.vram=gpu_scene.target;
    const unsigned bias=(1u<<s)-1u;
    gpu_unai.DrawingArea[0]=gpu_scene.clip[0]>>s;
    gpu_unai.DrawingArea[1]=gpu_scene.clip[1]>>s;
    gpu_unai.DrawingArea[2]=(gpu_scene.clip[2]+bias)>>s;
    gpu_unai.DrawingArea[3]=(gpu_scene.clip[3]+bias)>>s;
    gpuSceneActive().dirty=true;
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
static bool gpuSceneClear(PtrUnion packet)
{
    const int x=Max2((int)packet.S2[2],0),y=Max2((int)packet.S2[3],0);
    const int right=Min2((int)packet.S2[2]+(packet.S2[4]&1023),1024);
    const int bottom=Min2((int)packet.S2[3]+(packet.S2[5]&1023),512);
    if (right<=x || bottom<=y || !gpu_unai_config_ext.internal_resolution) return false;
    u16 saved[4];memcpy(saved,gpu_unai.DrawingArea,sizeof saved);
    gpu_unai.DrawingArea[0]=x;gpu_unai.DrawingArea[1]=y;
    gpu_unai.DrawingArea[2]=right;gpu_unai.DrawingArea[3]=bottom;
    const bool reduced=gpuSceneBegin(0,true);
    if (reduced) {
        const u16 color=GPU_RGB16(packet.U4[0]);
        for (unsigned yy=gpu_unai.DrawingArea[1];yy<gpu_unai.DrawingArea[3];++yy)
            for(unsigned xx=gpu_unai.DrawingArea[0];xx<gpu_unai.DrawingArea[2];++xx)
                gpu_unai.vram[yy*1024u+xx]=color;
        gpuSceneEnd();
    }
    memcpy(gpu_unai.DrawingArea,saved,sizeof saved);
    return reduced;
}
#endif
