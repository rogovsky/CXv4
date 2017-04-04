#ifndef __OTTUCC_DRV_I_H
#define __OTTUCC_DRV_I_H


// r1h??????,r9i,w20i,r20i
enum
{
    /* 0-9: data */
    OTTUCC_CHAN_DATA          = 0,

    /* 10-29: controls */
    OTTUCC_CHAN_SHOT          = 10,
    OTTUCC_CHAN_ISTART        = 11,
    OTTUCC_CHAN_WAITTIME      = 12,
    OTTUCC_CHAN_STOP          = 13,

    OTTUCC_CHAN_X             = 14,
    OTTUCC_CHAN_Y             = 15,
    OTTUCC_CHAN_W             = 16,
    OTTUCC_CHAN_H             = 17,
    OTTUCC_CHAN_K             = 18,
    OTTUCC_CHAN_T             = 19,
    OTTUCC_CHAN_SYNC          = 20,
    OTTUCC_CHAN_RRQ_MSECS     = 21,

    /* 30-49: status */
    OTTUCC_CHAN_MISS          = 30,
    OTTUCC_CHAN_ELAPSED       = 31,

    OTTUCC_CHAN_CUR_X         = 34,
    OTTUCC_CHAN_CUR_Y         = 35,
    OTTUCC_CHAN_CUR_W         = 36,
    OTTUCC_CHAN_CUR_H         = 37,
    OTTUCC_CHAN_CUR_K         = 38,
    OTTUCC_CHAN_CUR_T         = 39,
    OTTUCC_CHAN_CUR_SYNC      = 40,
    OTTUCC_CHAN_CUR_RRQ_MSECS = 41,

    OTTUCC_NUMCHANS           = 50
};


/* General device specs */
enum
{
    OTTUCC_MAX_W      = ???753,
    OTTUCC_MAX_H      = ???480,

    OTTUCC_SRCMAXVAL = 65535,
    OTTUCC_BPP       = 16,
};

#endif /* __OTTUCC_DRV_I_H */
