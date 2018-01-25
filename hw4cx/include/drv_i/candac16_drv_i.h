#ifndef __CANDAC16_DRV_I_H
#define __CANDAC16_DRV_I_H


#include "drv_i/kozdev_common_drv_i.h"


// w40i,r20i,w8i,r16i,w1i,r2i,w13i,r100i,w32i,w32i,r32i,r4i,w1i31,w1i496,w16i31,w81i,r1t100
enum
{
    CANDAC16_CHAN_ADC_n_count = 0,
    CANDAC16_CHAN_OUT_n_count = 16,

    CANDAC16_CHAN_OUT_IMM_n_base = KOZDEV_CHAN_OUT_n_base      + CANDAC16_CHAN_OUT_n_count,
    CANDAC16_CHAN_OUT_TAC_n_base = KOZDEV_CHAN_OUT_RATE_n_base + CANDAC16_CHAN_OUT_n_count,
};


#endif /* __CANDAC16_DRV_I_H */
