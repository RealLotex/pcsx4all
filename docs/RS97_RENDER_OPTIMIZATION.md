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

## Device-safety review and rollback

This source patch changes only GPU frame conversion and opaque tile stores:
it adds no bootloader, kernel, MTD, partition, or firmware write operations.
The existing emulator does persist user configuration, virtual memory card
data and save states; those can be damaged by an unexpected crash. Keep backups.

A permanently unbootable RS-97 is not an expected consequence of simply
running this user-space OPK, but **hardware safety cannot be guaranteed**
without building and testing the exact binary on the target device.
Crashes, visual corruption, freezes and save-file damage remain possible.

Before first use:
1. Copy the **original working OPK** to your computer, outside the console's
   scanned `opk` directory. Back up `~/.pcsx4all/memcards` and save states.
2. Install the test OPK as an **application only**. Never flash a system image
   or overwrite boot/firmware partitions to try this renderer patch.
3. Test with an expendable save and several games. If it hangs, restart the
   console and restore the original OPK. Avoid writing saves while testing.
4. The `RS-97 renderer regression tests` GitHub workflow runs host-side
   equality and sanitizer checks; it does not prove full MIPS binary
   compatibility or an FPS gain on the real device.

## Compatibility / render hotpath improvements (second pass)

- `src/gpu/gpulib/vram_scanline.h` is called by the 256/320/368/384/512/640
  video blitters. Normally it returns a direct VRAM pointer with no copy.
  If a 15-bit or 24-bit display scanline crosses the final VRAM address,
  it copies only that rare scanline into a 32-bit-aligned scratch buffer.
  This avoids out-of-bounds VRAM reads and handles the linear wrap.
- `src/gpu/gpu_unai/gpu_opaque_fill.h` centralizes the already-used packed
  opaque span fill. Flat-shaded **untextured, unblended, unmasked** polygons
  now use that same faster path instead of a scalar write for every pixel.
  Textured pixels, semitransparent polygons, dithering and mask checks
  stay in the original rendering paths.
- `tests/test_rs97_hotpaths.cpp` includes these **actual production headers**
  and verifies 0..512-pixel fills (both alignments), all six output widths,
  15/24-bit data, VRAM wrapping and guard words. C++03 builds and
  AddressSanitizer/UndefinedBehaviorSanitizer checks run in GitHub Actions.

**Limitations:** These tests protect only their exercised memory and color
routines. They cannot detect all compatibility problems or firmware issues.
Full RetroFW cross-compilation and real RS-97 V3.0 testing remain necessary
before claiming an FPS improvement or a fully validated OPK.
