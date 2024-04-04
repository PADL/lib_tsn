/*
 * Copyright (c) 2011 Jacques Fortier
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stddef.h>

#include "xc2compat.h"

#define COBS_DELIM_BYTE 0x00

#define _COBS_BUFFER_PAD(n) (((n + 254 - 1) & ~(254 - 1)) / 254)

size_t cobs_encode(ARRAY_OF_SIZE(const uint8_t, input, input_length),
                   size_t input_length,
                   ARRAY_OF_SIZE(uint8_t, output, output_length),
                   size_t output_length);

size_t cobs_decode(ARRAY_OF_SIZE(const uint8_t, input, input_length),
                   size_t input_length,
                   ARRAY_OF_SIZE(uint8_t, output, output_length),
                   size_t output_length);
