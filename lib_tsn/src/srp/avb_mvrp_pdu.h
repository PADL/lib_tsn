// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved

#pragma once

#include "avb_mrp_pdu.h"
#define AVB_MVRP_VID_VECTOR_ATTRIBUTE_TYPE 1

typedef struct {
    uint8_t vlan[2];
} mvrp_vid_vector_first_value;
