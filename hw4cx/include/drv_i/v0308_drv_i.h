#ifndef __V0308_DRV_I_H
#define __V0308_DRV_I_H


// w20i,r20i
enum
{
    V0308_CHAN_VOLTS_base  = 0,
    V0308_CHAN_RESET_base  = 2,
    V0308_CHAN_SW_ON_base  = 4,
    V0308_CHAN_SW_OFF_base = 6,
    V0308_CHAN_UNUS8_base  = 8,
    V0308_CHAN_RATE_base   = 10,
    V0308_CHAN_IMM_base    = 12,

    V0308_CHAN_IS_ON_base  = 20,
    V0308_CHAN_OVL_U_base  = 22,
    V0308_CHAN_OVL_I_base  = 24,
    V0308_CHAN_PLRTY_base  = 26,
    V0308_CHAN_CUR_base    = 30,

    V0308_NUMCHANS = 20
};

#endif /* __V0308_DRV_I_H */
