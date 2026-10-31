/* Whole-scene dispatch: deliberately includes sprites, text and HUD geometry. */
static bool gpuScenePolygon(PtrUnion packet,unsigned flags,bool quad,PolyType type)
{
    if (!gpuSceneBegin(flags)) return false;
    PP driver=gpuPolySpanDrivers[flags&~1024u];
#ifdef GPU_SCENE_TEST_COUNTERS
    gpu_scene.driver_flags=flags&~1024u;driver=gpuSceneCountedSpan;
#endif
    if (type==POLYTYPE_F) gpuDrawPolyF(packet,driver,quad);
    else if (type==POLYTYPE_FT) gpuDrawPolyFT(packet,driver,quad,true);
    else if (type==POLYTYPE_G) gpuDrawPolyG(packet,driver,quad,true);
    else gpuDrawPolyGT(packet,driver,quad,true);
    gpuSceneEnd();return true;
}

static void gpuSceneLine(PtrUnion packet,PSD driver,bool gouraud)
{
    const bool reduced=gpuSceneBegin(0);
    if (gouraud) gpuDrawLineG(packet,driver);
    else gpuDrawLineF(packet,driver);
    if (reduced) gpuSceneEnd();
}

static void gpuSceneTile(PtrUnion packet,PT driver)
{
    const bool reduced=gpuSceneBegin(0);
    gpuDrawT(packet,driver);
    if (reduced) gpuSceneEnd();
}

static void gpuSceneSprite(PtrUnion packet,unsigned flags)
{
    const unsigned poly_flags=(flags&127u)|((flags&128u)<<1);
    if (!gpuSceneBegin(poly_flags)) {
        gpuDrawS(packet,gpuSpriteSpanDrivers[flags]);return;
    }
    const unsigned s=gpu_lowres.shift, scale=1u<<s;
    const int nx=GPU_EXPANDSIGN(packet.S2[2]+gpu_unai.DrawingOffset[0]);
    const int ny=GPU_EXPANDSIGN(packet.S2[3]+gpu_unai.DrawingOffset[1]);
    const unsigned w=packet.U2[6]&1023u,h=packet.U2[7]&511u;
    int x=Max2(nx>>s,(int)gpu_unai.DrawingArea[0]);
    int y=Max2(ny>>s,(int)gpu_unai.DrawingArea[1]);
    const int right=Min2((nx+(int)w+(int)scale-1)>>s,(int)gpu_unai.DrawingArea[2]);
    const int bottom=Min2((ny+(int)h+(int)scale-1)>>s,(int)gpu_unai.DrawingArea[3]);
    gpu_unai.r5=packet.U1[0]>>3;gpu_unai.g5=packet.U1[1]>>3;gpu_unai.b5=packet.U1[2]>>3;
    const unsigned u0=packet.U1[8]+Max2(x*(int)scale-nx,0);
    unsigned v0=packet.U1[9]+Max2(y*(int)scale-ny,0);
    gpu_unai.u_inc=scale<<FIXED_BITS;gpu_unai.v_inc=0;
    PP driver=gpuPolySpanDrivers[poly_flags];
#ifdef GPU_SCENE_TEST_COUNTERS
    gpu_scene.driver_flags=poly_flags;driver=gpuSceneCountedSpan;
#endif
    if (w && h && right>x) for (;y<bottom;++y,v0+=scale) {
        gpu_unai.u=u0<<FIXED_BITS;gpu_unai.v=v0<<FIXED_BITS;
        driver(gpu_unai,gpu_unai.vram+y*1024+x,right-x);
    }
    gpuSceneEnd();
}
