#ifndef __ADC1000_DRV_I_H
#define __ADC1000_DRV_I_H


// r1i3132674,r9i,w30i,r100i
enum
{
    /* 0-9: data */
    ADC1000_CHAN_DATA          = 0,

    ADC1000_CHAN_MARKER        = 9,

    /* 10-39: controls */
    ADC1000_CHAN_SHOT          = 10, 
    ADC1000_CHAN_STOP          = 11,
    ADC1000_CHAN_ISTART        = 12,
    ADC1000_CHAN_WAITTIME      = 13, 
    ADC1000_CHAN_CALIBRATE     = 14,
    ADC1000_CHAN_FGT_CLB       = 15,
    ADC1000_CHAN_VISIBLE_CLB   = 16,
    ADC1000_CHAN_CALC_STATS    = 17,

    ADC1000_CHAN_PTSOFS        = 20,
    ADC1000_CHAN_NUMPTS        = 21,
    ADC1000_CHAN_TIMING        = 22,
    ADC1000_CHAN_FRQDIV        = 23,
    ADC1000_CHAN_RANGE         = 24,
    // unused 25,26,27
    ADC1000_CHAN_TRIG_TYPE     = 28,
    ADC1000_CHAN_TRIG_INPUT    = 29,
// !!! Future "TRIG_LEVEL"?
// !!! Future "TRIG_MODE" (single/multi/recorder) and TRIG_MULTI_NUM?

    /* 40-139: status */
    ADC1000_CHAN_DEVICE_ID     = 40,  ADC1000_CHAN_INFO_first = ADC1000_CHAN_DEVICE_ID,
    ADC1000_CHAN_BASE_SW_VER   = 41,
    ADC1000_CHAN_PGA_SW_VER    = 42,
    ADC1000_CHAN_BASE_HW_VER   = 43,
    ADC1000_CHAN_PGA_HW_VER    = 44,
    ADC1000_CHAN_PGA_VAR       = 45,
    ADC1000_CHAN_BASE_UNIQ_ID  = 46,
    ADC1000_CHAN_PGA_UNIQ_ID   = 47,  ADC1000_CHAN_INFO_last  = ADC1000_CHAN_PGA_UNIQ_ID,
    // unused 48,49

    ADC1000_CHAN_ELAPSED       = 50,
    ADC1000_CHAN_CLB_STATE     = 51,
    ADC1000_CHAN_XS_PER_POINT  = 52,
    ADC1000_CHAN_XS_DIVISOR    = 53,
    ADC1000_CHAN_XS_FACTOR     = 54,
    ADC1000_CHAN_CUR_PTSOFS    = 55,
    ADC1000_CHAN_CUR_NUMPTS    = 56,
    ADC1000_CHAN_CUR_TIMING    = 57,
    ADC1000_CHAN_CUR_FRQDIV    = 58,
    // unused 59
    ADC1000_CHAN_CUR_RANGE     = 60,
    // unused 61,62,63
    ADC1000_CHAN_OVERFLOW      = 64,
    // unused 65,66,67
    // unused 68,69

    ADC1000_CHAN_ON            = 70,
    // unused 71,72,73
    ADC1000_CHAN_RANGEMIN      = 74,
    // unused 75,76,77
    ADC1000_CHAN_RANGEMAX      = 78,
    // unused 79,80,81

    ADC1000_CHAN_CLB_FAIL0     = 82,
    ADC1000_CHAN_CLB_FAIL1     = 83,
    ADC1000_CHAN_CLB_FAIL2     = 84,
    ADC1000_CHAN_CLB_FAIL3     = 85,
    ADC1000_CHAN_CLB_DYN0      = 86,
    ADC1000_CHAN_CLB_DYN1      = 87,
    ADC1000_CHAN_CLB_DYN2      = 88,
    ADC1000_CHAN_CLB_DYN3      = 89,
    ADC1000_CHAN_CLB_ZERO0     = 90,
    ADC1000_CHAN_CLB_ZERO1     = 91,
    ADC1000_CHAN_CLB_ZERO2     = 92,
    ADC1000_CHAN_CLB_ZERO3     = 93,
    ADC1000_CHAN_CLB_GAIN0     = 94,
    ADC1000_CHAN_CLB_GAIN1     = 95,
    ADC1000_CHAN_CLB_GAIN2     = 96,
    ADC1000_CHAN_CLB_GAIN3     = 97,
    // unused 98,99

    ADC1000_CHAN_MIN           = 100, ADC1000_CHAN_STATS_first = ADC1000_CHAN_MIN,
    ADC1000_CHAN_MAX           = 101,
    ADC1000_CHAN_AVG           = 102,
    ADC1000_CHAN_INT           = 103, ADC1000_CHAN_STATS_last = ADC1000_CHAN_INT,
    // unused 104-115

    ADC1000_CHAN_TOTALMIN      = 116,
    ADC1000_CHAN_TOTALMAX      = 117,
    ADC1000_CHAN_NUM_LINES     = 118,
    // unused MANY

    ADC1000_NUMCHANS = 140
};

enum
{
    ADC4X240_PGA_VAR_1CH = 0, // 1ch/1000MSPS
    ADC4X240_PGA_VAR_2CH = 1, // 2ch/500MSPS
    ADC4X240_PGA_VAR_4CH = 2, // 4ch/250MSPS
    ADC4X240_PGA_VAR_ERR = 3, // Check contact between baseboard and amplifier
};

/* General device specs */
enum
{
    ADC1000_NUM_ADCS   = 4, // ==NUM_LINES in ADC1000, but still ==4 in ADC1000
    ADC1000_NUM_LINES  = 1,
    ADC1000_MAX_NUMPTS = 3132674, // =3132692-18
};
enum
{
    ADC1000_MIN_VALUE = -32767, /*!!! Should check!!! */
    ADC1000_MAX_VALUE = +32767, 
};


#endif /* __ADC1000_DRV_I_H */
