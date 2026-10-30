# Exact GTE accumulator bounds: fewer common-path branches

RTPS, RTPT, MVMVA and other GTE operations repeatedly check a full 64-bit intermediate against signed 32-bit limits. The existing ordered comparisons produce several branches in the official SDK's MIPS code, including the common case without overflow.

For these limits, a value fits exactly when it equals the sign extension of its low 32-bit word. The new path checks that equality and, on overflow, selects the original positive or negative flag from the sign of the full value. It returns the **unchanged 64-bit value**. It does not narrow the returned accumulator or remove exceptional flags. Other bounds use the existing ordered-comparison fallback. The supported GCC targets use the existing project's signed narrowing behavior; actual MIPS execution is checked.

The new path is enabled in RetroFW builds by default; `make GTE_SIGNED32_BOUNDS=0` restores the old implementation for a clean comparison. Clean/rebuild when changing this flag. There is no new gameplay option and no graphics-quality reduction. No CPU opcode timing, delay slot, memory mapping, code invalidation, firmware or bootloader logic is changed.

## Validation

`bash tests/run_gte_bounds_tests.sh` checks the production helper, returned 64-bit value, all flag combinations used by A1/A2/A3/F, signed-32 edges, signed-64 extremes, three million seeded inputs and custom-limit fallback. Host helper tests use ASan/UBSan.

The complete-state test compiles the actual production GTE implementation twice, under distinct function names. It compares complete register state after 10,000 cases for each of 22 GTE arithmetic commands (220,000 comparisons), including random states, projection matrices, zero states and extreme matrix/vector values. Set `CXX_MIPS` to the official SDK compiler to run the helper and complete-state comparisons as MIPS machine code under QEMU as well. This is equivalence with the existing implementation for tested states, not a claim of universal PS1 conformance. Existing signed-shift behavior elsewhere in the GTE is not changed.

## Same-executable local game comparison

The disposable replay executable contains original and optimized helpers; the JIT selects a fixed call target when compiling a block. The production OPK contains no runtime selector or replay hooks. Both variants use original graphics quality, HLE BIOS, NTSC, rec multiplier 2.0 and frameskip off, without a frame limit. Each run loads the same local state, discards 500 output frames, and measures the following 1,500. Order: original / optimized / optimized / original / original / optimized.

| Scene | Original CPU seconds | Optimized CPU seconds | Median change |
| --- | --- | --- | --- |
| Gran Turismo, stationary car on High Speed Ring | 17.42 / 17.19 / 17.19 | 16.16 / 16.29 / 16.29 | 5.2% less CPU time |
| Digimon World, interior/dialogue | 14.33 / 14.50 / 14.31 | 13.75 / 13.71 / 13.83 | 4.0% less CPU time |

All 24 captured checkpoints per game match exactly across variants and repetitions. These are QEMU CPU-time measurements on the development PC; they do not measure RS-97 FPS or establish compatibility with other games/scenes. The user's previous no-FPS-gain result concerned earlier optimizations and quality controls, not this new change. Hardware FPS remains unmeasured.
