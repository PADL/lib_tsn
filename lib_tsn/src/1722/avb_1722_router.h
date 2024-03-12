// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved

#pragma once

#include <xccompat.h>
#include "ethernet.h"
#include "default_avb_conf.h"

void avb_1722_enable_stream_forwarding(CLIENT_INTERFACE(ethernet_cfg_if, i_eth),
                                       unsigned int stream_id[2]);

void avb_1722_disable_stream_forwarding(CLIENT_INTERFACE(ethernet_cfg_if, i_eth),
                                        unsigned int stream_id[2]);

void avb_1722_add_stream_mapping(CLIENT_INTERFACE(ethernet_cfg_if, i_eth),
                                 unsigned int stream_id[2],
                                 int link_num,
                                 int avb_hash);

void avb_1722_remove_stream_mapping(CLIENT_INTERFACE(ethernet_cfg_if, i_eth),
                                    unsigned int streamId[2]);

void avb_1722_remove_stream_from_table(CLIENT_INTERFACE(ethernet_cfg_if, i_eth),
                                       unsigned int streamId[2]);
