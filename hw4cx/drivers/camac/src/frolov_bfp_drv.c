#define ABSTRACT_NAME frolov_bfp
#include "camac_abstract_common.h"

#include "drv_i/frolov_bfp_drv_i.h"


ABSENT_INIT_FUNC

ABSTRACT_READ_FUNC()
{
  int  w;
    
    switch (chan)
    {
        case FROLOV_BFP_CHAN_KMAX:
        case FROLOV_BFP_CHAN_KMIN:
            *rflags = x2rflags
                (DO_NAF(ref, N1, chan-FROLOV_BFP_CHAN_KMAX, 0, value));
            break;

        case FROLOV_BFP_CHAN_WRK_ENABLE:
        case FROLOV_BFP_CHAN_WRK_DISABLE:
        case FROLOV_BFP_CHAN_MOD_ENABLE:
        case FROLOV_BFP_CHAN_MOD_DISABLE:
            *rflags = 0;
            *value  = 0;
            break;
            
        case FROLOV_BFP_CHAN_STAT_UBS:
        case FROLOV_BFP_CHAN_STAT_WKALWD:
        case FROLOV_BFP_CHAN_STAT_WRK:
        case FROLOV_BFP_CHAN_STAT_MOD:
            *rflags = x2rflags(DO_NAF(ref, N1, 2, 0, &w));
            *value  = (w >> (chan - FROLOV_BFP_CHAN_STAT_UBS)) & 1;
            break;

        default:
            *rflags = CXRF_UNSUPPORTED;
            *value  = 0;
            break;
    }
    
    return 0;
}

ABSTRACT_FEED_FUNC()
{
  int  junk;
  int  A = -1;
    
    switch (chan)
    {
        case FROLOV_BFP_CHAN_KMAX:
        case FROLOV_BFP_CHAN_KMIN:
            if (*value < 0)   *value = 0;
            if (*value > 255) *value = 255;
            *rflags = x2rflags
                (DO_NAF(ref, N1, chan-FROLOV_BFP_CHAN_KMAX, 16, value));
            *rflags |= x2rflags(DO_NAF(ref, N1, 2, 25, &junk));
            break;

        case FROLOV_BFP_CHAN_WRK_ENABLE:  if (A < 0) A = 3;
        case FROLOV_BFP_CHAN_WRK_DISABLE: if (A < 0) A = 4;
        case FROLOV_BFP_CHAN_MOD_ENABLE:  if (A < 0) A = 0;
        case FROLOV_BFP_CHAN_MOD_DISABLE: if (A < 0) A = 1;
            if (*value == CX_VALUE_COMMAND)
                *rflags = x2rflags(DO_NAF(ref, N1, A, 25, &junk));
            else
                *rflags = 0;
        
            *value  = 0;
            break;
            
        default:
            return A_READ_FUNC_NAME(ref, N1, chan, value, rflags);
    }

    return 0;
}
