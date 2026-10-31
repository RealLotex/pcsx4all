# Interpreter / MIPS dynarec regression gate

The default MIPS dynarec now has the differential tests originally isolated
in PR #4. The experimental register-allocation policy and profiling counters
are not included. This is a compatibility gate, not a performance change.

The only production CPU correction is IRGB transfer storage. The interpreter's
`gtecalcMTC2` updates the 16-bit low fields of IR1–IR3 and retains the upper
halves of their internal backing words. The dynarec previously emitted full
word stores and cleared those halves. It now emits `SH` at each `sw.l` offset.
This changes neither the number of emitted instructions nor CPU cycle policy.
The test compares full internal state; retaining those halves is not a claim
that the PS1 exposes them directly through MFC2.

Run in a clean build directory with the official RetroFW SDK:

```sh
make maketree CROSS_COMPILE=/opt/retrofw-sdk/bin/mipsel-linux-
make -j2 all DYNAREC_TEST=1 CROSS_COMPILE=/opt/retrofw-sdk/bin/mipsel-linux-
sysroot=$(/opt/retrofw-sdk/bin/mipsel-linux-gcc --print-sysroot)
timeout 120s qemu-mipsel -L "$sysroot" pcsx4all/pcsx4all.dge
```

The synthetic executable runs before SDL startup and needs no BIOS, game,
save or console. It grants execution permission only to pages containing the
actual JIT array in this dedicated Linux/QEMU process. Test helpers and startup
are compiled out by default (`DYNAREC_TEST=0`). Do not distribute the synthetic
executable as an emulator. Build normally in a clean directory when switching
flags: the Makefile tracks included headers, but does not track flag changes.

Coverage uses the actual interpreter, emitted MIPS code and production guest
memory mappings:

- 50 IRGB cases: dynamic MTC2, constant-source MTC2, register zero and LWC2;
  ordinary execution and jump delay slots; zero, color boundaries and high
  source bits. It checks PC/GPR/CP0/CP2 state and three emitted MIPS `SH` stores.
- 200 deterministic randomized blocks of 120 instructions: unsigned ALU,
  MULTU/HI/LO, CPU loads/stores including LWL/LWR/SWL/SWR, selected GTE data
  transfers and jump delay slots. Compares GPR, PC, CP0, CP2 and all 2 MiB RAM.
- 600 MVMVA cases: matrix/vector/translation selections, shifts, saturation
  modes, randomized GTE state, normal and jump-delay-slot execution.
- Load/use agreement, SYSCALL cause/EPC/vector, SYSCALL in a jump delay slot,
  and recompilation after rewriting the start of a compiled block via the
  normal PS1 memory write path.

CI runs both existing GTE-bound variants, verifies that replacing the three
halfword stores with their previous word stores makes IRGB fail, then builds
a normal OPK and checks that synthetic-test startup strings are absent. Header
dependencies ensure that changing an included emitter really recompiles it.

Limits: this is not complete R3000A conformance, cache-isolation testing, timing
equivalence or a console FPS benchmark. The load/use check compares existing
cores, which currently expose the loaded value immediately. ADD/ADDI overflow
exceptions and DIV overflow are not covered. Only selected MFC2/LWC2/SWC2
registers are randomized. The delay-slot SYSCALL uses the exception vector as
its jump target, so it does not establish exception priority over a different
target. Code invalidation checks the block start, not every byte or alias.
Do not turn agreement between two cores into a claim that all PS1 behavior
is correct. Separate GTE tests cover all 22 arithmetic commands and flags.

No firmware, bootloader, user settings, graphics quality or clock changes are
part of this gate. Any real FPS improvement still requires console measurement.
