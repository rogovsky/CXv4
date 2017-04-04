#ifndef __C061621_DRV_I_H
#define __C061621_DRV_I_H


// r1i4096,r9i,w20i,r19i,r1t20
enum
{
    /* 0-9: data */
    C061621_CHAN_DATA          = 0,

    /* 10-29: controls */
    C061621_CHAN_SHOT          = 10, 
    C061621_CHAN_STOP          = 11,
    C061621_CHAN_ISTART        = 12,
    C061621_CHAN_WAITTIME      = 13, 
    C061621_CHAN_CALC_STATS    = 14,

    C061621_CHAN_PTSOFS        = 20,
    C061621_CHAN_NUMPTS        = 21,
    C061621_CHAN_TIMING        = 22,
    C061621_CHAN_RANGE         = 23,

    /* 30-49: status */
    C061621_CHAN_ELAPSED       = 30,
    C061621_CHAN_XS_PER_POINT  = 31,
    C061621_CHAN_XS_DIVISOR    = 32,
    C061621_CHAN_XS_FACTOR     = 33,
    C061621_CHAN_CUR_PTSOFS    = 34,
    C061621_CHAN_CUR_NUMPTS    = 35,
    C061621_CHAN_CUR_TIMING    = 36,
    C061621_CHAN_CUR_RANGE     = 37,

    C061621_CHAN_ON            = 38,
    C061621_CHAN_RANGEMIN      = 39,
    C061621_CHAN_RANGEMAX      = 40,

    C061621_CHAN_MIN           = 41, C061621_CHAN_STATS_first = C061621_CHAN_MIN,
    C061621_CHAN_MAX           = 42,
    C061621_CHAN_AVG           = 43,
    C061621_CHAN_INT           = 44, C061621_CHAN_STATS_last  = C061621_CHAN_INT,

    C061621_CHAN_TOTALMIN      = 45,
    C061621_CHAN_TOTALMAX      = 46,

    C061621_CHAN_NUM_LINES     = 47,

    C061621_CHAN_DEVTYPE_ID    = 48,
    C061621_CHAN_DEVTYPE_STR   = 49,

    C061621_NUMCHANS           = 50
};
#if 0
// w60,r20 1*100+0i/100+4096i
enum
{
    /* 1. writable parameters, range = [0, 50)                    */
    /* a. [00-10), standard controls, common for all drivers      */
    C061621_PARAM_SHOT       = 0,
    C061621_PARAM_STOP       ,
    C061621_PARAM_ISTART     ,
    C061621_PARAM_WAITTIME   ,
    C061621_PARAM_CALC_STATS ,

    /* b. [10-20), fastadc-specific controls, common for adc's    */
    C061621_PARAM_PTSOFS     = 10,
    C061621_PARAM_NUMPTS     ,

    /* c. [20-30), device-specific controls, such as timings,     */
    /*             frqdivision i.e. common for all lines          */
    C061621_PARAM_TIMING     = 20,

    /* e. [40-60), per-line configuration, i.e. individual delay, */
    /*             individual zero, individual gain measurements  */
    /*             range, zero shift and so on                    */
    C061621_PARAM_RANGE      = 40,

    /* 2. readable parameters, range = [60, 100)                  */
    /* a. only statistics avaliable at the moment                 */
    C061621_PARAM_MIN        = 60,
    C061621_PARAM_MAX        = 64,
    C061621_PARAM_AVG        = 68,
    C061621_PARAM_INT        = 72,

    C061621_PARAM_DEVTYPE    = 97,
    C061621_PARAM_STATUS     = 98,
    C061621_PARAM_ELAPSED    = 99,

    C061621_NUM_PARAMS       = 100,
};
#endif

/* Timings */
enum
{
    C061621_T_50NS  = 0,
    C061621_T_100NS = 1,
    C061621_T_200NS = 2,
    C061621_T_400NS = 3,
    C061621_T_500NS = 4,
    C061621_T_1US   = 5,
    C061621_T_2US   = 6,
    C061621_T_4US   = 7,
    C061621_T_5US   = 8,
    C061621_T_10US  = 9,
    C061621_T_20US  = 10,
    C061621_T_40US  = 11,
    C061621_T_50US  = 12,
    C061621_T_100US = 13,
    C061621_T_200US = 14,
    C061621_T_400US = 15,
    C061621_T_500US = 16,
    C061621_T_1MS   = 17,
    C061621_T_2MS   = 18,
    C061621_T_CPU   = 19,
    C061621_T_EXT   = 20,
};

/* Ranges */
enum
{
    C061621_R_80MV   = 0,
    C061621_R_160MV  = 1,
    C061621_R_320MV  = 2,
    C061621_R_639MV  = 3,
    C061621_R_1_279V = 4,
    C061621_R_2_558V = 5,
    C061621_R_5_115V = 6,
    C061621_R_10_02V = 7,
};

/* General device specs */
enum
{
    C061621_NUM_LINES  = 1,
    C061621_MAX_NUMPTS = 4096,
};
enum
{
    C061621_MIN_VALUE = -10230000, // ???
    C061621_MAX_VALUE = +10230000, // ???
};


#endif /* __C061621_DRV_I_H */
