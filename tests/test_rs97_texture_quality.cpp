#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define USE_GPULIB
#define INLINE static inline
#define GPU_INLINE static inline __attribute__((always_inline))
#include <gpu_unai.h>
#include <gpu_fixedpoint.h>
#include <gpu_inner.h>
#include <gpu_raster_polygon.h>

static u16 vram[1024u*512u];
static u32 seed=0x773a419du;
static u32 rnd() { seed^=seed<<13; seed^=seed>>17; seed^=seed<<5; return seed; }

static void reference(u32 mode, u32 group, u16 *dst, u32 count)
{
    const u32 tm=(mode>>5)&3u;
    const bool light=(mode&1u)!=0, gouraud=(mode&0x80u)!=0;
    u32 u=gpu_unai.u & gpu_unai.u_msk, v=gpu_unai.v & gpu_unai.v_msk;
    u32 g=gpu_unai.gCol;
    while (count) {
        const u8 *texture=(const u8 *)gpu_unai.TBA;
        const u32 tu=(u>>10)&~(group-1u), tv=(v>>10)&~(group-1u);
        u16 source;
        if (tm==1) {
            u8 packed=texture[(tv<<11)+(tu>>1)];
            source=gpu_unai.CBA[(packed>>((tu&1u)*4))&15u];
        } else if (tm==2) {
            source=gpu_unai.CBA[texture[(tv<<11)+tu]];
        } else {
            source=gpu_unai.TBA[(tv<<10)+tu];
        }
        u16 out=source;
        if (source && light) {
            out=gouraud ? gpuLightingTXTGouraud(source,g)
                        : gpuLightingTXT(source,gpu_unai.r5,gpu_unai.g5,gpu_unai.b5);
            out|=source&0x8000u;
        }
        const u32 n=count<group ? count : group;
        for (u32 i=0;i<n;++i) {
            if (source) dst[i]=out;
            u=(u+(u32)gpu_unai.u_inc)&gpu_unai.u_msk;
            v=(v+(u32)gpu_unai.v_inc)&gpu_unai.v_msk;
            if (light && gouraud) g+=gpu_unai.gInc;
        }
        dst+=n; count-=n;
    }
}

