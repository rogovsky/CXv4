#ifndef __OTTCAM_DRV_I_H
#define __OTTCAM_DRV_I_H


// r1h361440,r9i,w20i,r20i
enum
{
    /* 0-9: data */
    OTTCAM_CHAN_DATA          = 0,

    /* 10-29: controls */
    OTTCAM_CHAN_SHOT          = 10,
    OTTCAM_CHAN_ISTART        = 11,
    OTTCAM_CHAN_WAITTIME      = 12,
    OTTCAM_CHAN_STOP          = 13,

    OTTCAM_CHAN_X             = 14,
    OTTCAM_CHAN_Y             = 15,
    OTTCAM_CHAN_W             = 16,
    OTTCAM_CHAN_H             = 17,
    OTTCAM_CHAN_K             = 18,
    OTTCAM_CHAN_T             = 19,
    OTTCAM_CHAN_SYNC          = 20,
    OTTCAM_CHAN_RRQ_MSECS     = 21,

    /* 30-49: status */
    OTTCAM_CHAN_MISS          = 30,
    OTTCAM_CHAN_ELAPSED       = 31,

    OTTCAM_CHAN_CUR_X         = 34,
    OTTCAM_CHAN_CUR_Y         = 35,
    OTTCAM_CHAN_CUR_W         = 36,
    OTTCAM_CHAN_CUR_H         = 37,
    OTTCAM_CHAN_CUR_K         = 38,
    OTTCAM_CHAN_CUR_T         = 39,
    OTTCAM_CHAN_CUR_SYNC      = 40,
    OTTCAM_CHAN_CUR_RRQ_MSECS = 41,

    OTTCAM_NUMCHANS           = 50
};


/* General device specs */
enum
{
    OTTCAM_MAX_W      = 753,
    OTTCAM_MAX_H      = 480,

    OTTCAM_SRCMAXVAL = 1022,
    OTTCAM_BPP       = 10,
};

#endif /* __OTTCAM_DRV_I_H */
