#ifndef __CAC208_DRV_I_H
#define __CAC208_DRV_I_H


#include "drv_i/kozdev_common_drv_i.h"


// w40i,r20i,w8i,r16i,w1i,r2i,w13i,r100i,w32i,w32i,r32i,r4i,w1i31,w1i248,w8i31,w89i,r1t100
enum
{
    CAC208_CHAN_ADC_n_count = 24,
    CAC208_CHAN_OUT_n_count = 8,

    CAC208_CHAN_OUT_IMM_n_base = KOZDEV_CHAN_OUT_n_base      + CAC208_CHAN_OUT_n_count,
    CAC208_CHAN_OUT_TAC_n_base = KOZDEV_CHAN_OUT_RATE_n_base + CAC208_CHAN_OUT_n_count,
};


#endif /* __CAC208_DRV_I_H */
