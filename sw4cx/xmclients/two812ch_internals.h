#ifndef __TWO812CH_INTERNALS_H
#define __TWO812CH_INTERNALS_H


enum
{
    TWO812CH_CHAN_LINE1,
    TWO812CH_CHAN_LINE2,
    TWO812CH_CHAN_MARKER,

    TWO812CH_CHAN_XS_PER_POINT,
    TWO812CH_CHAN_XS_DIVISOR,
    TWO812CH_CHAN_XS_FACTOR,
    TWO812CH_CHAN_CUR_PTSOFS,
    TWO812CH_CHAN_CUR_NUMPTS,
    TWO812CH_CHAN_CUR_TIMING, /*!!! Should be removed upon deployment of XS management in fastadc_data */
    TWO812CH_CHAN_CUR_FRQDIV, /*!!! Should be removed upon deployment of XS management in fastadc_data */

    TWO812CH_CHAN_LINE1RANGEMIN,
    TWO812CH_CHAN_LINE2RANGEMIN,
    TWO812CH_CHAN_LINE1RANGEMAX,
    TWO812CH_CHAN_LINE2RANGEMAX,

    TWO812CH_NUMCHANS
};

/* General device specs */
enum
{
    TWO812CH_NUM_LINES  = 2,
    TWO812CH_MAX_NUMPTS = 262143,
};
enum
{
    TWO812CH_MIN_VALUE = -8192000, // (    0-2048)*4000
    TWO812CH_MAX_VALUE = +8188000, // (0xFFF-2048)*4000
};

/*!!! Should be removed upon deployment of XS management in fastadc_data */
enum
{
    TWO812CH_T_4MHZ = 0,
    TWO812CH_T_EXT  = 1
};


#endif /* __TWO812CH_INTERNALS_H */
