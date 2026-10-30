// Include the production inline helper, not a copy of the optimized code.
#include "../src/gte.cpp"
#include <stdio.h>
psxRegisters psxRegs;
static u32 seed=0x3f81a901u;
static u32 rnd() { seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed; }
static bool check(s64 value,s64 maximum,s64 minimum,u32 flags,u32 positive,u32 negative)
{
    const u32 expected=flags|(value>maximum?positive:(value<minimum?negative:0u));
    psxRegs.CP2C.r[31]=flags;
    const s64 actual=BOUNDS(value,maximum,(int)positive,minimum,(int)negative);
    if (actual!=value || psxRegs.CP2C.r[31]!=expected)
        return puts("FAIL BOUNDS value, flags or generic limit fallback"),false;
    return true;
}
int main()
{
    const s64 values[]={INT64_MIN,INT64_MIN+1,-INT64_C(4294967297),-INT64_C(4294967296),
        -INT64_C(2147483649),INT32_MIN,(s64)INT32_MIN+1,-1,0,1,(s64)INT32_MAX-1,
        INT32_MAX,INT64_C(2147483648),INT64_C(4294967295),INT64_C(4294967296),INT64_MAX-1,INT64_MAX};
    const u32 positive[]={0x40000000u,0x20000000u,0x10000000u,0x80010000u};
    const u32 negative[]={0x88000000u,0x84000000u,0x82000000u,0x80008000u};
    for (unsigned i=0;i<sizeof values/sizeof values[0];++i) for (unsigned f=0;f<4;++f)
        if (!check(values[i],INT32_MAX,INT32_MIN,rnd(),positive[f],negative[f])) return 1;
    for (unsigned i=0;i<3000000u;++i) {
        const u32 hi=rnd(),lo=rnd();
        const s64 value=(s64)(s32)hi*INT64_C(4294967296)+(u32)lo;
        const unsigned f=i&3u;
        if (!check(value,INT32_MAX,INT32_MIN,rnd(),positive[f],negative[f])) return 1;
        if ((i&15u)==0 && !check(value,123456,-654321,rnd(),positive[f],negative[f])) return 1;
    }
    puts("PASS production BOUNDS: signed-64 extremes, signed-32 edges, flags, returned value, 3 million inputs and custom limits");
    return 0;
}
