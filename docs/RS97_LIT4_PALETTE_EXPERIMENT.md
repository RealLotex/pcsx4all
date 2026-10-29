# Experimental flat-lit 4bpp palette

This is an opt-in experiment: build with
`GPU_UNAI_LIT4_PALETTE_EXPERIMENT` to enable it. Normal builds retain the
existing renderer. Do not infer RS-97 FPS from the measurements below.

The production span driver for an opaque, flat-lit, 4bpp polygon performs
three lighting-table lookups per nontransparent texel. The experiment lights
the 16 palette entries once per polygon and preserves transparency and source
bit 15; texture reads and destination writes remain sequential. It selects only
the original 0x21 driver and excludes blending, dithering, masks and pixel
skipping by that driver selection.

The polygon's entire clipping area must be disjoint from the palette. Otherwise
the original driver runs, because a pixel write might affect a future palette
read. This deliberately conservative range can decline safe polygons. A fresh
palette is built for every accepted polygon, so changes between primitives are
observed. The renderer currently executes primitives synchronously; this
experiment's per-translation-unit palette is not safe for concurrent polygon
rendering without making it per invocation.

In an instrumented x86 interpreter run with HLE on Gran Turismo NTSC
SCUS94194, an automated input sequence reached High Speed Ring with the car
stationary. In frames 3000–4000, 62,950,697 pixels went through the candidate,
across 505,576 palette preparations. The same tracked run measured about
224 million polygon pixels per 1000 frames, including other modes. This is one
scene and the instrumentation changes costs.

A synthetic workload of 80,000 polygons, 14 spans each, 9 pixels per span,
was chosen to approximate that observed average. A MIPS cross-compiled test
under QEMU measured the optimized routine approximately 1.33–1.42 times as
fast as the original; it is a microbenchmark, not a cycle-accurate JZ4760
measurement. The whole instrumented x86 game comparison did not establish
a speedup: base 7.263/7.299 seconds versus candidate 7.282/7.724 seconds for
frames 4000–5000; compilation overlapped one candidate run. Screenshots at
frames 3000, 4000 and 5000 matched in those runs. These tests do not establish
an FPS increase on hardware or correctness for every game.

`tests/test_rs97_lit4_palette.cpp` compares 6,000 randomized spans with the
production driver, including zero, black-lit and high-bit texels, varying
texture windows, negative increments and texture feedback. The paired
translation-unit polygon test compares 2,500 triangles and quadrilaterals and
full VRAM, including palette overlap fallback. The CI workflow runs both with
sanitizers on host and with the actual RetroFW compiler and QEMU. A full
experimental RetroFW OPK is cross-built. Further workloads, full-game
performance analysis, more hardware coverage and ABI review are still needed.

## MIPS dynarec game replay under QEMU

A separate scratch-only build was patched to decode an unaligned ISO directory
field safely and to mark the JIT buffer executable under Linux/QEMU. With HLE,
the default 2.0 cycle multiplier, frameskip off and the speed limiter disabled,
it ran Gran Turismo through High Speed Ring using the real MIPS dynarec. The
scratch baseline and candidate use the same code apart from the palette flag;
both also include the IRGB correction from the separate dynarec experiment.

For output frames 3000–4000 and 4000–5000, respectively, two baseline runs
took 12.895/12.544 and 12.758/12.396 seconds; two candidate runs took
12.531/12.495 and 12.674/12.741 seconds. The runs were alternated and the
candidate was not instrumented for GPU statistics. Screenshots in one
baseline and both candidate runs matched at frames 3000, 4000 and 5000. The
other baseline run differed by about two in-game frames; automated inputs and
frame numbers do not ensure an identical state. These small, variable
differences do not demonstrate an overall improvement.

Saving an automated scratch state succeeded, but subsequent loading stalled
under this QEMU harness even when saving after the event queue had been
processed. No state-based comparison is claimed. The alignment workaround,
JIT permission change and test hooks are not included in the product branch.
