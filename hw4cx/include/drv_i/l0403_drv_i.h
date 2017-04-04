#ifndef __L0403_DRV_I_H
#define __L0403_DRV_I_H


// w5i,r1i
enum
{
    L0403_CHAN_OUT0    = 0,
    L0403_CHAN_OUT1    = 1,
    L0403_CHAN_OUT2    = 2,
    L0403_CHAN_OUT3    = 3,

    L0403_CHAN_DISABLE = 4,
    
    L0403_CHAN_KIND    = 5,

    L0403_NUMCHANS = 6
};

enum
{
    L0403_KIND_ABSENT = 0,
    L0403_KIND_4x2to1 = 1,
    L0403_KIND_1x8to1 = 2,
    L0403_KIND_2x4to1 = 3,
};


#endif /* __L0403_DRV_I_H */
