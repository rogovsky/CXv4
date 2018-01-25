#ifndef __CDAC20_DRV_I_H
#define __CDAC20_DRV_I_H


#include "drv_i/kozdev_common_drv_i.h"


// w40i,r20i,w8i,r16i,w1i,r2i,w13i,r100i,w32i,w32i,r32i,r4i,w1i31,w1i31,w1i31,w96i,r1t100
enum
{
    CDAC20_CHAN_ADC_n_count = 8,
    CDAC20_CHAN_OUT_n_count = 1,

    CDAC20_CHAN_OUT_IMM_n_base = KOZDEV_CHAN_OUT_n_base      + CDAC20_CHAN_OUT_n_count,
    CDAC20_CHAN_OUT_TAC_n_base = KOZDEV_CHAN_OUT_RATE_n_base + CDAC20_CHAN_OUT_n_count,
};


#endif /* __CDAC20_DRV_I_H */
