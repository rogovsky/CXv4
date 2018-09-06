#ifndef __V5K5045_DRV_I_H
#define __V5K5045_DRV_I_H


// w20i,r80i
enum
{
    V5K5045_CHAN_WR_base       = 0,
      V5K5045_CHAN_WR_count    = 50,

    V5K5045_CHAN_HVSET           = V5K5045_CHAN_WR_base + 0,
    V5K5045_CHAN_HVSET_RATE      = V5K5045_CHAN_WR_base + 1,
    V5K5045_CHAN_PWRSET          = V5K5045_CHAN_WR_base + 2,
    V5K5045_CHAN_SWITCH_ON       = V5K5045_CHAN_WR_base + 3,
    V5K5045_CHAN_SWITCH_OFF      = V5K5045_CHAN_WR_base + 4,
    V5K5045_CHAN_RST_ILKS        = V5K5045_CHAN_WR_base + 5,
    V5K5045_CHAN_RESET_STATE     = V5K5045_CHAN_WR_base + 6,
    V5K5045_CHAN_RESET_C_ILKS    = V5K5045_CHAN_WR_base + 7,
#if 0
    V5K5045_CHAN_ = V5K5045_CHAN_WR_base + ,
#endif

    V5K5045_CHAN_RD_base       = 20,
      V5K5045_CHAN_RD_count    = 80,

    V5K5045_CHAN_HVSET_CUR       = V5K5045_CHAN_RD_base + 0,

    V5K5045_CHAN_ILK_base      = V5K5045_CHAN_RD_base + 10,
      V5K5045_CHAN_ILK_count   = 32,

    V5K5045_CHAN_C_ILK_base    = V5K5045_CHAN_RD_base + 42,
      V5K5045_CHAN_C_ILK_count = 32,
      

    V5K5045_CHAN_VDEV_STATE = 99,

    V5K5045_NUMCHANS = 100
};


#endif /* __V5K5045_DRV_I_H */
