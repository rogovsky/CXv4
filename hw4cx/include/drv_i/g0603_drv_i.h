#ifndef __G0603_DRV_I_H
#define __G0603_DRV_I_H


// w24i,r8i
enum
{
    G0603_CHAN_CODE_n_base      = 0, G0603_CHAN_CODE_n_count = 8,
    G0603_CHAN_CODE_RATE_n_base = 8,
    G0603_CHAN_CODE_IMM_n_base  = 16,
    G0603_CHAN_CODE_CUR_n_base  = 24,

    G0603_NUM_CHANS = 32,
};

#endif /* __G0603_DRV_I_H */
