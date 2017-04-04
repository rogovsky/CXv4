#ifndef __ADC502_DRV_I_H
#define __ADC502_DRV_I_H


// r1i256000,r2i128000,r7i,w20i,r50i
enum
{
    /* 0-9: data */
    ADC502_CHAN_DATA          = 0,
    ADC502_CHAN_LINE1         = 1,
    ADC502_CHAN_LINE2         = 2,

    ADC502_CHAN_MARKER        = 9,

    /* 10-29: controls */
    ADC502_CHAN_SHOT          = 10, 
    ADC502_CHAN_STOP          = 11,
    ADC502_CHAN_ISTART        = 12,
    ADC502_CHAN_WAITTIME      = 13, 
    ADC502_CHAN_CALC_STATS    = 14,

    ADC502_CHAN_PTSOFS        = 20,
    ADC502_CHAN_NUMPTS        = 21,
    ADC502_CHAN_TIMING        = 22,
    ADC502_CHAN_TMODE         = 23,
    ADC502_CHAN_RANGE1        = 24,
    ADC502_CHAN_RANGE2        = 25,
    ADC502_CHAN_SHIFT1        = 26,
    ADC502_CHAN_SHIFT2        = 27,

    /* 30-69: status */
    ADC502_CHAN_ELAPSED       = 30,
    ADC502_CHAN_XS_PER_POINT  = 31,
    ADC502_CHAN_XS_DIVISOR    = 32,
    ADC502_CHAN_XS_FACTOR     = 33,
    ADC502_CHAN_CUR_PTSOFS    = 34,
    ADC502_CHAN_CUR_NUMPTS    = 35,
    ADC502_CHAN_CUR_TIMING    = 36,
    ADC502_CHAN_CUR_TMODE     = 37,
    ADC502_CHAN_CUR_RANGE1    = 38,
    ADC502_CHAN_CUR_RANGE2    = 39,
    ADC502_CHAN_CUR_SHIFT1    = 40,
    ADC502_CHAN_CUR_SHIFT2    = 41,

    ADC502_CHAN_LINE1ON       = 50,
    ADC502_CHAN_LINE2ON       = 51,
    ADC502_CHAN_LINE1RANGEMIN = 52,
    ADC502_CHAN_LINE2RANGEMIN = 53,
    ADC502_CHAN_LINE1RANGEMAX = 54,
    ADC502_CHAN_LINE2RANGEMAX = 55,

    ADC502_CHAN_MIN1          = 60,  ADC502_CHAN_STATS_first = ADC502_CHAN_MIN1,
    ADC502_CHAN_MIN2          = 61,
    ADC502_CHAN_MAX1          = 62,
    ADC502_CHAN_MAX2          = 63,
    ADC502_CHAN_AVG1          = 64,
    ADC502_CHAN_AVG2          = 65,
    ADC502_CHAN_INT1          = 66,
    ADC502_CHAN_INT2          = 67,  ADC502_CHAN_STATS_last  = ADC502_CHAN_INT2,

    ADC502_CHAN_LINE1TOTALMIN = 70,
    ADC502_CHAN_LINE2TOTALMIN = 71,
    ADC502_CHAN_LINE1TOTALMAX = 72,
    ADC502_CHAN_LINE2TOTALMAX = 73,
    ADC502_CHAN_NUM_LINES     = 74,

    ADC502_NUMCHANS           = 80,
};


/* Timings */
enum
{
    ADC502_TIMING_INT = 0,
    ADC502_TIMING_EXT = 1,
};

enum
{
    ADC502_TMODE_HF = 0,
    ADC502_TMODE_LF = 1,
};

/* Ranges */
enum
{
    ADC502_RANGE_8192  = 0,
    ADC502_RANGE_4096  = 1,
    ADC502_RANGE_2048  = 2,
    ADC502_RANGE_1024  = 3,
};

enum
{
    ADC502_SHIFT_NONE = 0,
    ADC502_SHIFT_NEG4 = 1,
    ADC502_SHIFT_POS4 = 2,
};

/* General device specs */
enum
{
    ADC502_NUM_LINES  = 2,
    ADC502_MAX_NUMPTS = 65534*0+32767, // "6553*4*" -- because 65535=0xFFFF, which is forbidden for write into addr-ctr
};
enum
{
    ADC502_MIN_VALUE= -12002918, // ((    0 - 3072)*39072)/10
    ADC502_MAX_VALUE= +11999011, // ((0xFFF - 1024)*39072)/10
};


#endif /* __ADC502_DRV_I_H */
