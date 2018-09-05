#ifndef __UDPCAN_PROTO_H
#define __UDPCAN_PROTO_H


#include "misc_types.h"


enum {UDP_CAN_PORT_BASE = 9000};

typedef struct
{
    uint32 timestamp_high;
    uint32 timestamp_low;
    uint32 can_id;
    uint32 can_dlc;
    uint8  can_data[8];
} udp_can_frame_t;

typedef struct
{
    uint8  timestamp_high_b[4];
    uint8  timestamp_low_b [4];
    uint8  can_id_b [4];
    uint8  can_dlc_b[4];
    uint8  can_data[8];
} udp_can_frame_b_t;


#endif /* __UDPCAN_PROTO_H */
