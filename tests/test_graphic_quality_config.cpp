#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include "../src/psxcommon.h"
#include "../src/gpu/gpu_unai/gpu.h"

PcsxConfig Config;
gpu_unai_config_t gpu_unai_config_ext;
u32 cycle_multiplier;
extern void config_save();
extern void config_load();

int main()
{
    mkdir(".pcsx4all",0777);
    for (int value=0;value<4;++value) {
        gpu_unai_config_ext.texture_size=value;
        gpu_unai_config_ext.gradient_quality=3-value;
        gpu_unai_config_ext.vertex_grid=value;
        gpu_unai_config_ext.small_polygons=3-value;
        gpu_unai_config_ext.internal_resolution=value;
        config_save();
        memset(&gpu_unai_config_ext,0,sizeof gpu_unai_config_ext);
        config_load();
        if (gpu_unai_config_ext.texture_size!=value ||
            gpu_unai_config_ext.gradient_quality!=3-value ||
            gpu_unai_config_ext.vertex_grid!=value ||
            gpu_unai_config_ext.small_polygons!=3-value ||
            gpu_unai_config_ext.internal_resolution!=value) {
            printf("FAIL quality config roundtrip %d\n",value); return 1;
        }
    }
    FILE *f=fopen(".pcsx4all/pcsx4all.retrofw.cfg","w");
    if (!f) return puts("FAIL create config"),1;
    fputs("CONFIG_VERSION 0\ntexture_size -1\ngradient_quality 9\nvertex_grid invalid\nsmall_polygons 99\ninternal_resolution 4\n",f);
    fclose(f);
    memset(&gpu_unai_config_ext,0xff,sizeof gpu_unai_config_ext);
    config_load();
    if (gpu_unai_config_ext.texture_size || gpu_unai_config_ext.gradient_quality ||
        gpu_unai_config_ext.vertex_grid || gpu_unai_config_ext.small_polygons || gpu_unai_config_ext.internal_resolution)
        return puts("FAIL malformed quality config"),1;
    puts("PASS quality configuration persistence and invalid value fallback");
    return 0;
}
