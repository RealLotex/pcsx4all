#include "test_rs97_lit4_polygon.h"
#include <stdio.h>
#include <string.h>
enum { WORDS = 1024 * 512, GUARD = 1024 };
static uint16_t a[WORDS + GUARD], b[WORDS + GUARD];
static uint32_t seed = 0x52dc19abu;
static uint32_t rnd() { seed ^= seed << 13; seed ^= seed >> 17; seed ^= seed << 5; return seed; }
static uint32_t xy(int x, int y) { return (uint16_t)x | ((uint32_t)(uint16_t)y << 16); }
int main()
{
    for (unsigned i = 0; i < WORDS + GUARD; ++i) a[i] = b[i] = (uint16_t)rnd();
    for (unsigned trial = 0; trial < 2500; ++trial) {
        Lit4Case c; memset(&c, 0, sizeof c);
        c.texture = (trial & 1) ? 128 * 1024 : 0;
        c.clut = (trial % 3 == 0) ? (100 * 1024 + 128) : (480 * 1024);
        c.area[0]=0; c.area[1]=0; c.area[2]=320; c.area[3]=240;
        c.is_quad = trial & 1;
        c.packet[0] = rnd() & 0xffffffu;
        for (unsigned v = 0; v < (c.is_quad ? 4u : 3u); ++v) {
            c.packet[1+v*2] = xy((int)(rnd()%440)-60, (int)(rnd()%300)-30);
            c.packet[2+v*2] = rnd() & 0xffffu;
        }
        for (unsigned k=0; k<16; ++k) {
            uint16_t p = (uint16_t)rnd();
            if ((k+trial)%5 == 0) p=0;
            if ((k+trial)%11 == 0) p=0x8000;
            a[c.clut+k] = b[c.clut+k] = p;
        }
        lit4_ref(c, a); lit4_opt(c, b);
        if (memcmp(a,b,sizeof a)) {
            for (unsigned i=0; i<WORDS+GUARD; ++i) if (a[i]!=b[i]) {
                printf("FAIL polygon=%u word=%u reference=%04x candidate=%04x\n", trial,i,a[i],b[i]);
                return 1;
            }
        }
    }
    puts("PASS 2500 production flat-lit triangles and quads including CLUT feedback");
    return 0;
}