int main()
{
    SetupLightLUT();
    gpu_unai.vram=vram;
    gpu_unai.TBA=vram+256u*1024u+512u;
    gpu_unai.CBA=vram+500u*1024u;
    gpu_unai.DrawingArea[0]=0; gpu_unai.DrawingArea[1]=0;
    gpu_unai.DrawingArea[2]=320; gpu_unai.DrawingArea[3]=240;
    gpu_unai.u_msk=gpu_unai.v_msk=0x3ffffu;
    for (u32 i=0;i<1024u*512u;++i) vram[i]=(u16)rnd();
    for (u32 i=0;i<256;++i) gpu_unai.CBA[i]=(u16)((rnd()&0x7fffu)|1u);
    const u32 modes[]={0x20,0x21,0xa1,0x40,0x41,0xc1};
    gpu_unai_config_ext.texture_size=0;
    for (u32 i=0;i<6;++i)
        if (gpuChoosePotatoDriver(modes[i])!=gpuPolySpanDrivers[modes[i]]) return puts("FAIL off path"),1;
    for (u32 level=1;level<=2;++level) {
      gpu_unai_config_ext.texture_size=level;
      for (u32 i=0;i<6;++i) {
        const u32 mode=modes[i];
        if (gpuChoosePotatoDriver(mode)==gpuPolySpanDrivers[mode]) return puts("FAIL eligible selection"),1;
        for (u32 trial=0;trial<2500;++trial) {
            u16 expected[80], actual[80];
            for (u32 j=0;j<80;++j) expected[j]=actual[j]=(u16)rnd();
            gpu_unai.u=rnd(); gpu_unai.v=rnd();
            gpu_unai.u_msk=(rnd()&1u) ? 0x3ffffu : 0x1ffffu;
            gpu_unai.v_msk=(rnd()&1u) ? 0x3ffffu : 0x0ffffu;
            gpu_unai.u_inc=(s32)rnd(); gpu_unai.v_inc=(s32)rnd();
            gpu_unai.gCol=rnd(); gpu_unai.gInc=rnd();
            gpu_unai.r5=rnd()&31u; gpu_unai.g5=rnd()&31u; gpu_unai.b5=rnd()&31u;
            const u32 offset=1u+(rnd()&1u), count=rnd()%65u;
            reference(mode,1u<<level,expected+offset,count);
            gpuChoosePotatoDriver(mode)(gpu_unai,actual+offset,count);
            if (memcmp(expected,actual,sizeof expected)) {
                printf("FAIL mode=%x trial=%u count=%u alignment=%u\n",mode,trial,count,offset); return 1;
            }
        }
      }
      gpu_unai.CBA[3]=0;
      if (gpuChoosePotatoDriver(0x21)!=gpuPolySpanDrivers[0x21]) return puts("FAIL transparent glyph fallback"),1;
      gpu_unai.CBA[3]=0x8000;
      if (gpuChoosePotatoDriver(0x21)!=gpuPolySpanDrivers[0x21]) return puts("FAIL masked glyph fallback"),1;
      gpu_unai.CBA[3]=1;
      if (gpuChoosePotatoDriver(0x60)!=gpuPolySpanDrivers[0x60]) return puts("FAIL direct-color glyph fallback"),1;
      gpu_unai.CBA=vram+500u*1024u+1u;
      for (u32 j=0;j<16;++j) gpu_unai.CBA[j]=(u16)(j+1u);
      if (gpuChoosePotatoDriver(0x21)==gpuPolySpanDrivers[0x21]) return puts("FAIL unaligned palette"),1;
      gpu_unai.CBA[4]=0;
      if (gpuChoosePotatoDriver(0x21)!=gpuPolySpanDrivers[0x21]) return puts("FAIL unaligned alpha fallback"),1;
      gpu_unai.CBA=vram+500u*1024u;
      for (u32 j=0;j<256;++j) gpu_unai.CBA[j]=(u16)(j+1u);
    }
    const u32 unsupported[]={0x22,0x24,0x28,0x120,0x220,0x420,0x80};
    for (u32 i=0;i<sizeof unsupported/sizeof unsupported[0];++i)
        if (gpuChoosePotatoDriver(unsupported[i])!=gpuPolySpanDrivers[unsupported[i]]) return puts("FAIL effect fallback"),1;
    gpu_unai.TBA=vram+128u;
    if (gpuChoosePotatoDriver(0x21)!=gpuPolySpanDrivers[0x21]) return puts("FAIL texture feedback fallback"),1;
    gpu_unai.TBA=vram+256u*1024u+512u; gpu_unai.CBA=vram+128u;
    if (gpuChoosePotatoDriver(0x21)!=gpuPolySpanDrivers[0x21]) return puts("FAIL CLUT feedback fallback"),1;
    gpu_unai.CBA=vram+500u*1024u; gpu_unai.u_msk=0x157ffu;
    if (gpuChoosePotatoDriver(0x21)!=gpuPolySpanDrivers[0x21]) return puts("FAIL non-contiguous window fallback"),1;
    gpu_unai.u_msk=0x3ffffu; gpu_unai_config_ext.texture_size=3;
    for (u32 i=0;i<16;++i) gpu_unai.CBA[i]=(u16)(i+1);
    if (gpuChoosePotatoDriver(0x21)!=gpuPlainSpan) return puts("FAIL plain selection"),1;
    gpu_unai.u_msk=0x157ffu;
    if (gpuChoosePotatoDriver(0x21)!=gpuPlainSpan) return puts("FAIL plain window independence"),1;
    gpu_unai.u_msk=0x3ffffu;
    if (gpuChoosePotatoDriver(0xa0)!=gpuPolySpanDrivers[0xa0]) return puts("FAIL unsupported plain fallback"),1;
    u32 packet_words[12]={0};
    PtrUnion packet; packet.U4=packet_words;
    packet_words[1]=10u|(20u<<16); packet_words[3]=30u|(20u<<16);
    packet_words[5]=10u|(40u<<16); packet_words[7]=30u|(40u<<16);
    if (!gpuTextureLooksLike2D(packet,true,false) ||
        gpuChooseTexturePolygonDriver(0x21,packet,true,false)!=gpuPolySpanDrivers[0x21] ||
        !gpuTextureLooksLike2D(packet,false,false)) return puts("FAIL 2D text protection"),1;
    packet_words[5]=15u|(40u<<16);
    if (gpuTextureLooksLike2D(packet,true,false)) return puts("FAIL 3D polygon classification"),1;
    gpu_unai_config_ext.texture_size=1; gpu_unai_config_ext.vertex_grid=3;
    gpu_unai_config_ext.small_polygons=3; gpu_unai.CBA[3]=0;
    bool protected_polygon;
    if (gpuChooseTexturePolygonDriver(0x21,packet,true,false,&protected_polygon)!=gpuPolySpanDrivers[0x21] ||
        !protected_polygon) return puts("FAIL transformed transparent glyph protection"),1;
    PolyVertex protected_vertices[4];
    polyInitVertexBuffer(protected_vertices,packet,POLYTYPE_FT,1,protected_polygon);
    if (protected_vertices[0].x!=10 || protected_vertices[2].x!=15)
        return puts("FAIL transformed glyph coordinates"),1;
    gpu_unai.CBA[3]=1; gpu_unai_config_ext.texture_size=3;
    gpu_unai_config_ext.vertex_grid=0; gpu_unai_config_ext.small_polygons=0;
    for (u32 level=1;level<=3;++level) {
        gpu_unai_config_ext.gradient_quality=level;
        PP driver=gpuChooseGradientPolygonDriver(0x81,packet,true);
        if (driver==gpuPolySpanDrivers[0x81]) return puts("FAIL gradient selection"),1;
        for (u32 trial=0;trial<2500;++trial) {
            u16 expected[80],actual[80];
            for (u32 j=0;j<80;++j) expected[j]=actual[j]=(u16)rnd();
            gpu_unai.gCol=rnd(); gpu_unai.gInc=rnd();
            gpu_unai.r5=rnd()&31u; gpu_unai.g5=rnd()&31u; gpu_unai.b5=rnd()&31u;
            const u32 offset=1u+(rnd()&1u), count=rnd()%65u;
            u32 color=gpu_unai.gCol;
            for (u32 j=0;j<count;) {
                const u32 group=level==1 ? 2u : 4u;
                const u32 n=(count-j)<group ? (count-j) : group;
                const u16 out=level==3 ? (u16)(gpu_unai.r5|(gpu_unai.g5<<5)|(gpu_unai.b5<<10)) : gpuLightingRGB(color);
                for (u32 k=0;k<n;++k) expected[offset+j+k]=out;
                color+=gpu_unai.gInc*n; j+=n;
            }
            driver(gpu_unai,actual+offset,count);
            if (memcmp(expected,actual,sizeof expected)) return puts("FAIL gradient span"),1;
        }
        bool protected_effect=false;
        if (gpuChooseGradientPolygonDriver(0x83,packet,true,&protected_effect)!=gpuPolySpanDrivers[0x83] || !protected_effect)
            return puts("FAIL gradient effects fallback"),1;
    }
    packet_words[5]=10u|(40u<<16);
    if (gpuChooseGradientPolygonDriver(0x81,packet,true)!=gpuPolySpanDrivers[0x81])
        return puts("FAIL gradient 2D protection"),1;
    PolyVertex vertices[4];
    gpu_unai_config_ext.vertex_grid=2;
    gpu_unai.DrawingOffset[0]=0; gpu_unai.DrawingOffset[1]=0;
    polyInitVertexBuffer(vertices,packet,POLYTYPE_FT,1);
    if (vertices[0].x!=10 || vertices[1].x!=30 || vertices[2].x!=10)
        return puts("FAIL 2D vertex protection"),1;
    packet_words[5]=15u|(40u<<16);
    polyInitVertexBuffer(vertices,packet,POLYTYPE_FT,1);
    if (vertices[0].x!=12 || vertices[1].x!=32 || vertices[2].x!=16)
        return puts("FAIL vertex grid"),1;
    gpu_unai_config_ext.vertex_grid=0;
    polyInitVertexBuffer(vertices,packet,POLYTYPE_FT,1);
    if (vertices[0].x!=10 || vertices[1].x!=30 || vertices[2].x!=15)
        return puts("FAIL original vertex coordinates"),1;
    vertices[0].x=10; vertices[0].y=10;
    vertices[1].x=12; vertices[1].y=10;
    vertices[2].x=11; vertices[2].y=12;
    const PolyVertex *ordered[3];
    if (!polyUseTriangle(vertices,0,ordered) || !polyUseTriangle(vertices,0,ordered,2) ||
        polyUseTriangle(vertices,0,ordered,4)) return puts("FAIL small polygon threshold"),1;
    vertices[1].x=20;
    if (!polyUseTriangle(vertices,0,ordered,4) || polyUseTriangle(vertices,0,ordered,16))
        return puts("FAIL small polygon size"),1;
    // Character sprites use their existing span drivers at every quality level.
    u8 sprite_texture[128]; u16 sprite_ref[66], sprite_out[66];
    for (u32 i=0;i<128;++i) sprite_texture[i]=(u8)rnd();
    for (u32 i=0;i<66;++i) sprite_ref[i]=sprite_out[i]=(u16)rnd();
    gpu_unai.TextureWindow[2]=255;
    gpu_unai_config_ext.texture_size=0; gpu_unai_config_ext.gradient_quality=0;
    gpu_unai_config_ext.vertex_grid=0; gpu_unai_config_ext.small_polygons=0;
    gpuSpriteSpanDrivers[0x20](sprite_ref+1,64,sprite_texture,0);
    gpu_unai_config_ext.texture_size=3; gpu_unai_config_ext.gradient_quality=3;
    gpu_unai_config_ext.vertex_grid=3; gpu_unai_config_ext.small_polygons=3;
    gpuSpriteSpanDrivers[0x20](sprite_out+1,64,sprite_texture,0);
    if (memcmp(sprite_ref,sprite_out,sizeof sprite_ref)) return puts("FAIL text sprite unchanged"),1;

    // A common axis-aligned textured glyph quad must also remain pixel-exact.
    static u16 original_vram[1024u*512u], expected_vram[1024u*512u];
    for (u32 i=0;i<1024u*512u;++i) vram[i]=(u16)rnd();
    memcpy(original_vram,vram,sizeof vram);
    memset(packet_words,0,sizeof packet_words);
    packet_words[0]=0x00808080u;
    packet_words[1]=10u|(20u<<16); packet_words[2]=0u;
    packet_words[3]=30u|(20u<<16); packet_words[4]=20u;
    packet_words[5]=10u|(40u<<16); packet_words[6]=20u<<8;
    packet_words[7]=30u|(40u<<16); packet_words[8]=20u|(20u<<8);
    gpu_unai.TBA=vram+256u*1024u+512u; gpu_unai.CBA=vram+500u*1024u;
    gpu_unai.u_msk=gpu_unai.v_msk=0x3ffffu;
    gpu_unai.ilace_mask=0; gpu_unai.prog_ilace_flag=0;
    gpu_unai_config_ext.texture_size=0; gpu_unai_config_ext.gradient_quality=0;
    gpu_unai_config_ext.vertex_grid=0; gpu_unai_config_ext.small_polygons=0;
    PP glyph_driver=gpuChooseTexturePolygonDriver(0x21,packet,true,false,&protected_polygon);
    gpuDrawPolyFT(packet,glyph_driver,true,protected_polygon);
    memcpy(expected_vram,vram,sizeof vram);
    memcpy(vram,original_vram,sizeof vram);
    gpu_unai_config_ext.texture_size=3; gpu_unai_config_ext.gradient_quality=3;
    gpu_unai_config_ext.vertex_grid=3; gpu_unai_config_ext.small_polygons=3;
    glyph_driver=gpuChooseTexturePolygonDriver(0x21,packet,true,false,&protected_polygon);
    if (!protected_polygon) return puts("FAIL shared 2D classification"),1;
    gpuDrawPolyFT(packet,glyph_driver,true,protected_polygon);
    if (memcmp(expected_vram,vram,sizeof vram)) return puts("FAIL axis-aligned glyph unchanged"),1;
    for (u32 i=0;i<16;++i) gpu_unai.CBA[i]=(u16)(i+1);
    gpu_unai.r5=3; gpu_unai.g5=7; gpu_unai.b5=11;
    u16 plain[10]; for (u32 i=0;i<10;++i) plain[i]=0x7777;
    gpuChoosePotatoDriver(0x21)(gpu_unai,plain+1,8);
    if (plain[0]!=0x7777 || plain[9]!=0x7777) return puts("FAIL plain boundary"),1;
    for (u32 i=1;i<9;++i) if (plain[i]!=0x4210u) return puts("FAIL plain color"),1;
    gpu_unai.CBA[3]=0;
    if (gpuChoosePotatoDriver(0x21)!=gpuPolySpanDrivers[0x21]) return puts("FAIL transparent plain fallback"),1;
    gpu_unai.CBA[3]=0x8000;
    if (gpuChoosePotatoDriver(0x21)!=gpuPolySpanDrivers[0x21]) return puts("FAIL MSB plain fallback"),1;
    if (gpuChoosePotatoDriver(0x60)!=gpuPolySpanDrivers[0x60]) return puts("FAIL direct-color plain fallback"),1;

    // Production polygon code must fill an eligible non-rectangular 3D
    // primitive with the selected uniform color, without touching the HUD.
    memset(vram,0,sizeof vram);
    for (u32 i=0;i<16;++i) gpu_unai.CBA[i]=(u16)(i+1);
    memset(packet_words,0,sizeof packet_words);
    packet_words[0]=0x00c08020u;
    packet_words[1]=10u|(10u<<16); packet_words[2]=0;
    packet_words[3]=30u|(10u<<16); packet_words[4]=20u;
    packet_words[5]=15u|(30u<<16); packet_words[6]=20u<<8;
    gpu_unai_config_ext.texture_size=3; gpu_unai_config_ext.small_polygons=0;
    gpu_unai_config_ext.vertex_grid=0;
    PP flat_texture=gpuChooseTexturePolygonDriver(0x21,packet,false,false,&protected_polygon);
    if (flat_texture!=gpuPlainSpan) return puts("FAIL 3D plain driver"),1;
    if (protected_polygon) return puts("FAIL shared 3D classification"),1;
    gpuDrawPolyFT(packet,flat_texture,false,protected_polygon);
    unsigned pixels=0;
    for (u32 y=10;y<30;++y) for (u32 x=10;x<30;++x) {
        const u16 pixel=vram[y*1024u+x];
        if (pixel) { if (pixel!=0x4210u) return puts("FAIL 3D plain polygon color"),1; ++pixels; }
    }
    if (!pixels) return puts("FAIL 3D plain polygon empty"),1;

    // Small-polygon filtering skips an eligible tiny triangle only when set.
    for (u32 y=10;y<30;++y) for (u32 x=10;x<30;++x) vram[y*1024u+x]=0;
    packet_words[3]=13u|(10u<<16); packet_words[5]=11u|(13u<<16);
    flat_texture=gpuChooseTexturePolygonDriver(0x21,packet,false,false,&protected_polygon);
    gpu_unai_config_ext.small_polygons=0;
    gpuDrawPolyFT(packet,flat_texture,false,protected_polygon);
    pixels=0; for (u32 y=10;y<14;++y) for (u32 x=10;x<14;++x) pixels+=vram[y*1024u+x]!=0;
    if (!pixels) return puts("FAIL tiny polygon original"),1;
    for (u32 y=10;y<14;++y) for (u32 x=10;x<14;++x) vram[y*1024u+x]=0;
    gpu_unai_config_ext.small_polygons=1;
    gpuDrawPolyFT(packet,flat_texture,false,protected_polygon);
    for (u32 y=10;y<14;++y) for (u32 x=10;x<14;++x)
        if (vram[y*1024u+x]) return puts("FAIL tiny polygon cull"),1;
    gpu_unai_config_ext.small_polygons=0;

    memset(vram,0,sizeof vram);
    packet_words[0]=0x00c08020u;
    packet_words[1]=10u|(10u<<16); packet_words[2]=0x00ffffffu;
    packet_words[3]=30u|(10u<<16); packet_words[4]=0x00000000u;
    packet_words[5]=15u|(30u<<16);
    gpu_unai_config_ext.gradient_quality=3;
    PP flat_gradient=gpuChooseGradientPolygonDriver(0x81,packet,false,&protected_polygon);
    if (flat_gradient!=gpuFlatGradientSpan) return puts("FAIL 3D flat gradient driver"),1;
    if (protected_polygon) return puts("FAIL shared gradient classification"),1;
    gpuDrawPolyG(packet,flat_gradient,false,protected_polygon);
    pixels=0;
    const u16 first_color=(u16)(4u|(16u<<5)|(24u<<10));
    for (u32 y=10;y<30;++y) for (u32 x=10;x<30;++x) {
        const u16 pixel=vram[y*1024u+x];
        if (pixel) { if (pixel!=first_color) return puts("FAIL 3D flat gradient color"),1; ++pixels; }
    }
    if (!pixels) return puts("FAIL 3D flat gradient empty"),1;
    puts("PASS texture levels, transparency, MSB, wrap, alignment, plain and feedback fallback");
    return 0;
}
