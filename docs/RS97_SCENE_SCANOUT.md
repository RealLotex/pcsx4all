# Grouped reduced-resolution presentation

The previous full-scene renderer already draws at reduced resolution. Its
native-VRAM presentation path nevertheless fetched and converted all 76,800
display pixels, including identical samples. Grouping horizontal samples and
copying repeated rows removes this work without changing the image or reducing
quality further. Original rendering and direct-surface presentation are unchanged.

At Quarter for a 320x240 image, 4,800 source samples are converted and 180
repeated output rows are copied. All 76,800 display pixels are still written.
Source and display buffers are separate and remain stable during presentation.

## Validation

`tests/test_rs97_scene_scanout.cpp` compares against the previous scalar
sampler in 9,216 cases: RGB15/RGB24 input, RGB565/BGR555 output, supported PS1
widths, odd origins and output alignment, single/double row stepping, partial
row groups and VRAM wrapping. Host, full ASan/UBSan and official RetroFW MIPS
tests pass. The broader renderer, configuration and save-state tests remain.

Disposable MIPS/QEMU replay used the same executable for scalar and grouped
paths, Quarter in both, HLE/NTSC, no frameskip or limiter, 500 warmup and 1,500
measured frames. Three runs per path used order 0/1/1/0/0/1; no concurrent local
compilation/emulator work was run during timing. CPU times in seconds:

| Scene | Scalar runs | Grouped runs | Median reduction |
|---|---|---|---|
| Gran Turismo, stationary car on High Speed Ring | 18.24 / 18.22 / 18.31 | 15.38 / 15.12 / 14.91 | 17.1% |
| Silent Hill, first foggy street | 15.61 / 15.42 / 15.59 | 12.69 / 12.64 / 12.48 | 18.9% |

All 48 checkpoint captures match the previous full-scene reference. These
figures compare presentation implementations at the same reduced resolution;
they are not a fresh upstream-OPK comparison. Do not add them to percentages
from earlier experiments. Real RS-97 FPS remains unmeasured.

The runtime selector `GPU_SCENE_SCANOUT_TEST_ENV` is only for the disposable
comparison executable and is absent from production builds. No ROMs, save
states, replay hooks or other rejected optimization prototypes are shipped.
