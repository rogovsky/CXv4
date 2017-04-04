#ifndef __VSDC2_DRV_I_H
#define __VSDC2_DRV_I_H


// r10i,r10s,w20i,r2s2000,r8i,w20i,r30i
enum
{
    VSDC2_CHAN_HW_VER      = 0,
    VSDC2_CHAN_SW_VER      = 1,

    VSDC2_CHAN_GAIN_n_base = 2,
    VSDC2_CHAN_GAIN0       = VSDC2_CHAN_GAIN_n_base + 0,
    VSDC2_CHAN_GAIN1       = VSDC2_CHAN_GAIN_n_base + 1,

    VSDC2_CHAN_INT_n_base  = 10,
      VSDC2_CHAN_INT_n_count = 2,

    /* 20-39: reserved */

    /* 40-49: data */
    VSDC2_CHAN_LINE_n_base       = 40,
    VSDC2_CHAN_LINE0             = VSDC2_CHAN_LINE_n_base + 0,
    VSDC2_CHAN_LINE1             = VSDC2_CHAN_LINE_n_base + 1,

    /* 50-59: reserved for shot/stop/... */

    VSDC2_CHAN_GET_OSC_n_base    = 58,
    VSDC2_CHAN_GET_OSC0          = VSDC2_CHAN_GET_OSC_n_base + 0,
    VSDC2_CHAN_GET_OSC1          = VSDC2_CHAN_GET_OSC_n_base + 1,

    /* 60-69: controls */
    VSDC2_CHAN_PTSOFS_n_base     = 60,
    VSDC2_CHAN_PTSOFS0           = VSDC2_CHAN_PTSOFS_n_base + 0,
    VSDC2_CHAN_PTSOFS1           = VSDC2_CHAN_PTSOFS_n_base + 1,

    /* 70-??: status */
    VSDC2_CHAN_CUR_PTSOFS_n_base = 70,
    VSDC2_CHAN_CUR_PTSOFS0       = VSDC2_CHAN_CUR_PTSOFS_n_base + 0,
    VSDC2_CHAN_CUR_PTSOFS1       = VSDC2_CHAN_CUR_PTSOFS_n_base + 1,
    VSDC2_CHAN_CUR_NUMPTS_n_base = 72,
    VSDC2_CHAN_CUR_NUMPTS0       = VSDC2_CHAN_CUR_NUMPTS_n_base + 0,
    VSDC2_CHAN_CUR_NUMPTS1       = VSDC2_CHAN_CUR_NUMPTS_n_base + 1,
    VSDC2_CHAN_WRITE_ADDR_n_base = 74,
    VSDC2_CHAN_WRITE_ADDR0       = VSDC2_CHAN_WRITE_ADDR_n_base + 0,
    VSDC2_CHAN_WRITE_ADDR1       = VSDC2_CHAN_WRITE_ADDR_n_base + 1,

    VSDC2_NUMCHANS = 100
};

enum
{
    VSDC2_NUM_LINES  = 2,
    VSDC2_MAX_NUMPTS = 2000,
};
enum
{
    VSDC2_MIN_VALUE = -10, // -10.0
    VSDC2_MAX_VALUE = +10, // +10.0
};


#endif /* __VSDC2_DRV_I_H */
