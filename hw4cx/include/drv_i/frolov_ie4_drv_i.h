#ifndef __FROLOV_IE4_DRV_I_H
#define __FROLOV_IE4_DRV_I_H


// w40i,w10d,r50i
enum
{
    FROLOV_IE4_NUMOUTPUTS      = 4,

    // 0...49: write

    FROLOV_IE4_CHAN_A_n_base   = 0,
    FROLOV_IE4_CHAN_K_n_base   = 4,
    // Ex-_CHAN_V_n_base = 8,
    FROLOV_IE4_CHAN_OFF_n_base = 12,
    FROLOV_IE4_CHAN_ALLOFF     = 16,
    // 17?
    // 18?
    FROLOV_IE4_CHAN_KCLK_N     = 19,
    FROLOV_IE4_CHAN_FCLK_SEL   = 20,
    FROLOV_IE4_CHAN_START_SEL  = 21,
    FROLOV_IE4_CHAN_MODE       = 22,
    FROLOV_IE4_CHAN_UNUSED23   = 23,

    FROLOV_IE4_CHAN_RE_BUM     = 30,
    FROLOV_IE4_CHAN_BUM_START  = 31,
    FROLOV_IE4_CHAN_BUM_STOP   = 32,

    FROLOV_IE4_DOUBLE_CHAN_base = 40, FROLOV_IE4_DOUBLE_CHAN_count = 10,

    FROLOV_IE4_CHAN_V_n_base   = 40,  // double
    FROLOV_IE4_CHAN_F_IN_HZ    = 48,  // double
    FROLOV_IE4_CHAN_F_IN_NS    = 49,  // double

    // 50...99: read

    FROLOV_IE4_CHAN_IE_BUM     = 60,
    FROLOV_IE4_CHAN_BUM_GOING  = 61,
    FROLOV_IE4_CHAN_LAM_SIG    = 62,

    FROLOV_IE4_NUMCHANS       = 100
};

enum
{
    FROLOV_IE4_V_KCLK_1          = 0,
    FROLOV_IE4_V_KCLK_2          = 1,
    FROLOV_IE4_V_KCLK_4          = 2,
    FROLOV_IE4_V_KCLK_8          = 3,

    FROLOV_IE4_V_FCLK_FIN        = 0,
    FROLOV_IE4_V_FCLK_QUARTZ_A   = 1,
    FROLOV_IE4_V_FCLK_BLOCKED    = 2,
    FROLOV_IE4_V_FCLK_QUARTZ_B   = 3,

    FROLOV_IE4_V_START_START     = 0,
    FROLOV_IE4_V_START_Y1        = 1,
    FROLOV_IE4_V_START_50HZ      = 2,

    FROLOV_IE4_V_MODE_CONTINUOUS = 0,
    FROLOV_IE4_V_MODE_BUM        = 1,
};


#endif /* __FROLOV_IE4_DRV_I_H */
