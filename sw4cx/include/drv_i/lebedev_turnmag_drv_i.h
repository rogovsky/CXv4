#ifndef __LEBEDEV_TURNMAG_DRV_I_H
#define __LEBEDEV_TURNMAG_DRV_I_H


// w10i,r10i
enum
{
    LEBEDEV_TURNMAG_CHAN_I_LIM          = 0,
    LEBEDEV_TURNMAG_CHAN_I_LIM_TIME     = 1,
    LEBEDEV_TURNMAG_CHAN_GO_MAX_d_base  = 2,
    LEBEDEV_TURNMAG_CHAN_GO_BURY_MAX    = LEBEDEV_TURNMAG_CHAN_GO_MAX_d_base + 0,
    LEBEDEV_TURNMAG_CHAN_GO_RING_MAX    = LEBEDEV_TURNMAG_CHAN_GO_MAX_d_base + 1,
    LEBEDEV_TURNMAG_CHAN_GO_100_d_base  = 4,
    LEBEDEV_TURNMAG_CHAN_GO_BURY_100MS  = LEBEDEV_TURNMAG_CHAN_GO_100_d_base + 0,
    LEBEDEV_TURNMAG_CHAN_GO_RING_100MS  = LEBEDEV_TURNMAG_CHAN_GO_100_d_base + 1,
    LEBEDEV_TURNMAG_CHAN_GO_N_S_d_base  = 6,
    LEBEDEV_TURNMAG_CHAN_GO_BURY_N_S    = LEBEDEV_TURNMAG_CHAN_GO_N_S_d_base + 0,
    LEBEDEV_TURNMAG_CHAN_GO_RING_N_S    = LEBEDEV_TURNMAG_CHAN_GO_N_S_d_base + 1,
    LEBEDEV_TURNMAG_CHAN_N_S            = 8,
    LEBEDEV_TURNMAG_CHAN_STOP           = 9,

    LEBEDEV_TURNMAG_CHAN_CUR_DIRECTION  = 10,
    LEBEDEV_TURNMAG_CHAN_LIM_SW_base    = 11,
    LEBEDEV_TURNMAG_CHAN_LIM_SW_BURY    = LEBEDEV_TURNMAG_CHAN_LIM_SW_base + 0,
    LEBEDEV_TURNMAG_CHAN_LIM_SW_RING    = LEBEDEV_TURNMAG_CHAN_LIM_SW_base + 1,
    LEBEDEV_TURNMAG_CHAN_I_MOTOR        = 13,
    LEBEDEV_TURNMAG_CHAN_I_EXC_TIME     = 14,
    LEBEDEV_TURNMAG_CHAN_GO_TIME_LEFT   = 15,
    LEBEDEV_TURNMAG_CHAN_GOING_DIR      = 16,
    LEBEDEV_TURNMAG_CHAN_zzz            = 19,

    LEBEDEV_TURNMAG_NUMCHANS = 20
};

enum
{
    LEBEDEV_TURNMAG_DIR_UNKNOWN = 0,
    LEBEDEV_TURNMAG_DIR_RING    = 1,
    LEBEDEV_TURNMAG_DIR_BURY    = 2,
    LEBEDEV_TURNMAG_DIR_BETWEEN = 3,
};

enum
{
    LEBEDEV_TURNMAG_GOING_BURY = -1,
    LEBEDEV_TURNMAG_GOING_STOP =  0,
    LEBEDEV_TURNMAG_GOING_RING = +1,
};


#endif /* __LEBEDEV_TURNMAG_DRV_I_H */
