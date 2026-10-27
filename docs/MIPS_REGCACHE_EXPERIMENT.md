# Register-cache experiment (disabled by default)

The existing allocator frees every unlocked host register when the eight cache
slots are exhausted. The experimental `MIPSREC_SINGLE_SPILL` build flag instead
reuses one unlocked register and chooses the least recently used mapping. The
flag is intentionally not enabled in the RetroFW build: this is a candidate, not
a validated release optimization.

`tests/test_mips_regcache_model.cpp` uses the production allocator with modeled
loads, stores, and additions. It is not the PS1 interpreter or MIPS generated
code. On the deterministic million-addition model, the baseline emitted 875394
loads and 917308 stores, and the experimental version emitted 846147 and 899997.
A nine-register pressure trace emitted 16 loads and 16 stores in each version;
a naive first-victim policy emitted 2008 of each on that same trace. These
synthetic traces do not show the frequency of spills in games or hardware speed.

Before enabling this flag, execute interpreter-versus-dynarec PS1 programs on
MIPS, including delay slots, exceptions, GTE and self-modifying code, and compare
registers, memory, GTE and exception state. Cross-compile the complete RetroFW
build, inspect generated code and test compatibility and performance on the
RS-97. Follow `HANDOFF_MIPS_DYNAREC.md`; do not change firmware or bootloader.
