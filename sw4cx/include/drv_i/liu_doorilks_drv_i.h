#ifndef __LIU_DOORILKS_DRV_I_H
#define __LIU_DOORILKS_DRV_I_H


// w10i,r10i
enum
{
    // Prerequisites
    LIU_DOORILKS_ILK_count = 4,

    LIU_DOORILKS_CHAN_WR_base     = 0,
      LIU_DOORILKS_CHAN_WR_count  = 10,

    LIU_DOORILKS_CHAN_MODE        = LIU_DOORILKS_CHAN_WR_base + 0,
    LIU_DOORILKS_CHAN_USED_n_base = LIU_DOORILKS_CHAN_WR_base + 1,
    // ...LIU_DOORILKS_ILK_count-1 => +1+3

    LIU_DOORILKS_CHAN_RD_base     = 10,
      LIU_DOORILKS_CHAN_RD_count  = 10,

    LIU_DOORILKS_CHAN_SUM_STATE   = LIU_DOORILKS_CHAN_RD_base + 0,
    LIU_DOORILKS_CHAN_ILK_n_base  = LIU_DOORILKS_CHAN_RD_base + 1,
    // ...LIU_DOORILKS_ILK_count-1 => +1+3

    LIU_DOORILKS_NUMCHANS = 20,
};

enum
{
    LIU_DOORILKS_MODE_TEST = 0,
    LIU_DOORILKS_MODE_WORK = 1,
};


#endif /* __LIU_DOORILKS_DRV_I_H */
