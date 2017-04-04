#define ABSTRACT_NAME frolov_bl700
#include "camac_abstract_common.h"

#include "drv_i/frolov_bl700_drv_i.h"


enum
{
    A_RD_ILKS = 0,
    A_RD_MINS = 1,
    A_RD_MAXS = 2,
};


ABSENT_INIT_FUNC

ABSTRACT_READ_FUNC()
{
  int  c;

    switch (chan)
    {
        case FROLOV_BL700_CHAN_MIN_ILK_n_BASE ... FROLOV_BL700_CHAN_MIN_ILK_n_BASE + FROLOV_BL700_CHAN_MIN_ILK_n_COUNT - 1:
            *rflags = x2rflags(DO_NAF(ref, N1, A_RD_MINS, 0, &c));
            *value  = ((c & (1 << (chan - FROLOV_BL700_CHAN_MIN_ILK_n_BASE))) != 0);
            break;

        case FROLOV_BL700_CHAN_MAX_ILK_n_BASE ... FROLOV_BL700_CHAN_MAX_ILK_n_BASE + FROLOV_BL700_CHAN_MAX_ILK_n_COUNT - 1:
            *rflags = x2rflags(DO_NAF(ref, N1, A_RD_MAXS, 0, &c));
            *value  = ((c & (1 << (chan - FROLOV_BL700_CHAN_MAX_ILK_n_BASE))) != 0);
            break;

        case FROLOV_BL700_CHAN_MIN_ILK_8B:
            *rflags = x2rflags(DO_NAF(ref, N1, A_RD_MINS, 0, value));
            break;

        case FROLOV_BL700_CHAN_MAX_ILK_8B:
            *rflags = x2rflags(DO_NAF(ref, N1, A_RD_MAXS, 0, value));
            break;

        case FROLOV_BL700_CHAN_OUT_ILK_STATE:
            *rflags = x2rflags(DO_NAF(ref, N1, A_RD_ILKS, 0, &c));
            *value = ((c & 1) != 0);
            break;

        case FROLOV_BL700_CHAN_EXT_ILK_STATE:
            *rflags = x2rflags(DO_NAF(ref, N1, A_RD_ILKS, 0, &c));
            *value = ((c & 2) != 0);
            break;

        case FROLOV_BL700_CHAN_RESET_ILKS:
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
  int  junk;
    
    if (chan == FROLOV_BL700_CHAN_RESET_ILKS  &&
        *value == CX_VALUE_COMMAND)
    {
        *rflags = x2rflags(DO_NAF(ref, N1, 0, 25, &junk));
        return 0;
    }
    
    return A_READ_FUNC_NAME(ref, N1, chan, value, rflags);
}
