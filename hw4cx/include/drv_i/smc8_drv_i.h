#ifndef __SMC8_DRV_I_H
#define __SMC8_DRV_I_H


enum
{
    SMC8_NUMLINES = 8,
};


// r64i,w136i
enum
{
    SMC8_g_GOING        = 0,
    SMC8_g_STATE        = 1,
    SMC8_g_KM           = 2,
    SMC8_g_KP           = 3,
    SMC8_g_COUNTER      = 4,
    SMC8_g_5            = 5,
    SMC8_g_6            = 6,
    SMC8_g_7            = 7,
    SMC8_g_STOP         = 8,
    SMC8_g_GO           = 9,
    SMC8_g_GO_N_STEPS   = 10,
    SMC8_g_RST_COUNTER  = 11,
    SMC8_g_START_FREQ   = 12,
    SMC8_g_FINAL_FREQ   = 13,
    SMC8_g_ACCELERATION = 14,
    SMC8_g_NUM_STEPS    = 15,
    SMC8_g_OUT_MD       = 16, SMC8_g_CONTROL_FIRST = SMC8_g_OUT_MD, // Note: following gropus are in CONTROL_REG bits order
    SMC8_g_MASK_M       = 17,
    SMC8_g_MASK_P       = 18,
    SMC8_g_TYPE_M       = 19,
    SMC8_g_TYPE_P       = 20, SMC8_g_CONTROL_LAST  = SMC8_g_TYPE_P,
    SMC8_g_21           = 21,
    SMC8_g_22           = 22,
    SMC8_g_23           = 23,
    SMC8_g_24           = 24,

    SMC8_CHAN_GOING_base        = SMC8_g_GOING        * SMC8_NUMLINES,
    SMC8_CHAN_STATE_base        = SMC8_g_STATE        * SMC8_NUMLINES,
    SMC8_CHAN_KM_base           = SMC8_g_KM           * SMC8_NUMLINES,
    SMC8_CHAN_KP_base           = SMC8_g_KP           * SMC8_NUMLINES,
    SMC8_CHAN_COUNTER_base      = SMC8_g_COUNTER      * SMC8_NUMLINES,
    SMC8_CHAN_STOP_base         = SMC8_g_STOP         * SMC8_NUMLINES,
    SMC8_CHAN_GO_base           = SMC8_g_GO           * SMC8_NUMLINES,
    SMC8_CHAN_GO_N_STEPS_base   = SMC8_g_GO_N_STEPS   * SMC8_NUMLINES,
    SMC8_CHAN_RST_COUNTER_base  = SMC8_g_RST_COUNTER  * SMC8_NUMLINES,
    SMC8_CHAN_START_FREQ_base   = SMC8_g_START_FREQ   * SMC8_NUMLINES,
    SMC8_CHAN_FINAL_FREQ_base   = SMC8_g_FINAL_FREQ   * SMC8_NUMLINES,
    SMC8_CHAN_ACCELERATION_base = SMC8_g_ACCELERATION * SMC8_NUMLINES,
    SMC8_CHAN_NUM_STEPS_base    = SMC8_g_NUM_STEPS    * SMC8_NUMLINES,
    SMC8_CHAN_OUT_MD_base       = SMC8_g_OUT_MD       * SMC8_NUMLINES,
    SMC8_CHAN_MASK_M_base       = SMC8_g_MASK_M       * SMC8_NUMLINES,
    SMC8_CHAN_MASK_P_base       = SMC8_g_MASK_P       * SMC8_NUMLINES,
    SMC8_CHAN_TYPE_M_base       = SMC8_g_TYPE_M       * SMC8_NUMLINES,
    SMC8_CHAN_TYPE_P_base       = SMC8_g_TYPE_P       * SMC8_NUMLINES,

    SMC8_CHAN_HW_VER = 198,
    SMC8_CHAN_SW_VER = 199,

    SMC8_NUMCHANS = 200,
};

enum
{
    SMC8_STATE_STILL       = 0,  // 0b0000
    SMC8_STATE_LIMIT       = 1,  // 0b0001
    SMC8_STATE_STOPPED     = 2,  // 0b0010
    SMC8_STATE_ERRPARAMS   = 3,  // 0b0011

    SMC8_STATE_STARTING    = 8,  // 0b1000
    SMC8_STATE_ACCEL       = 9,  // 0b1001
    SMC8_STATE_CONSTANT    = 10, // 0b1010
    SMC8_STATE_DECEL       = 11, // 0b1011
    SMC8_STATE_STOPPING    = 12, // 0b1100

    SMC8_STATE_MOVING_mask = 8
};


#endif /* __SMC8_DRV_I_H */
