/* Exercise production command dispatch, target lifetime and display sampling.
 * Intentionally lossy mode: native equality is required only at shift zero. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define GPU_SCENE_TEST_COUNTERS
#include "gpu/gpu_unai/gpulib_if.cpp"
#include "gpu/gpulib/scene_scanout.h"
#include "plugins.h"
#include "plugin_lib.h"
pl_data_t pl_data;
void pl_clear_borders() {}
static uint16_t screen[320u*240u];
unsigned short *SCREEN=screen;
static unsigned flips;
void video_flip() { ++flips; }
static uint16_t memory[1024u*512u] __attribute__((aligned(16384)));
static uint16_t baseline[1024u*512u];
static unsigned xy(unsigned x,unsigned y) { return x|(y<<16); }
static void commands(uint32_t *p,unsigned n) {
    int last=-1;
    assert(do_cmd_list(p,n,&last)==(int)n);
}
static void init(unsigned shift) {
    memset(memory,0,sizeof memory);gpu.vram=memory;
    renderer_init();gpu_unai_config_ext.internal_resolution=shift;
    gpu_unai.config.blending=1;gpu_unai.config.lighting=1;
    uint32_t settings[]={0xe3000000u,0xe4000000u|319u|(239u<<10),0xe5000000u,0xe1000108u};
    commands(settings,4);
    // Native 16-bit texture at x512, y0, deliberately outside the draw clip.
    for (unsigned y=0;y<256;++y) for(unsigned x=0;x<256;++x)
        memory[y*1024u+512+x]=(uint16_t)(1+(x%31)+((y%31)<<5));
    renderer_update_caches(0,0,1024,512);
}
int main() {
    uint32_t tile[]={0x600000ffu,xy(0,0),xy(320,240)};
    uint32_t flat[]={0x2000ff00u,xy(16,16),xy(144,16),xy(16,144)};
    uint32_t ft[]={0x25008080u,xy(160,16),0,xy(288,16),0x1080000u|128,xy(160,144),128<<8};
    uint32_t g[]={0x300000ffu,xy(16,160),0x0000ff00u,xy(144,160),0x00ff0000u,xy(16,224)};
    uint32_t gt[]={0x35808080u,xy(160,160),0,0x00808080u,xy(288,160),0x1080000u|128,0x00808080u,xy(160,224),64<<8};
    uint32_t line[]={0x4000ffffu,xy(8,8),xy(296,232)};
    uint32_t gl[]={0x5000ffffu,xy(8,232),0xff0000,xy(296,8)};
    uint32_t sprite[]={0x65808080u,xy(80,80),0,xy(48,48)};
    for(unsigned s=0;s<=3;++s) {
        init(s);
        commands(tile,3);commands(flat,4);commands(ft,7);commands(g,6);
        commands(gt,9);commands(line,3);commands(gl,4);commands(sprite,4);
        if(s) {
            assert(gpuSceneActive().dirty && gpu_lowres.shift==0 && gpu_unai.vram==memory);
            assert(memory[0]==0); // No per-fragment native-size expansion.
            const uint64_t n=gpu_scene.resolves;
            renderer_flush_queues();assert(gpu_scene.resolves==n+1);
            renderer_flush_queues();assert(gpu_scene.resolves==n+1);
            for(unsigned y=0;y<240;++y) for(unsigned x=0;x<320;++x)
                assert(memory[y*1024+x]==memory[(y>>s)*(1u<<s)*1024+(x>>s)*(1u<<s)]);
            assert(gpu_scene.seeded==((320u>>s)*(240u>>s)));
            assert(gpu_scene.draws==8);
        } else {
            memcpy(baseline,memory,sizeof memory);
            // Whole-scene disabled must still run the native renderer.
            assert(!gpuSceneActive().dirty && gpu_scene.draws==0);
        }
        // A CPU upload must invalidate the target and seed from fresh data.
        renderer_flush_queues();memory[0]=0x7777;renderer_update_caches(0,0,1,1);
        uint32_t dot[]={0x6800ff00u,xy(24,24)};commands(dot,2);
        renderer_flush_queues();assert(memory[0]==0x7777);
        if(s) assert(memory[24u*1024u+24u]!=0);
        renderer_finish();
    }
    // Native mode is independent of whether the reduced target ran earlier.
    init(0);commands(tile,3);commands(flat,4);commands(ft,7);commands(g,6);
    commands(gt,9);commands(line,3);commands(gl,4);commands(sprite,4);
    assert(!memcmp(memory,baseline,sizeof memory));renderer_finish();
    for(unsigned s=1;s<=3;++s) {
        init(s);
        // Eight overlapping quads cost 8*(64/scale)^2 target writes,
        // followed by one expansion of the clip. The old path expanded
        // every sample immediately, so overdraw remained native-size.
        uint32_t q[]={0x280000ffu,xy(16,16),xy(80,16),xy(16,80),xy(80,80)};
        for(unsigned i=0;i<8;++i) commands(q,5);
        assert(gpu_scene.fragments==8u*(64u>>s)*(64u>>s));
        assert(gpu_scene.expanded==0 && memory[16u*1024u+16u]==0);
        // Present directly from the small surface, without expanding VRAM.
        gpu.screen.x=gpu.screen.y=0;gpu.screen.hres=320;gpu.screen.vres=240;gpu.screen.h=240;
        gpu.status.rgb24=0;gpu.status.blanking=0;gpu.state.fb_dirty=1;
        GPUScreenInfo_t info;GPU_getScreenInfo(&info);
        assert(gpu_scene.expanded==0); // per-frame metadata is not a readback
        GPU_updateLace();assert(gpu_scene.expanded==0);
        assert(screen[16u*320u+16u]==0xf800u);
        pl_data.sinfo=info;uint16_t preview[160u*120u];
        pl_screenshot_160x120_rgb565(preview);assert(gpu_scene.resolves==1);
        renderer_flush_queues();assert(gpu_scene.resolves==1);
        assert(gpu_scene.expanded==320u*240u);
        printf("shift=%u opaque target fragments=%llu native resolve writes=%llu\n",s,
               (unsigned long long)gpu_scene.fragments,(unsigned long long)gpu_scene.expanded);
        // Resolve before framebuffer-as-texture sampling. All native texture
        // pointers must stay outside the shadow target.
        uint32_t page[]={0xe1000100u};commands(page,1);
        const uint64_t n=gpu_scene.resolves;
        commands(q,5);
        uint32_t fb[]={0x65808080u,xy(160,0),16u|(16u<<8),xy(16,16)};
        commands(fb,4);assert(gpu_scene.resolves==n+1);
        renderer_flush_queues();assert(memory[160]==31);
        // Masked destinations are evaluated on the small target.
        uint32_t maskset[]={0xe6000001u};commands(maskset,1);commands(q,5);
        uint32_t maskcheck[]={0xe6000002u};commands(maskcheck,1);
        q[0]=0x2800ff00u;commands(q,5);renderer_flush_queues();
        assert(memory[16u*1024u+16u]==(0x8000u|31u));
        // VRAM-to-VRAM copies must consume the completed reduced image.
        uint32_t move[]={0x80000000u,xy(16,16),xy(400,300),xy(8,8)};
        commands(move,4);assert(memory[300u*1024u+400u]==(0x8000u|31u));
        // Texture-only uploads keep the unrelated target valid and dirty.
        commands(q,5);const uint64_t resolves=gpu_scene.resolves;
        renderer_vram_write_begin(700,400,8,8);memory[400u*1024u+700u]=123;
        renderer_update_caches(700,400,8,8);
        assert(gpuSceneActive().valid && gpuSceneActive().dirty && gpu_scene.resolves==resolves);
        // Save/load must resolve pending pixels and discard the old surface
        // before restoring native VRAM; a later draw must not resurrect it.
        static GPUFreeze_t state;
        GPU_freeze(1,&state);assert(!gpuSceneActive().dirty);
        commands(tile,3);GPU_freeze(0,&state);assert(!gpuSceneActive().valid);
        assert(!memcmp(memory,state.psxVRam,sizeof memory));
        renderer_finish();
    }
    // Gran Turismo alternates y=0 and y=240 framebuffers. Keep both reduced:
    // changing draw area and displaying the other buffer must not resolve it.
    for(unsigned s=1;s<=3;++s) {
        init(s);
        uint32_t a[]={0x020000ffu,xy(0,0),xy(320,240)};
        uint32_t b[]={0x0200ff00u,xy(0,240),xy(320,240)};
        commands(a,3);commands(b,3);
        assert(gpu_scene.seeded==0 && gpu_scene.expanded==0);
        assert(gpu_scene.cache[0].valid && gpu_scene.cache[1].valid);
        for(unsigned flip=0;flip<8;++flip) {
            const unsigned y=(flip&1u)?240:0;
            uint32_t area[]={0xe3000000u|(y<<10),0xe4000000u|319u|((y+239u)<<10)};
            commands(area,2);
            commands((flip&1u)?b:a,3);
            gpu.screen.x=0;gpu.screen.y=y;gpu.screen.hres=320;gpu.screen.vres=240;gpu.screen.h=240;
            gpu.status.rgb24=0;gpu.status.blanking=0;gpu.state.fb_dirty=1;
            GPU_updateLace();assert(gpu_scene.expanded==0);
            const uint16_t expected=(flip&1u)?0x07c0u:0xf800u;
            for(unsigned i=0;i<320u*240u;++i) assert(screen[i]==expected);
        }
        assert(memory[0]==0 && memory[240u*1024u]==0);
        renderer_flush_queues();assert(gpu_scene.resolves==2);
        assert(memory[0]==31 && memory[240u*1024u]==(31u<<5));
        // A write to only the top buffer preserves the bottom cached image.
        renderer_vram_write_begin(0,0,320,240);
        renderer_update_caches(0,0,320,240);
        unsigned valid=0;for(unsigned i=0;i<2;++i) valid+=gpu_scene.cache[i].valid;
        assert(valid==1);renderer_finish();
    }
    // Uploaded bitmap and 24-bit movie pixels are sampled across the entire
    // display, with no font/HUD exceptions and with VRAM address wrapping.
    for(unsigned i=0;i<1024u*512u;++i) memory[i]=(uint16_t)i;
    for(unsigned s=1;s<=3;++s) for(unsigned x=0;x<640;++x) {
        const unsigned mask=(1u<<s)-1u;
        const unsigned o=511u*1024u+1000u;
        const unsigned addr=(((511u&~mask)*1024u)+((1000u+x)&~mask))&524287u;
        assert(gpuSceneDisplayPixel(memory,o,x,false,s)==memory[addr]);
        const uint16_t a=gpuSceneDisplayPixel(memory,o,x,true,s);
        assert(a==gpuSceneDisplayPixel(memory,o,x&~mask,true,s));
    }
    puts("PASS whole-scene dispatch: polygons, lines, tiles, sprites, HUD, uploads and scanout");
}
