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
