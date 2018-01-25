#ifndef __ADC4_DRV_I_H
#define __ADC4_DRV_I_H


// r1i131044,r4i32761,r5i,w20i,r70i
enum
{
    /* 0-9: data */
    ADC4_CHAN_DATA  = 0,
    ADC4_CHAN_LINE0 = 1,
    ADC4_CHAN_LINE1 = 2,
    ADC4_CHAN_LINE2 = 3,
    ADC4_CHAN_LINE3 = 4,

    ADC4_CHAN_MARKER = 9,

    /* 10-29: controls */
    ADC4_CHAN_SHOT          = 10, 
    ADC4_CHAN_STOP          = 11,
    ADC4_CHAN_ISTART        = 12,
    ADC4_CHAN_WAITTIME      = 13, 
    ADC4_CHAN_CALC_STATS    = 14,

    ADC4_CHAN_PTSOFS        = 20,
    ADC4_CHAN_NUMPTS        = 21,
    ADC4_CHAN_ZERO0         = 22,
    ADC4_CHAN_ZERO1         = 23,
    ADC4_CHAN_ZERO2         = 24,
    ADC4_CHAN_ZERO3         = 25,

    /* 30-99: status */
    ADC4_CHAN_ELAPSED       = 30,
    ADC4_CHAN_XS_PER_POINT  = 31,
    ADC4_CHAN_XS_DIVISOR    = 32,
    ADC4_CHAN_XS_FACTOR     = 33,
    ADC4_CHAN_CUR_PTSOFS    = 34,
    ADC4_CHAN_CUR_NUMPTS    = 35,
    ADC4_CHAN_CUR_ZERO0     = 36,
    ADC4_CHAN_CUR_ZERO1     = 37,
    ADC4_CHAN_CUR_ZERO2     = 38,
    ADC4_CHAN_CUR_ZERO3     = 39,

    ADC4_CHAN_LINE0ON       = 50,
    ADC4_CHAN_LINE1ON       = 51,
    ADC4_CHAN_LINE2ON       = 52,
    ADC4_CHAN_LINE3ON       = 53,
    ADC4_CHAN_LINE0RANGEMIN = 54,
    ADC4_CHAN_LINE1RANGEMIN = 55,
    ADC4_CHAN_LINE2RANGEMIN = 56,
    ADC4_CHAN_LINE3RANGEMIN = 57,
    ADC4_CHAN_LINE0RANGEMAX = 58,
    ADC4_CHAN_LINE1RANGEMAX = 59,
    ADC4_CHAN_LINE2RANGEMAX = 60,
    ADC4_CHAN_LINE3RANGEMAX = 61,

    ADC4_CHAN_MIN0          = 70, ADC4_CHAN_STATS_first = ADC4_CHAN_MIN0,
    ADC4_CHAN_MIN1          = 71,
    ADC4_CHAN_MIN2          = 72,
    ADC4_CHAN_MIN3          = 73,
    ADC4_CHAN_MAX0          = 74,
    ADC4_CHAN_MAX1          = 75,
    ADC4_CHAN_MAX2          = 76,
    ADC4_CHAN_MAX3          = 77,
    ADC4_CHAN_AVG0          = 78,
    ADC4_CHAN_AVG1          = 79,
    ADC4_CHAN_AVG2          = 80,
    ADC4_CHAN_AVG3          = 81,
    ADC4_CHAN_INT0          = 82,
    ADC4_CHAN_INT1          = 83,
    ADC4_CHAN_INT2          = 84,
    ADC4_CHAN_INT3          = 85, ADC4_CHAN_STATS_last = ADC4_CHAN_INT3,

    ADC4_CHAN_LINE0TOTALMIN = 86,
    ADC4_CHAN_LINE1TOTALMIN = 87,
    ADC4_CHAN_LINE2TOTALMIN = 88,
    ADC4_CHAN_LINE3TOTALMIN = 89,
    ADC4_CHAN_LINE0TOTALMAX = 90,
    ADC4_CHAN_LINE1TOTALMAX = 91,
    ADC4_CHAN_LINE2TOTALMAX = 92,
    ADC4_CHAN_LINE3TOTALMAX = 93,
    ADC4_CHAN_NUM_LINES     = 94,

    ADC4_NUMCHANS           = 100,
};


/* General device specs */
enum
{
    ADC4_NUM_LINES  = 4,
    ADC4_JUNK_MSMTS = 6,
    ADC4_MAX_NUMPTS = 32767 - ADC4_JUNK_MSMTS, // 32K words per channel, minus 6 words of junk
};
enum
{
    ADC4_MIN_VALUE = -2048,
    ADC4_MAX_VALUE = +2047,
};


#endif /* __ADC4_DRV_I_H */
