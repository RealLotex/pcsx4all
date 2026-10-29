#include "test_rs97_lit4_polygon.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#ifdef LIT4_REFERENCE
#define gpu_unai_config_ext lit4_ref_config
#define gpuDrawPolyF lit4_ref_poly_f
#define gpuDrawPolyFT lit4_ref_poly_ft
#define gpuDrawPolyG lit4_ref_poly_g
#define gpuDrawPolyGT lit4_ref_poly_gt
#define TEST_ENTRY lit4_ref
#else
#define gpu_unai_config_ext lit4_opt_config
#define gpuDrawPolyF lit4_opt_poly_f
#define gpuDrawPolyFT lit4_opt_poly_ft
#define gpuDrawPolyG lit4_opt_poly_g
#define gpuDrawPolyGT lit4_opt_poly_gt
#define GPU_UNAI_LIT4_PALETTE_EXPERIMENT
#define TEST_ENTRY lit4_opt
#endif
#define USE_GPULIB
#define INLINE static inline
#define GPU_INLINE static inline __attribute__((always_inline))
#include <gpu_unai.h>
#include <gpu_fixedpoint.h>
#include <gpu_inner.h>
#include <gpu_raster_polygon.h>
extern "C" void TEST_ENTRY(const Lit4Case &c, uint16_t *vram)
{
    static bool initialized;
    if (!initialized) { SetupLightLUT(); SetupDitheringConstants(); initialized = true; }
    gpu_unai.vram = vram; gpu_unai.TBA = vram + c.texture;
    gpu_unai.CBA = vram + c.clut;
    memcpy(gpu_unai.DrawingArea, c.area, sizeof c.area);
    memcpy(gpu_unai.DrawingOffset, c.offset, sizeof c.offset);
    gpu_unai.u_msk = gpu_unai.v_msk = 0x3ffffu;
    gpu_unai.ilace_mask = 0;
    PtrUnion packet; packet.ptr = (void *)c.packet;
    gpuDrawPolyFT(packet, gpuPolySpanDrivers[0x21], c.is_quad);
}
