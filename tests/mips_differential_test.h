// Runs only in the dedicated MIPS/QEMU test build, not in normal releases.
// Uses the real interpreter, recompiler and production guest memory mappings.
#include <sys/mman.h>
#include <stdint.h>
extern u32 *recMem;
static u32 diff_rng = 0x39e07d5u;
static u32 diff_random() { diff_rng ^= diff_rng << 13; diff_rng ^= diff_rng >> 17; diff_rng ^= diff_rng << 5; return diff_rng; }
static int run_mips_differential_test()
{
    Config.Cpu = CPU_DYNAREC;
    Config.HLE = 0;
    if (psxInit() != 0) { puts("FAIL psxInit"); return 1; }
    // The original RetroFW platform permits execution from the JIT buffer.
    // Linux/QEMU needs that permission set explicitly in this test process.
    const uintptr_t page = (uintptr_t)recMem & ~(uintptr_t)4095;
    if (mprotect((void *)page, 0x401000, PROT_READ|PROT_WRITE|PROT_EXEC) != 0) {
        perror("mprotect JIT"); return 1;
    }
    memset(&psxRegs, 0, sizeof(psxRegs));
    psxRegs.psxM=psxM; psxRegs.psxP=psxP; psxRegs.psxH=psxH; psxRegs.psxR=psxR;
    psxRegs.writeok=1; psxRegs.io_cycle_counter=0x7fffffffu;
    const u32 start=0x80010000u, end=0x80012000u;
    for (unsigned trial=0; trial<200; ++trial) {
        memset(psxM, 0, 0x200000);
        u32 *program=(u32 *)(psxM+0x10000);
        for (unsigned i=0; i<120; ++i) {
            const u32 d=1+diff_random()%25, s=1+diff_random()%25;
            if (i & 1) {
                const u32 t=1+diff_random()%25;
                program[i]=(s<<21)|(t<<16)|(d<<11)|0x21; // ADDU
            } else {
                program[i]=(9u<<26)|(s<<21)|(d<<16)|(diff_random()&0xffff); // ADDIU
            }
        }
        program[120]=(2u<<26)|((end>>2)&0x03ffffffu); // J
        program[121]=(9u<<26)|(1u<<21)|(1u<<16)|7u; // delay slot
        psxRegs.pc=start; psxRegs.cycle=0; psxRegs.interrupt=0;
        for (unsigned r=1; r<32; ++r) psxRegs.GPR.r[r]=diff_random();
        const psxRegisters initial=psxRegs;
        psxCpu=&psxInt; psxInt.ExecuteBlock(end);
        const psxRegisters expected=psxRegs;
        psxRegs=initial;
        psxCpu=&psxRec; psxRec.Reset(); psxRec.ExecuteBlock(end);
        for (unsigned r=0; r<34; ++r) if (psxRegs.GPR.r[r]!=expected.GPR.r[r]) {
            printf("FAIL trial %u GPR %u: interpreter=%08x dynarec=%08x\n",trial,r,expected.GPR.r[r],psxRegs.GPR.r[r]);
            return 1;
        }
        if (psxRegs.pc!=expected.pc || memcmp(&psxRegs.CP0,&expected.CP0,sizeof(psxRegs.CP0)) ||
            memcmp(&psxRegs.CP2D,&expected.CP2D,sizeof(psxRegs.CP2D)) ||
            memcmp(&psxRegs.CP2C,&expected.CP2C,sizeof(psxRegs.CP2C))) {
            printf("FAIL trial %u PC/CP state\n",trial); return 1;
        }
    }
    puts("PASS real interpreter vs MIPS dynarec: 200 randomized ALU blocks and jump delay slots");
    return 0;
}
