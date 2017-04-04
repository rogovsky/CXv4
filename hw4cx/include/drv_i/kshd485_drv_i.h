#ifndef __KSHD485_DRV_I_H
#define __KSHD485_DRV_I_H


// r20i,w80i
enum
{
    KSHD485_RD_CHAN_base = 0,
      KSHD485_RD_CHAN_count = 20,
    KSHD485_CHAN_DEV_VERSION       = KSHD485_RD_CHAN_base + 0,
    KSHD485_CHAN_DEV_SERIAL        = KSHD485_RD_CHAN_base + 1,
    KSHD485_CHAN_STEPS_LEFT        = KSHD485_RD_CHAN_base + 2,
    KSHD485_CHAN_STATUS_BYTE       = KSHD485_RD_CHAN_base + 3,
    KSHD485_CHAN_STATUS_base       = KSHD485_RD_CHAN_base + 4,
    KSHD485_CHAN_STATUS_READY        = KSHD485_CHAN_STATUS_base + 0,
    KSHD485_CHAN_STATUS_GOING        = KSHD485_CHAN_STATUS_base + 1,
    KSHD485_CHAN_STATUS_KM           = KSHD485_CHAN_STATUS_base + 2,
    KSHD485_CHAN_STATUS_KP           = KSHD485_CHAN_STATUS_base + 3,
    KSHD485_CHAN_STATUS_SENSOR       = KSHD485_CHAN_STATUS_base + 4,
    KSHD485_CHAN_STATUS_PREC         = KSHD485_CHAN_STATUS_base + 5,
    KSHD485_CHAN_STATUS_WAS_K        = KSHD485_CHAN_STATUS_base + 6,
    KSHD485_CHAN_STATUS_BC           = KSHD485_CHAN_STATUS_base + 7,

    KSHD485_WR_CHAN_base = 20,
      KSHD485_WR_CHAN_count = 80,
    KSHD485_CONFIG_CHAN_base       = KSHD485_WR_CHAN_base + 0, // Note: following 4 channels go in the order of data in GET_CONFIG packet
    KSHD485_CHAN_WORK_CURRENT        = KSHD485_CONFIG_CHAN_base + 0,
    KSHD485_CHAN_HOLD_CURRENT        = KSHD485_CONFIG_CHAN_base + 1,
    KSHD485_CHAN_HOLD_DELAY          = KSHD485_CONFIG_CHAN_base + 2,
    KSHD485_CHAN_CONFIG_BYTE         = KSHD485_CONFIG_CHAN_base + 3,
    KSHD485_SPEEDS_CHAN_base       = KSHD485_WR_CHAN_base + 4, // Note: following 2 channels go in the order of data in GET_SPEEDS packet
    KSHD485_CHAN_MIN_VELOCITY        = KSHD485_SPEEDS_CHAN_base + 0,
    KSHD485_CHAN_MAX_VELOCITY        = KSHD485_SPEEDS_CHAN_base + 1,
    KSHD485_CHAN_ACCELERATION        = KSHD485_SPEEDS_CHAN_base + 2,
    KSHD485_CHAN_NUM_STEPS         = KSHD485_WR_CHAN_base + 7,
    KSHD485_CHAN_STOPCOND_BYTE     = KSHD485_WR_CHAN_base + 8,
    KSHD485_CHAN_ABSCOORD          = KSHD485_WR_CHAN_base + 9,

    KSHD485_CHAN_CONFIG_BIT_base   = KSHD485_WR_CHAN_base + 20,
    KSHD485_CHAN_CONFIG_BIT_HALF     = KSHD485_CHAN_CONFIG_BIT_base + 0,
    KSHD485_CHAN_CONFIG_BIT_1        = KSHD485_CHAN_CONFIG_BIT_base + 1,
    KSHD485_CHAN_CONFIG_BIT_KM       = KSHD485_CHAN_CONFIG_BIT_base + 2,
    KSHD485_CHAN_CONFIG_BIT_KP       = KSHD485_CHAN_CONFIG_BIT_base + 3,
    KSHD485_CHAN_CONFIG_BIT_SENSOR   = KSHD485_CHAN_CONFIG_BIT_base + 4,
    KSHD485_CHAN_CONFIG_BIT_SOFTK    = KSHD485_CHAN_CONFIG_BIT_base + 5,
    KSHD485_CHAN_CONFIG_BIT_LEAVEK   = KSHD485_CHAN_CONFIG_BIT_base + 6,
    KSHD485_CHAN_CONFIG_BIT_ACCLEAVE = KSHD485_CHAN_CONFIG_BIT_base + 7,

    KSHD485_CHAN_STOPCOND_BIT_base = KSHD485_WR_CHAN_base + 28,
    KSHD485_CHAN_STOPCOND_BIT_0      = KSHD485_CHAN_STOPCOND_BIT_base + 0,
    KSHD485_CHAN_STOPCOND_BIT_1      = KSHD485_CHAN_STOPCOND_BIT_base + 1,
    KSHD485_CHAN_STOPCOND_BIT_KP     = KSHD485_CHAN_STOPCOND_BIT_base + 2,
    KSHD485_CHAN_STOPCOND_BIT_KM     = KSHD485_CHAN_STOPCOND_BIT_base + 3,
    KSHD485_CHAN_STOPCOND_BIT_S0     = KSHD485_CHAN_STOPCOND_BIT_base + 4,
    KSHD485_CHAN_STOPCOND_BIT_S1     = KSHD485_CHAN_STOPCOND_BIT_base + 5,
    KSHD485_CHAN_STOPCOND_BIT_6      = KSHD485_CHAN_STOPCOND_BIT_base + 6,
    KSHD485_CHAN_STOPCOND_BIT_7      = KSHD485_CHAN_STOPCOND_BIT_base + 7,

    KSHD485_CM_CHAN_base = 80,
    KSHD485_CHAN_GO                = KSHD485_CM_CHAN_base + 0,
    KSHD485_CHAN_GO_WO_ACCEL       = KSHD485_CM_CHAN_base + 1,
    KSHD485_CHAN_STOP              = KSHD485_CM_CHAN_base + 2,
    KSHD485_CHAN_SWITCHOFF         = KSHD485_CM_CHAN_base + 3,
    KSHD485_CHAN_GO_N_STEPS        = KSHD485_CM_CHAN_base + 4,
    KSHD485_CHAN_GO_WOA_N_STEPS    = KSHD485_CM_CHAN_base + 5,

    KSHD485_CHAN_DO_RESET          = 98, // = KSHD485_CM_CHAN_base + 18
    KSHD485_CHAN_SAVE_CONFIG       = 99, // = KSHD485_CM_CHAN_base + 19

    KSHD485_NUMCHANS    = 100
};


#endif /* __KSHD485_DRV_I_H */
