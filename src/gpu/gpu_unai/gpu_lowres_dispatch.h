/* Experimental dispatch kept separate from the delivered quality-control OPK. */
static bool gpuTryLowResPolygon(PtrUnion packet,unsigned flags,bool quad,PolyType type,unsigned shift)
{
    if (!shift) return false;
    const bool textured=(type&POLYATTR_TEXTURE)!=0;
    const bool gouraud=(type&POLYATTR_GOURAUD)!=0;
    // The renderer has no model or text IDs. Preserve common screen-aligned
    // UI primitives and all sprites; arbitrary transformed text needs review.
    if (textured && gpuTextureLooksLike2D(packet,quad,gouraud)) return false;
    if (!textured && gpuTextureLooksLike2D(packet,quad,false)) return false;
    if (!gpuLowResBegin(shift,flags)) return false;
    const PP driver=gpuLowResDriver(flags,shift);
    if (type==POLYTYPE_FT) gpuDrawPolyFT(packet,driver,quad,true);
    else if (type==POLYTYPE_GT) gpuDrawPolyGT(packet,driver,quad,true);
    else if (type==POLYTYPE_G) gpuDrawPolyG(packet,driver,quad,true);
    else { gpuLowResEnd();return false; }
    gpuLowResEnd();
    return true;
}
