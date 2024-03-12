// Copyright (c) 2014-2017, XMOS Ltd, All rights reserved

#pragma once

#include <xccompat.h>
#include "ethernet.h"

unsafe void eth_send_packet(CLIENT_INTERFACE(ethernet_tx_if, i),
                            char *unsafe packet,
                            unsigned n,
                            unsigned dst_port);
