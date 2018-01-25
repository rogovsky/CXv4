#ifndef __VDAC20_DRV_I_H
#define __VDAC20_DRV_I_H


// w50,r8,w1,r41
enum
{
    /*** Config channels ********************************************/
    VDAC20_CONFIG_CHAN_base  = 0,
    VDAC20_CONFIG_CHAN_count = 20,

    VDAC20_CHAN_DO_CALIBRATE = 16,
    VDAC20_CHAN_DIGCORR_MODE = 17,
    VDAC20_CHAN_DIGCORR_V    = 18,

    /*** Regular channels *******************************************/
    VDAC20_CHAN_ADC_n_base   = 50, VDAC20_CHAN_ADC_n_count = 8,
    VDAC20_CHAN_OUT_n_base   = 58, VDAC20_CHAN_OUT_n_count = 1,
    
    VDAC20_NUMCHANS          = 100
};


#endif /* __VDAC20_DRV_I_H */
