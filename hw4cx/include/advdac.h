#ifndef __ADVDAC_H
#define __ADVDAC_H


typedef struct
{
    int32  cur;  // Current value
    int32  spd;  // Speed
    int32  min;  // Min alwd
    int32  max;  // Max alwd
    int32  trg;  // Target
    int32  dsr;  // DeSiRed current value; used in smooth movement

    // slowmo
    int32  tac;    // Time of AcCeleration (in heartbeats)
    int32  mxs;    // MaX speed per heartbeat-Step
    int32  crs;    // CuRrent Speed
    int32  mns;    // MiN Speed per heartbeat-Step
    int32  acs;    // AcCeleration per heartbeat-Step
    int32  a_dir;    // +1: accelerating, 0: linear movement, -1:decelerating
    int32  dcd;    // DeCeleration Distance (decelerate when nearer to target)

    // cankoz_table
    int8   rcv;  // Current value was received
    int8   aff;  // Is affected by table
    int8   lkd;  // Is locked by table
    int8   isc;  // Is changing now
    int8   nxt;  // Next write may be performed (previous was acknowledged)
    int8   fst;  // This is the FirST step
    int8   fin;  // This is a FINal write
    int8   rsv;  // Reserved (8th int8)
} advdac_out_ch_t;


#endif /* __ADVDAC_H */
