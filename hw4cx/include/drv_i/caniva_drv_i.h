#ifndef __CANIVA_DRV_I_H
#define __CANIVA_DRV_I_H


// r16i,r16i,w16i,w16i,r16i,r16i,r16i
enum
{
    CANIVA_CHAN_IMES_n_base  =  0, CANIVA_CHAN_IMES_n_count = 16,
    CANIVA_CHAN_UMES_n_base  = 16, CANIVA_CHAN_UMES_n_count = 16,
    CANIVA_CHAN_ILIM_n_base  = 32, CANIVA_CHAN_ILIM_n_count = 16,
    CANIVA_CHAN_ULIM_n_base  = 48, CANIVA_CHAN_ULIM_n_count = 16,
    CANIVA_CHAN_ILHW_n_base  = 64, CANIVA_CHAN_ILHW_n_count = 16,
    CANIVA_CHAN_IALM_n_base  = 80, CANIVA_CHAN_IALM_n_count = 16,
    CANIVA_CHAN_UALM_n_base  = 96, CANIVA_CHAN_UALM_n_count = 16,

    CANIVA_NUMCHANS          = 112
};


#endif /* __CANIVA_DRV_I_H */
