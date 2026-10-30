#ifndef TEST_RS97_LIT4_POLYGON_H
#define TEST_RS97_LIT4_POLYGON_H
#include <stdint.h>
struct Lit4Case {
    uint32_t packet[9], texture, clut;
    uint16_t area[4];
    int16_t offset[2];
    uint32_t is_quad;
};
extern "C" void lit4_ref(const Lit4Case &, uint16_t *);
extern "C" void lit4_opt(const Lit4Case &, uint16_t *);
#endif
