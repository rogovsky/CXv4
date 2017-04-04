#ifndef __ADC200_DRV_I_H
#define __ADC200_DRV_I_H


// r1i256000,r2i128000,r7i,w20i,r50i
enum
{
    /* 0-9: data */
    ADC200_CHAN_DATA          = 0,
    ADC200_CHAN_LINE1         = 1,
    ADC200_CHAN_LINE2         = 2,

    ADC200_CHAN_MARKER        = 9,

    /* 10-29: controls */
    ADC200_CHAN_SHOT          = 10, 
    ADC200_CHAN_STOP          = 11,
    ADC200_CHAN_ISTART        = 12,
    ADC200_CHAN_WAITTIME      = 13, 
    ADC200_CHAN_CALC_STATS    = 14,

    ADC200_CHAN_PTSOFS        = 20,
    ADC200_CHAN_NUMPTS        = 21,
    ADC200_CHAN_TIMING        = 22,
    ADC200_CHAN_FRQDIV        = 23,
    ADC200_CHAN_RANGE1        = 24,
    ADC200_CHAN_RANGE2        = 25,
    ADC200_CHAN_ZERO1         = 26,
    ADC200_CHAN_ZERO2         = 27,

    /* 30-69: status */
    ADC200_CHAN_ELAPSED       = 30,
    ADC200_CHAN_XS_PER_POINT  = 31,
    ADC200_CHAN_XS_DIVISOR    = 32,
    ADC200_CHAN_XS_FACTOR     = 33,
    ADC200_CHAN_CUR_PTSOFS    = 34,
    ADC200_CHAN_CUR_NUMPTS    = 35,
    ADC200_CHAN_CUR_TIMING    = 36,
    ADC200_CHAN_CUR_FRQDIV    = 37,
    ADC200_CHAN_CUR_RANGE1    = 38,
    ADC200_CHAN_CUR_RANGE2    = 39,
    ADC200_CHAN_CUR_ZERO1     = 40,
    ADC200_CHAN_CUR_ZERO2     = 41,

    ADC200_CHAN_LINE1ON       = 50,
    ADC200_CHAN_LINE2ON       = 51,
    ADC200_CHAN_LINE1RANGEMIN = 52,
    ADC200_CHAN_LINE2RANGEMIN = 53,
    ADC200_CHAN_LINE1RANGEMAX = 54,
    ADC200_CHAN_LINE2RANGEMAX = 55,

    ADC200_CHAN_MIN1          = 60,  ADC200_CHAN_STATS_first = ADC200_CHAN_MIN1,
    ADC200_CHAN_MIN2          = 61,
    ADC200_CHAN_MAX1          = 62,
    ADC200_CHAN_MAX2          = 63,
    ADC200_CHAN_AVG1          = 64,
    ADC200_CHAN_AVG2          = 65,
    ADC200_CHAN_INT1          = 66,
    ADC200_CHAN_INT2          = 67,  ADC200_CHAN_STATS_last  = ADC200_CHAN_INT2,

    ADC200_CHAN_LINE1TOTALMIN = 70,
    ADC200_CHAN_LINE2TOTALMIN = 71,
    ADC200_CHAN_LINE1TOTALMAX = 72,
    ADC200_CHAN_LINE2TOTALMAX = 73,
    ADC200_CHAN_NUM_LINES     = 74,

    ADC200_NUMCHANS           = 80,
};


/* Frequency divisors */
enum
{
    ADC200_FRQD_5NS  = 0,
    ADC200_FRQD_10NS = 1,
    ADC200_FRQD_20NS = 2,
    ADC200_FRQD_40NS = 3
};

/* Timings */
enum
{
    ADC200_T_200MHZ   = 0,
    ADC200_T_195MHZ   = 1,
    ADC200_T_TIMER    = 2
};

/* Ranges */
enum
{
    ADC200_R_256  = 0,
    ADC200_R_512  = 1,
    ADC200_R_1024 = 2,
    ADC200_R_2048 = 3,
};

/* General device specs */
enum
{
    ADC200_NUM_LINES  = 2,
    ADC200_MAX_NUMPTS = 128000,
};
enum
{
    ADC200_MIN_VALUE = -1234567,
    ADC200_MAX_VALUE = +1234567,
};


#endif /* __ADC200_DRV_I_H */
