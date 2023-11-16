// Copyright (c) 2013-2017, XMOS Ltd, All rights reserved

#ifndef _audio_clock_CS2100CP_h_
#define _audio_clock_CS2100CP_h_
#include "i2c.h"

void audio_clock_CS2100CP_init_ex(client interface i2c_master_if i2c, unsigned int mclks_per_wclk);

// Set up the multiplier in the PLL clock generator
static inline void audio_clock_CS2100CP_init(client interface i2c_master_if i2c)
{
  audio_clock_CS2100CP_init_ex(i2c, 512);
}

#endif
