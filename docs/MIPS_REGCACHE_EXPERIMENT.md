# Register-cache experiment (disabled by default)

The existing allocator frees every unlocked host register when the eight cache
slots are exhausted. The experimental `MIPSREC_SINGLE_SPILL` build flag instead
reuses one unlocked register and chooses the least recently used mapping. The
flag is intentionally not enabled in the RetroFW build: this is a candidate, not
a validated release optimization. No game trace or RS-97 benchmark has established
how frequently this pressure occurs or whether the alternative improves FPS.

`tests/test_mips_regcache_model.cpp` uses the production allocator with modeled
loads, stores, and additions. On the deterministic million-addition model, the
baseline emitted 875394 loads and 917308 stores, and the experimental version
emitted 846147 and 899997. A nine-register pressure trace emitted 16 loads and
16 stores in each version; a naive first-victim policy emitted 2008 of each.
These synthetic traces do not represent a game and the model is not a PS1 CPU.

The separate QEMU MIPS test build executes both the real interpreter and dynarec
using the same initial CPU state and RAM. It tests 200 seeded blocks containing
ADDU/ADDIU, MULTU, MTC2, aligned LW/LB/LH/SW/SB, unaligned LWL/LWR/SWL/SWR,
selected MFC2/LWC2/SWC2 registers and a J delay slot, and compares GPR/HI/LO, PC, CP0, CP2 and 2 MiB RAM. It also
checks a non-delay-slot SYSCALL against the expected exception vector, cause and
EPC, and verifies that a C-side write invalidates the start of an already
compiled block. Both allocator modes are tested with this branch's GTE correction.
These cases do not cover special-effect GTE transfer registers, load-use hazards,
exceptions in delay slots, arbitrary self-modifying code, all instruction values
or real hardware.

The expanded test exposed an existing difference in MTC2 to IRGB: the interpreter
writes only the low 16 bits of IR1–IR3, while the dynarec overwrote all 32 bits.
The branch emits halfword stores, preserving the upper halves as the interpreter
does. An interpreter comparison cannot establish correctness by itself: notably,
the existing interpreter's ADD and ADDI do not raise the overflow exception
mentioned in their comments.

CI cross-builds the original release, the PR base, and the experimental variant
with the official RetroFW 2.3 SDK. The linker warns about mixing abicalls and
non-abicalls objects. Full ABI review, broader differential tests and RS-97
compatibility and performance measurements are still needed before enabling or
merging the experiment. Follow `HANDOFF_MIPS_DYNAREC.md`; do not modify firmware
or bootloader.
