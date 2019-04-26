#ifndef __MPS20_CEAC124_DRV_I_H
#define __MPS20_CEAC124_DRV_I_H


#include "vdev_ps_conditions.h"


// w50i,r50i
enum
{
    MPS20_CEAC124_CHAN_WR_base      = 0,
      MPS20_CEAC124_CHAN_WR_count   = 50,

    MPS20_CEAC124_CHAN_ISET           = MPS20_CEAC124_CHAN_WR_base + 0,
    MPS20_CEAC124_CHAN_ISET_RATE      = MPS20_CEAC124_CHAN_WR_base + 1,
    MPS20_CEAC124_CHAN_SWITCH_ON      = MPS20_CEAC124_CHAN_WR_base + 2,
    MPS20_CEAC124_CHAN_SWITCH_OFF     = MPS20_CEAC124_CHAN_WR_base + 3,
    //
    MPS20_CEAC124_CHAN_RESET_STATE    = MPS20_CEAC124_CHAN_WR_base + 5,
    MPS20_CEAC124_CHAN_RESET_C_ILKS   = MPS20_CEAC124_CHAN_WR_base + 10,
    MPS20_CEAC124_CHAN_DISABLE        = MPS20_CEAC124_CHAN_WR_base + 11,

    MPS20_CEAC124_CHAN_RD_base      = 50,
      MPS20_CEAC124_CHAN_RD_count   = 50,

    MPS20_CEAC124_CHAN_IMES           = MPS20_CEAC124_CHAN_RD_base + 0,
    MPS20_CEAC124_CHAN_UMES           = MPS20_CEAC124_CHAN_RD_base + 1,
    /* +2..+15 -- reserved for CEAC124 ADC channels */

    MPS20_CEAC124_CHAN_ISET_CUR       = MPS20_CEAC124_CHAN_RD_base + 16,

    MPS20_CEAC124_CHAN_ILK            = MPS20_CEAC124_CHAN_RD_base + 17,
    MPS20_CEAC124_CHAN_OPR_S1         = MPS20_CEAC124_CHAN_RD_base + 18,
    MPS20_CEAC124_CHAN_OPR_S2         = MPS20_CEAC124_CHAN_RD_base + 19,
    MPS20_CEAC124_CHAN_INPRB3         = MPS20_CEAC124_CHAN_RD_base + 20,

    MPS20_CEAC124_CHAN_IS_ON          = MPS20_CEAC124_CHAN_RD_base + 21,

    MPS20_CEAC124_CHAN_C_ILK          = MPS20_CEAC124_CHAN_RD_base + 22,

    MPS20_CEAC124_CHAN_VDEV_CONDITION = 97,
    MPS20_CEAC124_CHAN_VDEV_STATE     = 99,

    MPS20_CEAC124_NUMCHANS = 100,
};


#endif /* __MPS20_CEAC124_DRV_I_H */
