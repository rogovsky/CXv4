#ifndef __DDS500_DRV_I_H
#define __DDS500_DRV_I_H


// v2: w30,r20 v4:w30i,r20i
enum
{
    DDS500_CHAN_WR_base = 0,

    DDS500_CHAN_AMP_n_base        = DDS500_CHAN_WR_base + 0,
    DDS500_CHAN_PHS_n_base        = DDS500_CHAN_WR_base + 2,
    DDS500_CHAN_FTW_n_base        = DDS500_CHAN_WR_base + 4,

    DDS500_CHAN_USE_SINE_n_base   = DDS500_CHAN_WR_base + 6,

    DDS500_CHAN_DLY_n_base        = DDS500_CHAN_WR_base + 18,
//    DDS500_CHAN__n_base = DDS500_CHAN_WR_base + ,
//    DDS500_CHAN__n_base = DDS500_CHAN_WR_base + ,

    DDS500_CHAN_ENABLE_DELAYS     = DDS500_CHAN_WR_base + 20,

    DDS500_CHAN_CLR_PHASE         = DDS500_CHAN_WR_base + 21,

    DDS500_CHAN_DEBUG_REG_N       = DDS500_CHAN_WR_base + 27,
    DDS500_CHAN_DEBUG_REG_W       = DDS500_CHAN_WR_base + 28,

    DDS500_CHAN_RESET_TO_DEFAULTS = DDS500_CHAN_WR_base + 29,

    DDS500_CHAN_RD_base = 30,

    DDS500_CHAN_SERIAL            = DDS500_CHAN_RD_base + 10,
    DDS500_CHAN_FIRMWARE          = DDS500_CHAN_RD_base + 11,
    
    DDS500_CHAN_DEBUG_REG_R       = DDS500_CHAN_RD_base + 19,

    DDS500_NUMCHANS               = 50
};

enum
{
    DDS500_V_RESET_TO_DEFAULTS_KEYVAL = 'R' + ('s' << 8) + ('e' << 16) + ('t' << 24),
};


#endif /* __DDS500_DRV_I_H */
