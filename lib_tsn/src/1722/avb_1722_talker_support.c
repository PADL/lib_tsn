// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved

#include <xccompat.h>

#include "avb_1722_talker.h"
#include "default_avb_conf.h"

void avb1722_set_buffer_vlan(int vlan, uint8_t Buf0[]) {
    uint8_t *Buf = &Buf0[2];
    AVB_Frame_t *pEtherHdr = (AVB_Frame_t *)&(Buf[0]);

    CLEAR_AVBTP_VID(pEtherHdr);
    SET_AVBTP_VID(pEtherHdr, vlan);

    return;
}
