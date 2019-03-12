#ifndef __FROLOV_D16_DRV_I_H
#define __FROLOV_D16_DRV_I_H


// w40i,w10d,r50i,w100
enum
{
    FROLOV_D16_NUMOUTPUTS      = 4,
    
    FROLOV_D16_CHAN_A_n_base   = 0,
    FROLOV_D16_CHAN_B_n_base   = 4,
    // Ex-_V_n_base   = 8,
    FROLOV_D16_CHAN_OFF_n_base = 12,
    FROLOV_D16_CHAN_ALLOFF     = 16, // This chan MUST follow individual offs
    FROLOV_D16_CHAN_DO_SHOT    = 17,

    FROLOV_D16_CHAN_KSTART     = 18,
    FROLOV_D16_CHAN_KCLK_N     = 19,
    FROLOV_D16_CHAN_FCLK_SEL   = 20,
    FROLOV_D16_CHAN_START_SEL  = 21,
    FROLOV_D16_CHAN_MODE       = 22,
    FROLOV_D16_CHAN_UNUSED23   = 23,
    FROLOV_D16_CHAN_UNUSED24   = 24,
    FROLOV_D16_CHAN_ONES_STOP  = 25,

    FROLOV_D16_CHAN_AUTO_SHOT  = 26,
    FROLOV_D16_CHAN_AUTO_SECS  = 27,

    FROLOV_D16_CHAN_ACTIVATE_PRESET_N = 39,

    FROLOV_D16_DOUBLE_CHAN_base = 40, FROLOV_D16_DOUBLE_CHAN_count = 10,

    FROLOV_D16_CHAN_V_n_base   = 40,  // double
    FROLOV_D16_CHAN_F_IN_HZ    = 48,  // double
    FROLOV_D16_CHAN_F_IN_NS    = 49,  // double

    FROLOV_D16_CHAN_WAS_START  = 61,
    FROLOV_D16_CHAN_LAM_SIG    = 62,

    FROLOV_D16_CHAN_PRESETS_base = 100,
      FROLOV_D16_CHAN_PRESETS_per_one = 20,
      FROLOV_D16_CHAN_PRESETS_num     = 4,
    FROLOV_D16_CHAN_PRESETS_ofs_A_n_base   = 0,
    FROLOV_D16_CHAN_PRESETS_ofs_B_n_base   = 4,
    FROLOV_D16_CHAN_PRESETS_ofs_OFF_n_base = 8,
    FROLOV_D16_CHAN_PRESETS_ofs_ALLOFF     = 12,

    FROLOV_D16_NUMCHANS        = 200
};

enum
{
    FROLOV_D16_V_KCLK_1          = 0,
    FROLOV_D16_V_KCLK_2          = 1,
    FROLOV_D16_V_KCLK_4          = 2,
    FROLOV_D16_V_KCLK_8          = 3,

    FROLOV_D16_V_FCLK_FIN        = 0,
    FROLOV_D16_V_FCLK_QUARTZ     = 1,
    FROLOV_D16_V_FCLK_IMPACT     = 2,

    FROLOV_D16_V_START_START     = 0,
    FROLOV_D16_V_START_Y1        = 1,
    FROLOV_D16_V_START_50HZ      = 2,

    FROLOV_D16_V_MODE_CONTINUOUS = 0,
    FROLOV_D16_V_MODE_ONESHOT    = 1,
};


#endif /* __FROLOV_D16_DRV_I_H */
