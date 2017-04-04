#ifndef __DDS500_DRV_I_H
#define __DDS500_DRV_I_H


enum
{
    DDS500_CHAN_RESET_TO_DEFAULTS = 9,

    DDS500_CHAN_SERIAL            = 10,
    DDS500_CHAN_FIRMWARE          = 11,
    
    DDS500_NUMCHANS               = 20
};

enum
{
    DDS500_V_RESET_TO_DEFAULTS_KEYVAL = 'R' + ('s' << 8) + ('e' << 16) + ('t' << 24),
};


#endif /* __DDS500_DRV_I_H */
