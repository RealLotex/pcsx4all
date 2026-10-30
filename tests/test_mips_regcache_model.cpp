// Exercise the production register allocator with a small executable model.
// This is not a replacement for running the MIPS JIT against the interpreter.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t u32;
typedef int32_t s32;
enum { MIPSREG_S0=16, MIPSREG_S7=23, PERM_REG_1=30 };
static u32 guest[32], host[32], expected[32];
static unsigned loads, stores;
static u32 seed = 0x9317b12u;
static u32 rnd() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
static bool IsConst(u32) { return false; }
static u32 GetConst(u32) { return 0; }
#define offGPR(r) (r)
static void LW(u32 d, u32, u32 r) { host[d] = guest[r]; ++loads; }
static void SW(u32 s, u32, u32 r) { guest[r] = host[s]; ++stores; }
static void LI32(u32 d, u32 v) { host[d] = v; }
#include "../src/recompiler/mips/regcache.h"

static int check(const char *label) {
    regClearJump();
    if (memcmp(guest, expected, sizeof(guest))) {
        for (unsigned i=0; i<32; ++i) if (guest[i] != expected[i])
            printf("FAIL %s r%u got=%08x want=%08x\n",label,i,guest[i],expected[i]);
        return 1;
    }
    return 0;
}
static void add(unsigned d, unsigned s, u32 imm) {
    const u32 hd = regMipsToHost(d, d == s ? REG_LOAD : REG_FIND, REG_REGISTER);
    const u32 hs = d == s ? hd : regMipsToHost(s, REG_LOAD, REG_REGISTER);
    host[hd] = host[hs] + imm;
    expected[d] = expected[s] + imm;
    regMipsChanged(d);
    regUpdate();
}
int main() {
    unsigned long total_loads=0, total_stores=0;
    for (unsigned trial=0; trial<1000; ++trial) {
        for (unsigned i=0; i<32; ++i) guest[i]=expected[i]=rnd();
        guest[0]=expected[0]=0;
        memset(host,0,sizeof(host)); loads=stores=0; regReset();
        for (unsigned i=0; i<1000; ++i) {
            unsigned d=1+rnd()%31, s=1+rnd()%31;
            add(d,s,rnd());
            if ((i % 71)==70 && check("periodic flush")) return 1;
        }
        if (check("final flush")) return 1;
        total_loads+=loads; total_stores+=stores;
    }
    printf("PASS allocator model: 1000000 additions; loads=%lu stores=%lu\n",total_loads,total_stores);
    // Eight live guest registers, plus a ninth transient register. Count emitted
    // loads/stores on this controlled trace; it does not represent a game trace.
    memset(guest,0,sizeof(guest)); memset(expected,0,sizeof(expected));
    memset(host,0,sizeof(host)); loads=stores=0; regReset();
    for (unsigned i=1; i<=8; ++i) add(i,i,1);
    for (unsigned i=0; i<1000; ++i) {
        add(9,9,1);
        for (unsigned j=1; j<=7; ++j) add(j,j,1);
    }
    if (check("nine-register pressure")) return 1;
    printf("TRACE nine-register pressure: loads=%u stores=%u\n",loads,stores);
    if (loads > 16 || stores > 16) {
        puts("FAIL excessive traffic in nine-register pressure trace");
        return 1;
    }
    // Keep a source locked while a destination forces eviction.
    memset(guest,0,sizeof(guest)); memset(expected,0,sizeof(expected));
    memset(host,0,sizeof(host)); regReset();
    for (unsigned i=1; i<=8; ++i) add(i,i,i);
    const u32 source = regMipsToHost(1, REG_LOAD, REG_REGISTER);
    const u32 destination = regMipsToHost(9, REG_FIND, REG_REGISTER);
    if (source == destination || host[source] != expected[1]) {
        puts("FAIL locked source was evicted"); return 1;
    }
    host[destination] = host[source] + 5;
    expected[9] = expected[1] + 5;
    regMipsChanged(9); regUpdate();
    if (check("locked source")) return 1;
    puts("PASS locked source survives eviction");
    return 0;
}
