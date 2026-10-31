# Experimental whole-scene resolution

Build with `INTERNAL_RESOLUTION=1 FULL_SCENE=1` (clean build when changing
flags). The graphics menu keeps **Internal resolution: original / half /
quarter / eighth**. Original is the default. Each step divides both render
dimensions: a 320x240 scene becomes 160x120, 80x60 or 40x30. Text and HUD are
included, as requested; there is no UI-preservation heuristic in this mode.

The previous experiment reduced selected polygon sampling and immediately
expanded every fragment into native VRAM. Sprites, rectangles and common UI
polygons bypassed it. That implementation did not provide whole-scene internal
resolution. It remains available only when `FULL_SCENE=0` for comparison.

The new implementation redirects all polygon, line, rectangle and sprite
families to a separate smaller render surface. Texture/CLUT storage remains
native. Destination reads, blending and writes happen at reduced resolution.
Scanout reads the small surface directly and enlarges it for the RS-97 screen.
Two nonoverlapping render areas are retained across framebuffer flips. Native
VRAM is resolved when a readback, framebuffer texture dependency, copy, cache
eviction or save state requires it. Framebuffer clears also fill the small
surface directly. Uploads invalidate only overlapping surfaces. Loading a state
discards both old surfaces before restoring VRAM. Per-frame screen metadata
queries do not trigger expansion; screenshot pixel readers do.

In the dispatch regression, eight overlapping 64x64 opaque quads generate
8,192 / 2,048 / 512 destination fragments at half / quarter / eighth, compared
with 32,768 native fragments. Presentation performs zero native VRAM expansion
writes. An explicit readback resolves the drawing area once. These are work
counts, not console FPS measurements.

CPU-uploaded backgrounds and RGB24 movies also display at reduced resolution.
Their upload and movie decoding costs are unchanged. The display sampler for
those images is not the raster optimization. It ensures there are no remaining
full-resolution islands on screen.

This is a deliberately lossy graphics mode: small geometry and letters can
disappear, masks/blending operate on coarse pixels, and intra-primitive
framebuffer feedback is approximate. Original mode retains the native renderer.
The separate texture-size, gradient, vertex-grid and small-polygon experiments
are bypassed by whole-scene polygon drawing; lighting/blending settings still
apply. Extra surface storage is 1 MiB. Allocation failure disables the mode.
No PS1 CPU, dynarec exception or delay-slot behavior is changed here.

Run `bash tests/run_scene_tests.sh` for production command-dispatch, native
re-entry, render-write counts, direct presentation, feedback, mask, upload,
VRAM-copy and state-save/load regressions. The same test runs under QEMU with
the RetroFW compiler in CI. ASan/UBSan excludes shift and signed-overflow checks
because the existing GPU uses signed fixed-point shifts/overflow throughout;
address and other undefined-behavior checks remain enabled.

Actual FPS on RS-97 must be measured on the device. PC/QEMU replay CPU times
cannot be converted into a console FPS claim.
