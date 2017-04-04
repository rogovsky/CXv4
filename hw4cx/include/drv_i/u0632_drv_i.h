#ifndef __U0632_DRV_I_H
#define __U0632_DRV_I_H


/* Note: U0632 is Repkov's KIPP */

// r30i32,r120i32,r90i,w60i,w50i,r50i
enum
{
    U0632_MAXUNITS       = 30,  // 15 boxes per line, 2 lines
    U0632_CELLS_PER_UNIT = 32,  // X and Y, 15 wires per X/Y, 2 "unused"

    U0632_CHAN_DATA_n_base     = 0,
    U0632_CHAN_BIAS_R0_n_base  = 30,
    U0632_CHAN_BIAS_R1_n_base  = 60,
    U0632_CHAN_BIAS_R2_n_base  = 90,
    U0632_CHAN_BIAS_R3_n_base  = 120,
    U0632_CHAN_ONLINE_n_base   = 150,
    U0632_CHAN_CUR_M_n_base    = 180,
    U0632_CHAN_CUR_P_n_base    = 210,
    U0632_CHAN_M_n_base        = 240,
    U0632_CHAN_P_n_base        = 270,

    U0632_CHAN_WR_base         = 300,
      U0632_CHANR_WR_count     = 50,

    U0632_CHAN_BIAS_GET        = U0632_CHAN_WR_base + 10,
    U0632_CHAN_BIAS_DROP       = U0632_CHAN_WR_base + 11,
    U0632_CHAN_BIAS_COUNT      = U0632_CHAN_WR_base + 12,

    U0632_CHAN_RD_base         = 350,
      U0632_CHANR_RD_count     = 50,

    U0632_CHAN_HW_VER          = U0632_CHAN_RD_base + 0,
    U0632_CHAN_SW_VER          = U0632_CHAN_RD_base + 1,

    U0632_CHAN_BIAS_IS_ON      = U0632_CHAN_RD_base + 10,
    U0632_CHAN_BIAS_STEPS_LEFT = U0632_CHAN_RD_base + 11,
    U0632_CHAN_BIAS_CUR_COUNT  = U0632_CHAN_RD_base + 12,

    U0632_CHAN_CONST32         = U0632_CHAN_RD_base + 20,

    U0632_NUMCHANS           = 400,
};

enum
{
    U0632_R_400  = 0,
    U0632_R_100  = 1,
    U0632_R_25   = 2,
    U0632_R_6_25 = 3,

    U0632_NUMRANGES = 4
};


#endif /* __U0632_DRV_I_H */
