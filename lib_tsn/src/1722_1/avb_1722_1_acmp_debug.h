// Copyright (c) 2011-2017, XMOS Ltd, All rights reserved

#pragma once

char *debug_acmp_message_s[] = {
    "CONNECT_TX_COMMAND",        "CONNECT_TX_RESPONSE",       "DISCONNECT_TX_COMMAND",
    "DISCONNECT_TX_RESPONSE",    "GET_TX_STATE_COMMAND",      "GET_TX_STATE_RESPONSE",
    "CONNECT_RX_COMMAND",        "CONNECT_RX_RESPONSE",       "DISCONNECT_RX_COMMAND",
    "DISCONNECT_RX_RESPONSE",    "GET_RX_STATE_COMMAND",      "GET_RX_STATE_RESPONSE",
    "GET_TX_CONNECTION_COMMAND", "GET_TX_CONNECTION_RESPONSE"};

char *debug_acmp_status_s[] = {"SUCCESS",
                               "LISTENER_UNKNOWN_ID",
                               "TALKER_UNKNOWN_ID",
                               "TALKER_DEST_MAC_FAIL",
                               "TALKER_NO_STREAM_INDEX",
                               "TALKER_NO_BANDWIDTH",
                               "TALKER_EXCLUSIVE",
                               "LISTENER_TALKER_TIMEOUT",
                               "LISTENER_EXCLUSIVE",
                               "STATE_UNAVAILABLE",
                               "NOT_CONNECTED",
                               "NO_SUCH_CONNECTION",
                               "COULD_NOT_SEND_MESSAGE",
                               "LISTENER_DEFAULT_FORMAT_INVALID",
                               "TALKER_DEFAULT_FORMAT_INVALID",
                               "DEFAULT_SET_DIFFERENT",
                               "NOT_SUPPORTED"};
