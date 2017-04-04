#define ABSTRACT_NAME frolov_spd
#include "camac_abstract_common.h"

#include "drv_i/frolov_spd_drv_i.h"


ABSTRACT_INIT_FUNC()
{
  int  dummy;
  
    DO_NAF(ref, N1, 0, 25, &dummy);
    return 0;
}

ABSTRACT_READ_FUNC()
{
  int  c;
    
    switch (chan)
    {
        case FROLOV_SPD_CHAN_NUMTURNS:
            *rflags = status2rflags(DO_NAF(ref, N1, 1,  0, value));
            break;

        case FROLOV_SPD_CHAN_NUMBUNCHES:
            *rflags = status2rflags(DO_NAF(ref, N1, 0,  0, value));
            *value &= 0x0F;
            break;

        case FROLOV_SPD_CHAN_SAWTYPE:
        case FROLOV_SPD_CHAN_OPMODE:
            *rflags = status2rflags(DO_NAF(ref, N1, 0, 0, &c));
            *value  = (c & (chan == FROLOV_SPD_CHAN_SAWTYPE? 64 : 16)) != 0;
            break;

        case FROLOV_SPD_CHAN_DOSTART:
            *value  = 0;
            *rflags = 0;
            break;
            
        default:
            *value  = 0;
            *rflags = CXRF_UNSUPPORTED;
    }
    
    return 0;
}

ABSTRACT_FEED_FUNC()
{
  int  c;
  int  v;
  int  dummy;
    
    switch (chan)
    {
        case FROLOV_SPD_CHAN_NUMTURNS:
            v = *value;
            if (v < 0)   v = 1;
            if (v > 255) v = 255;
            *rflags = status2rflags(DO_NAF(ref, N1, 1, 16, &v));
            *value = v;
            break;

        case FROLOV_SPD_CHAN_NUMBUNCHES:
            v = *value;
            if (v < 0)   v = 1;
            if (v > 16)  v = 15;
            *rflags = status2rflags(DO_NAF(ref, N1, 0, 16, &v));
            *value = v;
            break;

        case FROLOV_SPD_CHAN_SAWTYPE:
        case FROLOV_SPD_CHAN_OPMODE:
            *value = (*value != 0);
            *rflags = status2rflags(DO_NAF(ref, N1, 1, *value? 24:26, &dummy));
            break;

        case FROLOV_SPD_CHAN_DOSTART:
            break;
            
        default:
            *value  = 0;
            *rflags = CXRF_UNSUPPORTED;
    }
    
    return 0;
}
