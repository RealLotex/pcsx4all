# RS-97 framebuffer optimization (experimental)

This change optimizes actual rendering code, **not** audio, frameskip, or console clock settings.

## What changed
- `src/gpu/gpulib/vout_port.cpp`: paired BGR555 -> RGB565 conversion and aligned 32-bit output stores for 256/384/512/640 horizontal modes. The 384/512/640 fast paths run only with Pixel Skip enabled. Misaligned destinations, RGB24 and Pixel Skip disabled keep the old path.
- `src/gpu/gpu_unai/gpu_inner.h`: paired aligned 32-bit stores for opaque, non-blended, no-mask-check tile spans. Other tile operations retain the original implementation.

## Local regression tests
```sh
g++ -std=c++11 -O2 -Wall -Wextra tests/test_rs97_render.cpp -o /tmp/test_rs97_render
/tmp/test_rs97_render

g++ -std=c++11 -O1 -g -fsanitize=address,undefined -fno-omit-frame-pointer tests/test_rs97_render.cpp -o /tmp/test_rs97_render_san
/tmp/test_rs97_render_san
```
The standalone test compares all 65536 input colors, 1500 random scanlines for each width (256, 384, 512, 640), and tile fills (sizes 1–159, aligned and offset by 2 bytes). These are representative operations; they do **not** run the complete emulator.

## Building for RetroFW
Use a supported RetroFW MIPS toolchain and SDK, with `mksquashfs`, then run:
```sh
make clean
make opk
```
Expected output: `pcsx4all/pcsx4all.opk`. Save your existing OPK first.

## Important limitations
These code paths passed host-side comparisons. The emulator has **not** been compiled with the RetroFW SDK in this environment, nor tested on RS-97 hardware. No end-to-end FPS gain is claimed. If the main bottleneck is MIPS dynarec or polygon rasterization, FPS improvement may be modest or absent.

Compare original/modified OPKs using the same clock, saved scene, GPU settings and Show FPS. Inspect sprites, effects and menus for rendering regressions.
