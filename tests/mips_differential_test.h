// Runs only in the dedicated MIPS/QEMU test build, not in normal releases.
// Uses the real interpreter, recompiler and production guest memory mappings.
#include <sys/mman.h>
#include <stdint.h>
#include "psxevents.h"
extern u32 *recMem;
#ifdef MIPSREC_PROFILE
extern void recProfileDump();
#endif
static u32 diff_rng = 0x39e07d5u;
static u32 diff_random() { diff_rng ^= diff_rng << 13; diff_rng ^= diff_rng >> 17; diff_rng ^= diff_rng << 5; return diff_rng; }
static int run_mips_differential_test()
{
    setvbuf(stdout, NULL, _IONBF, 0);
    puts("TEST entering MIPS differential harness");
    Config.Cpu = CPU_DYNAREC;
    Config.HLE = 0;
    if (psxInit() != 0) { puts("FAIL psxInit"); return 1; }
    // The original RetroFW platform permits execution from the JIT buffer.
    // Linux/QEMU needs that permission set explicitly in this test process.
    puts("TEST psxInit completed");
    const uintptr_t page = (uintptr_t)recMem & ~(uintptr_t)4095;
    if (mprotect((void *)page, 0x401000, PROT_READ|PROT_WRITE|PROT_EXEC) != 0) {
        perror("mprotect JIT"); return 1;
    }
    memset(&psxRegs, 0, sizeof(psxRegs));
    psxRegs.psxM=psxM; psxRegs.psxP=psxP; psxRegs.psxH=psxH; psxRegs.psxR=psxR;
    psxRegs.writeok=1;
    memset(psxH, 0, 0x10000);
    psxEvqueueInit();
    const u32 start=0x80010000u, end=0x80012000u;
    unsigned long generated_words=0;
    for (unsigned trial=0; trial<200; ++trial) {
        if (!trial) printf("TEST first trial psxM=%p recMem=%p\n", (void *)psxM, (void *)recMem);
        memset(psxM, 0, 0x200000);
        u32 *program=(u32 *)(psxM+0x10000);
        u32 *data=(u32 *)(psxM+0x18000);
        for (unsigned i=0; i<64; ++i) data[i]=diff_random();
        for (unsigned i=0; i<120; ++i) {
            const u32 d=1+diff_random()%25, s=1+diff_random()%25;
            const u32 offset=diff_random() & 0xff;
            switch (diff_random()%15) {
            case 0: {
                const u32 t=1+diff_random()%25;
                program[i]=(s<<21)|(t<<16)|(d<<11)|0x21; // ADDU
                break;
            }
            case 1: program[i]=(9u<<26)|(s<<21)|(d<<16)|(diff_random()&0xffff); break; // ADDIU
            case 2: program[i]=(0x2bu<<26)|(26u<<21)|(d<<16)|(offset&~3u); break; // SW
            case 3: program[i]=(0x28u<<26)|(26u<<21)|(d<<16)|offset; break; // SB
            case 4: program[i]=(0x2au<<26)|(26u<<21)|(d<<16)|offset; break; // SWL
            case 5: program[i]=(0x2eu<<26)|(26u<<21)|(d<<16)|offset; break; // SWR
            case 6: {
                const u32 t=1+diff_random()%25;
                program[i]=(s<<21)|(t<<16)|0x19u; // MULTU
                break;
            }
            case 7: {
                const u32 cpreg=diff_random()%32;
                program[i]=(0x12u<<26)|(4u<<21)|(d<<16)|(cpreg<<11); // MTC2
                break;
            }
            case 8: {
                if (i == 119) { program[i]=0; break; }
                const u32 safe_regs[] = { 0,2,4,6,12,13,14,20,21,22,23,24,25,26,27,30,31 };
                const u32 cpreg=safe_regs[diff_random() % (sizeof(safe_regs)/sizeof(safe_regs[0]))];
                program[i]=(0x12u<<26)|(d<<16)|(cpreg<<11); // MFC2
                program[++i]=0;
                break;
            }
            case 9: {
                if (i == 119) { program[i]=0; break; }
                const u32 cpreg=(diff_random()%4)*2;
                program[i]=(0x32u<<26)|(26u<<21)|(cpreg<<16)|(offset&~3u); // LWC2
                program[++i]=0;
                break;
            }
            case 10: {
                const u32 cpreg=(diff_random()%4)*2;
                program[i]=(0x3au<<26)|(26u<<21)|(cpreg<<16)|(offset&~3u); // SWC2
                break;
            }
            default: {
                // An independent delay-slot instruction follows each load.
                // Preserve room for it at the end of the block.
                if (i == 119) { program[i]=0; break; }
                const u32 loads[] = { 0x23u, 0x20u, 0x21u, 0x22u, 0x26u };
                const u32 op=loads[diff_random()%5];
                const u32 address_offset=op == 0x23u ? (offset&~3u) : (op == 0x21u ? (offset&~1u) : offset);
                program[i]=(op<<26)|(26u<<21)|(d<<16)|address_offset; // LW/LB/LH/LWL/LWR
                program[++i]=0;
                break;
            }
            }
        }
        program[120]=(2u<<26)|((end>>2)&0x03ffffffu); // J
        program[121]=(9u<<26)|(1u<<21)|(1u<<16)|7u; // delay slot
        psxRegs.pc=start; psxRegs.cycle=0; psxRegs.interrupt=0;
        for (unsigned r=1; r<32; ++r) psxRegs.GPR.r[r]=diff_random();
        psxRegs.GPR.r[26]=0x80018000u;
        static u8 initial_ram[0x200000];
        memcpy(initial_ram, psxM, sizeof(initial_ram));
        const psxRegisters initial=psxRegs;
        if (!trial) puts("TEST interpreter start");
        psxCpu=&psxInt; psxInt.ExecuteBlock(end);
        if (!trial) puts("TEST interpreter end");
        const psxRegisters expected=psxRegs;
        static u8 expected_ram[0x200000];
        memcpy(expected_ram, psxM, sizeof(expected_ram));
        memcpy(psxM, initial_ram, sizeof(initial_ram));
        psxRegs=initial;
        if (!trial) puts("TEST dynarec start");
        psxCpu=&psxRec; psxRec.Reset();
        u32 *const code_start=recMem;
        psxRec.ExecuteBlock(end);
        generated_words += (unsigned long)(recMem - code_start);
        if (!trial) puts("TEST dynarec end");
        for (unsigned r=0; r<34; ++r) if (psxRegs.GPR.r[r]!=expected.GPR.r[r]) {
            printf("FAIL trial %u GPR %u: interpreter=%08x dynarec=%08x\n",trial,r,expected.GPR.r[r],psxRegs.GPR.r[r]);
            return 1;
        }
        if (memcmp(expected_ram, psxM, sizeof(expected_ram))) {
            printf("FAIL trial %u RAM differs\n", trial); return 1;
        }
        bool mismatch=psxRegs.pc!=expected.pc;
        for (unsigned r=0; r<32; ++r) {
            if (psxRegs.CP0.r[r]!=expected.CP0.r[r]) {
                printf("CP0[%u] interpreter=%08x dynarec=%08x\n",r,expected.CP0.r[r],psxRegs.CP0.r[r]); mismatch=true;
            }
            if (psxRegs.CP2D.r[r]!=expected.CP2D.r[r]) {
                printf("CP2D[%u] interpreter=%08x dynarec=%08x\n",r,expected.CP2D.r[r],psxRegs.CP2D.r[r]); mismatch=true;
            }
            if (psxRegs.CP2C.r[r]!=expected.CP2C.r[r]) {
                printf("CP2C[%u] interpreter=%08x dynarec=%08x\n",r,expected.CP2C.r[r],psxRegs.CP2C.r[r]); mismatch=true;
            }
        }
        if (mismatch) {
            printf("FAIL trial %u PC interpreter=%08x dynarec=%08x\n",trial,expected.pc,psxRegs.pc);
            for (unsigned i=0; i<122; ++i) printf("%03u: %08x\n",i,program[i]);
            return 1;
        }
    }
    puts("PASS real interpreter vs MIPS dynarec: 200 randomized ALU/HI-LO/GTE and CPU/GTE load-store blocks and jump delay slots");
    printf("Generated MIPS instructions for randomized blocks: %lu\n", generated_words);
    // IRGB updates the low halves of IR1-IR3 and must retain the upper
    // halves. Also check that the JIT really emitted MIPS32 SH instructions.
    u32 *gte_program=(u32 *)(psxM+0x10000);
    gte_program[0]=(0x12u<<26)|(4u<<21)|(2u<<16)|(28u<<11);
    gte_program[1]=(2u<<26)|((end>>2)&0x03ffffffu);
    gte_program[2]=0;
    psxRegs.pc=start; psxRegs.GPR.r[2]=0x7fffu;
    psxRegs.CP2D.r[9]=0xaabb1357u; psxRegs.CP2D.r[10]=0xccdd2468u; psxRegs.CP2D.r[11]=0x1234abcdu;
    const psxRegisters gte_start=psxRegs;
    psxCpu=&psxInt; psxInt.ExecuteBlock(end);
    const psxRegisters gte_expected=psxRegs;
    if (gte_expected.CP2D.r[9]!=0xaabb0f80u || gte_expected.CP2D.r[10]!=0xccdd0f80u ||
        gte_expected.CP2D.r[11]!=0x12340f80u) { puts("FAIL interpreter IRGB fields"); return 1; }
    psxRegs=gte_start; psxCpu=&psxRec; psxRec.Reset();
    u32 *const gte_code_start=recMem;
    psxRec.ExecuteBlock(end);
    if (memcmp(&psxRegs.CP2D,&gte_expected.CP2D,sizeof(psxRegs.CP2D))) {
        puts("FAIL dynarec IRGB fields"); return 1;
    }
    unsigned sh_found=0;
    const unsigned ir_offset=(unsigned)((char *)&psxRegs.CP2D.r[9]-(char *)&psxRegs);
    for (const u32 *insn=gte_code_start; insn<recMem; ++insn) {
        if ((*insn>>26)==0x29u && ((*insn>>21)&31u)==30u) {
            const unsigned off=*insn & 0xffffu;
            if (off==ir_offset || off==ir_offset+4 || off==ir_offset+8) ++sh_found;
        }
    }
    if (sh_found!=3) { printf("FAIL IRGB emitted %u expected 3 halfword stores\n",sh_found); return 1; }
    puts("PASS IRGB preservation and three emitted MIPS32 halfword stores");
    // Compare the existing CPU cores on a load immediately followed by a
    // use of its destination. Agreement alone is not an R3000A oracle.
    gte_program[0]=(0x23u<<26)|(26u<<21)|(1u<<16); // LW r1,0(r26)
    gte_program[1]=(9u<<26)|(1u<<21)|(2u<<16); // ADDIU r2,r1,0
    gte_program[2]=(9u<<26)|(1u<<21)|(3u<<16); // ADDIU r3,r1,0
    gte_program[3]=(2u<<26)|((end>>2)&0x03ffffffu);
    gte_program[4]=0;
    *(u32 *)(psxM+0x18000)=0x22222222u;
    psxRegs.pc=start; psxRegs.GPR.r[1]=0x11111111u; psxRegs.GPR.r[26]=0x80018000u;
    const psxRegisters hazard_start=psxRegs;
    psxCpu=&psxInt; psxInt.ExecuteBlock(end);
    const psxRegisters hazard_expected=psxRegs;
    psxRegs=hazard_start; psxCpu=&psxRec; psxRec.Reset(); psxRec.ExecuteBlock(end);
    if (memcmp(&psxRegs.GPR,&hazard_expected.GPR,sizeof(psxRegs.GPR))) {
        printf("FAIL load-use compatibility: interpreter r2=%08x dynarec r2=%08x\n",
               hazard_expected.GPR.r[2],psxRegs.GPR.r[2]); return 1;
    }
    printf("PASS load-use compatibility; following instruction saw %08x\n",psxRegs.GPR.r[2]);
    // Test a non-delay-slot SYSCALL against both the interpreter and the
    // architectural exception code, EPC, and vector.
    u32 *program=(u32 *)(psxM+0x10000);
    program[0]=0x0000000cu; // SYSCALL
    psxRegs.pc=start; psxRegs.cycle=0; psxRegs.CP0.n.Status=0; psxRegs.CP0.n.Cause=0;
    const psxRegisters exception_start=psxRegs;
    psxCpu=&psxInt; psxInt.ExecuteBlock(0x80000080u);
    const psxRegisters exception_expected=psxRegs;
    if (exception_expected.CP0.n.EPC!=start || (exception_expected.CP0.n.Cause & 0x7cu)!=0x20u) {
        puts("FAIL interpreter SYSCALL architectural exception state"); return 1;
    }
    psxRegs=exception_start; psxCpu=&psxRec; psxRec.Reset();
    psxRec.ExecuteBlock(0x80000080u);
    if (psxRegs.pc!=exception_expected.pc ||
        memcmp(&psxRegs.CP0,&exception_expected.CP0,sizeof(psxRegs.CP0)) ||
        memcmp(&psxRegs.GPR,&exception_expected.GPR,sizeof(psxRegs.GPR))) {
        puts("FAIL SYSCALL interpreter/dynarec state"); return 1;
    }
    puts("PASS SYSCALL exception vector, cause, EPC and CPU state");
    // Branch to the exception vector with SYSCALL in the delay slot. This
    // checks BD and EPC without running unbounded guest code after an error.
    program[0]=(2u<<26)|((0x80000080u>>2)&0x03ffffffu);
    program[1]=0x0000000cu;
    psxRegs=exception_start; psxRegs.pc=start;
    psxCpu=&psxInt; psxInt.ExecuteBlock(0x80000080u);
    const psxRegisters delay_exception_expected=psxRegs;
    if (delay_exception_expected.CP0.n.EPC!=start ||
        (delay_exception_expected.CP0.n.Cause & 0x8000007cu)!=0x80000020u) {
        puts("FAIL interpreter delay-slot SYSCALL cause/EPC"); return 1;
    }
    psxRegs=exception_start; psxRegs.pc=start;
    psxCpu=&psxRec; psxRec.Reset(); psxRec.ExecuteBlock(0x80000080u);
    if (psxRegs.pc!=delay_exception_expected.pc ||
        memcmp(&psxRegs.CP0,&delay_exception_expected.CP0,sizeof(psxRegs.CP0)) ||
        memcmp(&psxRegs.GPR,&delay_exception_expected.GPR,sizeof(psxRegs.GPR))) {
        printf("FAIL delay-slot SYSCALL: expected cause=%08x EPC=%08x, got cause=%08x EPC=%08x\n",
               delay_exception_expected.CP0.n.Cause, delay_exception_expected.CP0.n.EPC,
               psxRegs.CP0.n.Cause, psxRegs.CP0.n.EPC); return 1;
    }
    puts("PASS delay-slot SYSCALL cause, BD bit, EPC and CPU state");
    // Invalidate the start of an already-compiled block through the normal
    // PS1 memory write path, then verify the modified instruction executes.
    program[0]=(9u<<26)|(1u<<21)|(1u<<16)|1u; // ADDIU r1,r1,1
    program[1]=(2u<<26)|((end>>2)&0x03ffffffu);
    program[2]=0;
    psxCpu=&psxRec; psxRec.Reset(); psxRegs.pc=start; psxRegs.GPR.r[1]=3;
    psxRec.ExecuteBlock(end);
    if (psxRegs.GPR.r[1]!=4) { puts("FAIL initial code block"); return 1; }
    psxMemWrite32(start, (9u<<26)|(1u<<21)|(1u<<16)|7u);
    psxRegs.pc=start; psxRegs.GPR.r[1]=3;
    psxRec.ExecuteBlock(end);
    if (psxRegs.GPR.r[1]!=10) { puts("FAIL self-modifying block invalidation"); return 1; }
    puts("PASS rewritten block is invalidated and recompiled");
#ifdef MIPSREC_PROFILE
    recProfileDump();
#endif
    return 0;
}
