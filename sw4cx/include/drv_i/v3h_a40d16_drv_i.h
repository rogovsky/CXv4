#ifndef __V3H_A40D16_DRV_I_H
#define __V3H_A40D16_DRV_I_H


#include "vdev_ps_conditions.h"


// w7,r13
enum
{
    V3H_A40D16_CHAN_WR_base        = 0,
      V3H_A40D16_CHAN_WR_count     = 7,

    V3H_A40D16_CHAN_ISET           = V3H_A40D16_CHAN_WR_base + 0,
    V3H_A40D16_CHAN_ISET_RATE      = V3H_A40D16_CHAN_WR_base + 1,
    V3H_A40D16_CHAN_SWITCH_ON      = V3H_A40D16_CHAN_WR_base + 2,
    V3H_A40D16_CHAN_SWITCH_OFF     = V3H_A40D16_CHAN_WR_base + 3,
    V3H_A40D16_CHAN_RESET_STATE    = V3H_A40D16_CHAN_WR_base + 4,
    V3H_A40D16_CHAN_RESET_C_ILKS   = V3H_A40D16_CHAN_WR_base + 5,
    V3H_A40D16_CHAN_RSRVD_WR6      = V3H_A40D16_CHAN_WR_base + 6,

    V3H_A40D16_CHAN_RD_base        = 7,
      V3H_A40D16_CHAN_RD_count     = 13,

    V3H_A40D16_CHAN_DMS            = V3H_A40D16_CHAN_RD_base + 0,
    V3H_A40D16_CHAN_STS            = V3H_A40D16_CHAN_RD_base + 1,
    V3H_A40D16_CHAN_MES            = V3H_A40D16_CHAN_RD_base + 2,
    V3H_A40D16_CHAN_MU1            = V3H_A40D16_CHAN_RD_base + 3,
    V3H_A40D16_CHAN_MU2            = V3H_A40D16_CHAN_RD_base + 4,
        
    V3H_A40D16_CHAN_ISET_CUR       = V3H_A40D16_CHAN_RD_base + 5,
    V3H_A40D16_CHAN_OPR            = V3H_A40D16_CHAN_RD_base + 6,
    V3H_A40D16_CHAN_ILK            = V3H_A40D16_CHAN_RD_base + 7,
    V3H_A40D16_CHAN_IS_ON          = V3H_A40D16_CHAN_RD_base + 8,
    V3H_A40D16_CHAN_C_ILK          = V3H_A40D16_CHAN_RD_base + 9,
    V3H_A40D16_CHAN_VDEV_CONDITION = V3H_A40D16_CHAN_RD_base + 10,
    V3H_A40D16_CHAN_RSRVD_RD11     = V3H_A40D16_CHAN_RD_base + 11,
    V3H_A40D16_CHAN_VDEV_STATE     = V3H_A40D16_CHAN_RD_base + 12,

    V3H_A40D16_NUMCHANS = 20
};


#endif /* __V3H_A40D16_DRV_I_H */
