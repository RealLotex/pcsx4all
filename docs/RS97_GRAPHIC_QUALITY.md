# Optional 3D quality controls

## Hardware result

On 2026-10-30, the user tested the delivered RetroFW OPK and reported that the new controls work but provide no FPS improvement. The report did not include a per-setting numerical breakdown. Keep the controls experimental and disabled by default; they are not a demonstrated performance improvement.

The tested file was `pcsx4all-quality-experimental.opk`, SHA-256 `3439c9b7aae0d961822bf4fe8581e7397924053876ac751b76a15165ece5dada`. Earlier QEMU timings used different revisions and safeguards and must not be presented as gains on this device or as measurements of this exact delivered build.

## Controls

The GPU settings menu offers four independent controls. Their default values keep the original renderer. Change a value with left/right; the change takes effect when the game resumes and is saved in the existing configuration file.

| Menu item | Values | Effect |
| --- | --- | --- |
| Texture size | Original / Half / Quarter / Plain | Coarser samples on eligible opaque textured 3D polygons. Plain uses a uniform neutral gray instead of sampling their textures. |
| Gradient quality | Original / Half / Quarter / Flat | Coarser color gradients on eligible untextured, Gouraud-shaded polygons; Flat uses the first vertex color. |
| Vertex grid | Original / 2px / 4px / 8px | Snaps eligible textured 3D polygon vertices to a progressively coarser screen grid. |
| Small polygons | Keep / <4px / <8px / <16px | Omits eligible triangles whose width and height are both smaller than the selected threshold. |

These settings are independent of frame skip, interlace, dithering, fast lighting, blending and pixel skip. They change visual detail and may improve or worsen speed depending on the game and scene. They do not implement a different framebuffer resolution or create additional polygons.

To protect common text and HUD rendering, sprites and axis-aligned rectangular textured polygons (including their individual right triangles) use the normal path. Blended, masked and dithered texture/gradient spans also use the normal path. Texture approximation avoids texture-page and CLUT feedback, and unsupported texture-window masks fall back. All reduced-texture levels are restricted to indexed palettes with no transparent or mask-bit colors; direct-color and other uncertain cases keep their textures.

Some games draw text as arbitrary transformed polygons, which the PS1 GPU does not identify as text. The protection cannot guarantee unchanged typography in every game. Vertex snapping or removing small polygons can also remove small scene details. Restore defaults or set every control to Original/Keep if a game has graphical problems.

These controls do not modify the firmware or bootloader. Any performance result from a PC or MIPS emulation is not a measurement of FPS on the RS-97.
