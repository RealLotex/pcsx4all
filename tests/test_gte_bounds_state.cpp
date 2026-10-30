#include "../src/gte.h"
#include <stdio.h>
#include <string.h>
psxRegisters psxRegs;
extern void bounds_gteRTPS();
extern void bounds_gteNCLIP();
extern void bounds_gteNCDS();
extern void bounds_gteNCDT();
extern void bounds_gteCDP();
extern void bounds_gteNCCS();
extern void bounds_gteCC();
extern void bounds_gteNCS();
extern void bounds_gteNCT();
extern void bounds_gteDPCT();
extern void bounds_gteAVSZ3();
extern void bounds_gteAVSZ4();
extern void bounds_gteRTPT();
extern void bounds_gteNCCT();
extern void bounds_gteOP(u32);
extern void bounds_gteDPCS(u32);
extern void bounds_gteINTPL(u32);
extern void bounds_gteMVMVA(u32);
extern void bounds_gteSQR(u32);
extern void bounds_gteDCPL(u32);
extern void bounds_gteGPF(u32);
extern void bounds_gteGPL(u32);
static u32 seed=0xd0314fc7u;
static u32 rnd() { seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed; }
static void initial_state(unsigned trial)
{
    memset(&psxRegs,0,sizeof psxRegs);
    for (unsigned i=0;i<32;++i) { psxRegs.CP2D.r[i]=rnd();psxRegs.CP2C.r[i]=rnd(); }
    if ((trial&3u)==1) {
        for (unsigned i=0;i<8;++i) psxRegs.CP2C.r[i]=0;
        psxRegs.CP2C.r[0]=psxRegs.CP2C.r[2]=psxRegs.CP2C.r[4]=0x1000u;
        psxRegs.CP2C.r[7]=1024;psxRegs.CP2C.r[26]=256;
    } else if ((trial&3u)==2) {
        memset(&psxRegs.CP2D,0,sizeof psxRegs.CP2D);
        memset(&psxRegs.CP2C,0,sizeof psxRegs.CP2C);
    } else if ((trial&3u)==3) {
        const u32 v=(trial&4u)?0x7fff7fffu:0x80008000u;
        for (unsigned i=0;i<8;++i) psxRegs.CP2D.r[i]=v;
        for (unsigned i=0;i<24;++i) psxRegs.CP2C.r[i]=(i%8<5)?v:((trial&8)?0x7fffffffu:0x80000000u);
    }
}
static bool same(const psxRegisters &expected,const char *name,unsigned trial)
{
    if (memcmp(&psxRegs,&expected,sizeof psxRegs)) {
        printf("FAIL %s trial=%u FLAG=%08x/%08x\n",name,trial,psxRegs.CP2C.r[31],expected.CP2C.r[31]);
        return false;
    }
    return true;
}
int main()
{
    struct Zero { const char *name;void (*original)();void (*fast)(); };
    struct One { const char *name;void (*original)(u32);void (*fast)(u32); };
    const Zero zero[]={
        {"RTPS",gteRTPS,bounds_gteRTPS},
        {"NCLIP",gteNCLIP,bounds_gteNCLIP},
        {"NCDS",gteNCDS,bounds_gteNCDS},
        {"NCDT",gteNCDT,bounds_gteNCDT},
        {"CDP",gteCDP,bounds_gteCDP},
        {"NCCS",gteNCCS,bounds_gteNCCS},
        {"CC",gteCC,bounds_gteCC},
        {"NCS",gteNCS,bounds_gteNCS},
        {"NCT",gteNCT,bounds_gteNCT},
        {"DPCT",gteDPCT,bounds_gteDPCT},
        {"AVSZ3",gteAVSZ3,bounds_gteAVSZ3},
        {"AVSZ4",gteAVSZ4,bounds_gteAVSZ4},
        {"RTPT",gteRTPT,bounds_gteRTPT},
        {"NCCT",gteNCCT,bounds_gteNCCT}
    };
    const One one[]={
        {"OP",gteOP,bounds_gteOP},
        {"DPCS",gteDPCS,bounds_gteDPCS},
        {"INTPL",gteINTPL,bounds_gteINTPL},
        {"MVMVA",gteMVMVA,bounds_gteMVMVA},
        {"SQR",gteSQR,bounds_gteSQR},
        {"DCPL",gteDCPL,bounds_gteDCPL},
        {"GPF",gteGPF,bounds_gteGPF},
        {"GPL",gteGPL,bounds_gteGPL}
    };
    unsigned cases=0;
    for (unsigned trial=0;trial<10000u;++trial) {
        for (unsigned i=0;i<sizeof zero/sizeof zero[0];++i) {
            initial_state(trial);const psxRegisters initial=psxRegs;
            zero[i].original();const psxRegisters expected=psxRegs;
            psxRegs=initial;zero[i].fast();
            if (!same(expected,zero[i].name,trial)) return 1;
            ++cases;
        }
        for (unsigned i=0;i<sizeof one/sizeof one[0];++i) {
            initial_state(trial);const psxRegisters initial=psxRegs;const u32 op=rnd();
            one[i].original(op);const psxRegisters expected=psxRegs;
            psxRegs=initial;one[i].fast(op);
            if (!same(expected,one[i].name,trial)) return 1;
            ++cases;
        }
    }
    printf("PASS %u complete production GTE state comparisons across 22 commands\n",cases);
    return 0;
}
