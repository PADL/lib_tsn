// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved

#pragma once

#include "avb_mrp_pdu.h"

// We don't use the Service Requirement type
#define AVB_MMRP_SERVICE_REQUIREMENT_ATTRIBUTE_TYPE 1
#define AVB_MMRP_MAC_VECTOR_ATTRIBUTE_TYPE          2

typedef struct {
    uint8_t addr[6];
} mmrp_mac_vector_first_value;

typedef struct {
    uint8_t addr[6];
} mmrp_service_requirement_first_value;
