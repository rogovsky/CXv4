#ifndef __CURVV_DRV_I_H
#define __CURVV_DRV_I_H


#include "drv_i/cankoz_common_drv_i.h"


// w8i,r24i,w24i,w1i,r1i,w1i,r1i,w8i,r16i,w1i,r2i,w13i,r10i
enum
{
    /* 0-59: Powerful/opticalless I/O registers */
    CURVV_CHAN_PWR_TTL_base    = 0, CURVV_CHAN_PWR_TTL_count = 60,

    CURVV_CHAN_PWR_Bn_base     = CURVV_CHAN_PWR_TTL_base + 0,  CURVV_CHAN_PWR_Bn_count = 8,
    CURVV_CHAN_TTL_Bn_base     = CURVV_CHAN_PWR_TTL_base + 8,  CURVV_CHAN_TTL_Bn_count = 24,
    CURVV_CHAN_TTL_IEn_base    = CURVV_CHAN_PWR_TTL_base + 32,
    CURVV_CHAN_PWR_8B          = CURVV_CHAN_PWR_TTL_base + 56,
    CURVV_CHAN_TTL_24B         = CURVV_CHAN_PWR_TTL_base + 57,
    CURVV_CHAN_TTL_IE24B       = CURVV_CHAN_PWR_TTL_base + 58,
    CURVV_CHAN_TTL_PWR_RSRVD59 = CURVV_CHAN_PWR_TTL_base + 59,

    /* 60-99: regs */
    CURVV_CHAN_REGS_base = 60,
    CURVV_CHAN_REGS_OUTRB_n_base = CURVV_CHAN_REGS_base + CANKOZ_IOREGS_OFS_OUTRB0,
    CURVV_CHAN_REGS_INPRB_n_base = CURVV_CHAN_REGS_base + CANKOZ_IOREGS_OFS_INPRB0,
    CURVV_CHAN_REGS_OUTR8B     = CURVV_CHAN_REGS_base + CANKOZ_IOREGS_OFS_OUTR8B,
    CURVV_CHAN_REGS_INPR8B     = CURVV_CHAN_REGS_base + CANKOZ_IOREGS_OFS_INPR8B,
    CURVV_CHAN_REGS_last       = CURVV_CHAN_REGS_base + CANKOZ_IOREGS_CHANCOUNT -1,
    CURVV_CHAN_REGS_RSVD_B     = CURVV_CHAN_REGS_base + CANKOZ_IOREGS_CHANCOUNT,
    CURVV_CHAN_REGS_RSVD_E     = 99,

    /* Diagnostic channels */
    CURVV_CHAN_HW_VER = 100,
    CURVV_CHAN_SW_VER = 101,

    CURVV_NUMCHANS = 110
};


#endif /* __CURVV_DRV_I_H */
