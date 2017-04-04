#ifndef __V0628_DRV_I_H
#define __V0628_DRV_I_H


// w25i,r33i
enum
{
    V0628_CHAN_WR_N_BASE = 0,   V0628_CHAN_WR_N_COUNT = 24,
    V0628_CHAN_FAKE_WR1  = 24,
    V0628_CHAN_RD_N_BASE = 25,  V0628_CHAN_RD_N_COUNT = 32,
    V0628_CHAN_RD1       = 57,

    V0628_NUMCHANS       = 58,
};


#endif /* __V0628_DRV_I_H */
