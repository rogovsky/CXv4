#ifndef __WELDPROC_DRV_I_H
#define __WELDPROC_DRV_I_H


// w10i,w20i,r20i
enum
{
    WELDPROC_CHAN_WR_base    = 0,
      WELDPROC_CHAN_WR_count = 50,

    WELDPROC_CHAN_DO_START     = WELDPROC_CHAN_WR_base + 0,
    WELDPROC_CHAN_DO_STOP      = WELDPROC_CHAN_WR_base + 1,
    WELDPROC_CHAN_RESET_STATE  = WELDPROC_CHAN_WR_base + 2,

    WELDPROC_CHAN_TIME         = WELDPROC_CHAN_WR_base + 9,

    WELDPROC_max_lines_count = 10,
    WELDPROC_CHAN_BEG_n_base   = 10,
    WELDPROC_CHAN_END_n_base   = 20,


    WELDPROC_CHAN_RD_base    = 50,
    WELDPROC_CHAN_RD_count   = 50,

    WELDPROC_CHAN_IS_WELDING   = WELDPROC_CHAN_RD_base + 0,

    WELDPROC_CHAN_CUR_n_base   = 60,

    WELDPROC_CHAN_VDEV_STATE   = 99,

    WELDPROC_NUMCHANS       = 100,
};


#endif /* __WELDPROC_DRV_I_H */
