#ifndef __F4226_DRV_I_H
#define __F4226_DRV_I_H


// r1i1024,r9i,w20i,r20i
enum
{
    /* 0-9: data */
    F4226_CHAN_DATA          = 0,

    F4226_CHAN_MARKER        = 9,  // Note: just for compatibility with others

    /* 10-29: controls */
    F4226_CHAN_SHOT          = 10, 
    F4226_CHAN_STOP          = 11,
    F4226_CHAN_ISTART        = 12,
    F4226_CHAN_WAITTIME      = 13, 
    F4226_CHAN_CALC_STATS    = 14,

    F4226_CHAN_PTSOFS        = 20,
    F4226_CHAN_NUMPTS        = 21,
    F4226_CHAN_TIMING        = 22,
    F4226_CHAN_RANGE         = 23,
    F4226_CHAN_PREHIST64     = 24,

    /* 30-49: status */
    F4226_CHAN_ELAPSED       = 30,
    F4226_CHAN_XS_PER_POINT  = 31,
    F4226_CHAN_XS_DIVISOR    = 32,
    F4226_CHAN_XS_FACTOR     = 33,
    F4226_CHAN_CUR_PTSOFS    = 34,
    F4226_CHAN_CUR_NUMPTS    = 35,
    F4226_CHAN_CUR_TIMING    = 36,
    F4226_CHAN_CUR_RANGE     = 37,
    F4226_CHAN_CUR_PREHIST64 = 38,

    F4226_CHAN_ON            = 39,
    F4226_CHAN_RANGEMIN      = 40,
    F4226_CHAN_RANGEMAX      = 41,

    F4226_CHAN_MIN           = 42, F4226_CHAN_STATS_first = F4226_CHAN_MIN,
    F4226_CHAN_MAX           = 43,
    F4226_CHAN_AVG           = 44,
    F4226_CHAN_INT           = 45, F4226_CHAN_STATS_last  = F4226_CHAN_INT,

    F4226_CHAN_TOTALMIN      = 46,
    F4226_CHAN_TOTALMAX      = 47,

    F4226_CHAN_NUM_LINES     = 48,

    F4226_NUMCHANS           = 50
};

/* Timings */
enum
{
    F4226_T_50NS  = 0,
    F4226_T_100NS = 1,
    F4226_T_200NS = 2,
    F4226_T_400NS = 3,
    F4226_T_800NS = 4,
    F4226_T_1_6US = 5,
    F4226_T_3_2US = 6,
    F4226_T_EXT   = 7,
};

/* Ranges */
enum
{
    F4226_INP_256MV  = 0,
    F4226_INP_512MV  = 1,
    F4226_INP_1V     = 2,
    F4226_TST_256MV  = 3,
};

/* General device specs */
enum
{
    F4226_NUM_LINES  = 1,
    F4226_MAX_NUMPTS = 1024,
};
enum
{
    F4226_MIN_VALUE = -10160000, // ???
    F4226_MAX_VALUE = +10240000, // ???
};


#endif /* __F4226_DRV_I_H */
