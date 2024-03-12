// Copyright (c) 2013-2017, XMOS Ltd, All rights reserved
#include <print.h>
#include "avb_1722_common.h"

short ntoh_16(uint8_t x[2]) { return ((x[0] << 8) | x[1]); }

int ntoh_32(uint8_t x[4]) { return ((x[0] << 24) | x[1] << 16 | x[2] << 8 | x[3]); }

void get_64(uint8_t g[8], uint8_t c[8]) {
    for (int i = 0; i < 8; i++) {
        g[7 - i] = c[i];
    }
}

void set_64(uint8_t g[8], uint8_t c[8]) {
    for (int i = 0; i < 8; i++) {
        g[i] = c[7 - i];
    }
}

void hton_16(uint8_t x[2], uint16_t v) {
    x[0] = (v >> 8) & 0xFF;
    x[1] = (v & 0xFF);
}

void hton_32(uint8_t x[4], unsigned int v) {
    x[0] = (uint8_t)(v >> 24);
    x[1] = (uint8_t)(v >> 16);
    x[2] = (uint8_t)(v >> 8);
    x[3] = (uint8_t)(v);
}

static inline void hton_32_inline(uint8_t x[4], unsigned int v);