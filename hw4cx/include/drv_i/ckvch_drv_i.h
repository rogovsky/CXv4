#ifndef __CKVCH_DRV_I_H
#define __CKVCH_DRV_I_H


enum
{
    CKVCH_CONFIG_ABSENT = 0,
    CKVCH_CONFIG_4x2_1  = 1,
    CKVCH_CONFIG_1x8_1  = 2,
    CKVCH_CONFIG_2x4_1  = 3,
};

// w10i,r10i
enum
{
    CKVCH_CHAN_OUT_n_base  = 0, CKVCH_CHAN_OUT_n_count = 4,
    CKVCH_CHAN_OUT_4BITS   = 4,

    CKVCH_CHAN_NUM_OUTS    = 16,
    CKVCH_CHAN_CONFIG_BITS = 17,
    CKVCH_CHAN_HW_VER      = 18,
    CKVCH_CHAN_SW_VER      = 19,

    CKVCH_NUMCHANS = 20
};


#endif /* __CKVCH_DRV_I_H */
