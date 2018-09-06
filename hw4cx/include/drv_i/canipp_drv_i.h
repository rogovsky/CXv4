#ifndef __CANIPP_DRV_I_H
#define __CANIPP_DRV_I_H


/* Note: channel map is identical to U0632 (KIPP) */

// r30i32,r120i32,r90i,w60i,w50i,r50i
enum
{
    CANIPP_MAXUNITS       = 30,  // 15 boxes per line, 2 lines
    CANIPP_CELLS_PER_UNIT = 32,  // X and Y, 15 wires per X/Y, 2 "unused"

    CANIPP_CHAN_DATA_n_base     = 0,
    CANIPP_CHAN_BIAS_R0_n_base  = 30,
    CANIPP_CHAN_BIAS_R1_n_base  = 60,
    CANIPP_CHAN_BIAS_R2_n_base  = 90,
    CANIPP_CHAN_BIAS_R3_n_base  = 120,
    CANIPP_CHAN_ONLINE_n_base   = 150,
    CANIPP_CHAN_CUR_M_n_base    = 180,
    CANIPP_CHAN_CUR_P_n_base    = 210,
    CANIPP_CHAN_M_n_base        = 240,
    CANIPP_CHAN_P_n_base        = 270,

    CANIPP_CHAN_WR_base         = 300,
      CANIPP_CHANR_WR_count     = 50,

    CANIPP_CHAN_BIAS_GET        = CANIPP_CHAN_WR_base + 10,
    CANIPP_CHAN_BIAS_DROP       = CANIPP_CHAN_WR_base + 11,
    CANIPP_CHAN_BIAS_COUNT      = CANIPP_CHAN_WR_base + 12,

    CANIPP_CHAN_RD_base         = 350,
      CANIPP_CHANR_RD_count     = 50,

    CANIPP_CHAN_HW_VER          = CANIPP_CHAN_RD_base + 0,
    CANIPP_CHAN_SW_VER          = CANIPP_CHAN_RD_base + 1,

    CANIPP_CHAN_BIAS_IS_ON      = CANIPP_CHAN_RD_base + 10,
    CANIPP_CHAN_BIAS_STEPS_LEFT = CANIPP_CHAN_RD_base + 11,
    CANIPP_CHAN_BIAS_CUR_COUNT  = CANIPP_CHAN_RD_base + 12,

    CANIPP_CHAN_CONST32         = CANIPP_CHAN_RD_base + 20,

    CANIPP_NUMCHANS           = 400,
};

enum
{
    CANIPP_R_400  = 0,
    CANIPP_R_100  = 1,
    CANIPP_R_25   = 2,
    CANIPP_R_6_25 = 3,

    CANIPP_NUMRANGES = 4
};


#endif /* __CANIPP_DRV_I_H */
