#ifndef __CPKS8_DRV_I_H
#define __CPKS8_DRV_I_H


#include "drv_i/cankoz_common_drv_i.h"


// w16i,r8i,r2i
enum
{
    CPKS8_CHAN_OUT_n_base      = 0, CPKS8_CHAN_OUT_n_count = 8,
    CPKS8_CHAN_OUT_RATE_n_base = 8,
    CPKS8_CHAN_OUT_CUR_n_base  = 16,

    CPKS8_CHAN_HW_VER          = 24,
    CPKS8_CHAN_SW_VER          = 25,

    CPKS8_NUMCHANS         = 26
};


#endif /* __CPKS8_DRV_I_H */
