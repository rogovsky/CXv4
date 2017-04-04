#define ABSTRACT_NAME frolov_bfp2
#include "camac_abstract_common.h"

#include "drv_i/frolov_bfp2_drv_i.h"


/*!!! HACK, for abstract-drivers only, otherwise should store in privrec */
static int       numperiods       = 1;
static rflags_t  numperiods_flags = 0;

ABSTRACT_INIT_FUNC()
{
    numperiods_flags = x2rflags(DO_NAF(ref, N1, 0, 1, &numperiods));

    return 0;
}

ABSTRACT_READ_FUNC()
{
  int  w;

    switch (chan)
    {
        case FROLOV_BFP2_CHAN_KMAX:
        case FROLOV_BFP2_CHAN_KMIN:
            *rflags = x2rflags
                (DO_NAF(ref, N1, 0 + chan-FROLOV_BFP2_CHAN_KMAX, 0, value));
            break;

        case FROLOV_BFP2_CHAN_KTIME:
            *rflags = x2rflags(DO_NAF(ref, N1, 1, 1, value));
            break;

        case FROLOV_BFP2_CHAN_LOOPMODE:
            *rflags = x2rflags(DO_NAF(ref, N1, 3, 1, &w));
            *value = w & 1;
            break;

        case FROLOV_BFP2_CHAN_NUMPERIODS:
            *rflags = numperiods_flags;
            *value  = numperiods;
            break;

        case FROLOV_BFP2_CHAN_MOD_ENABLE:
        case FROLOV_BFP2_CHAN_MOD_DISABLE:
        case FROLOV_BFP2_CHAN_RESET_CTRS:
        case FROLOV_BFP2_CHAN_WRK_ENABLE:
        case FROLOV_BFP2_CHAN_WRK_DISABLE:
        case FROLOV_BFP2_CHAN_LOOP_START:
        case FROLOV_BFP2_CHAN_LOOP_STOP:
            *rflags = 0;
            *value  = 0;
            break;

        case FROLOV_BFP2_CHAN_STAT_UBS:
        case FROLOV_BFP2_CHAN_STAT_WKALWD:
        case FROLOV_BFP2_CHAN_STAT_MOD:
        case FROLOV_BFP2_CHAN_STAT_LOOP:
            *rflags = x2rflags(DO_NAF(ref, N1, 2, 0, &w));
            *value  = (w >> (chan - FROLOV_BFP2_CHAN_STAT_UBS)) & 1;
            break;

        case FROLOV_BFP2_CHAN_STAT_CURPRD:
            *rflags = x2rflags(DO_NAF(ref, N1, 0, 1, value));
            break;

        default:
            *rflags = CXRF_UNSUPPORTED;
            *value  = 0;
    }

    return 0;
}

ABSTRACT_FEED_FUNC()
{
  int  w;
  int  junk;
  int  A = -1;

    switch (chan)
    {
        case FROLOV_BFP2_CHAN_KMAX:
        case FROLOV_BFP2_CHAN_KMIN:
            if (*value < 1)      *value = 1;
            if (*value > 655355) *value = 65535;
            *rflags = x2rflags
                (DO_NAF(ref, N1, 0 + chan-FROLOV_BFP2_CHAN_KMAX, 16, value));
            *rflags |= x2rflags(DO_NAF(ref, N1, 2, 25, &junk));
            break;

        case FROLOV_BFP2_CHAN_KTIME:
            if (*value < 0) *value = 0;
            if (*value > 3) *value = 3;
            *rflags = x2rflags(DO_NAF(ref, N1, 1, 17, value));
            break;

        case FROLOV_BFP2_CHAN_LOOPMODE:
            *value &= 1;
            w = *value | 2;
            *rflags = x2rflags(DO_NAF(ref, N1, 3, 17, &w));
            break;

        case FROLOV_BFP2_CHAN_MOD_ENABLE:
        case FROLOV_BFP2_CHAN_MOD_DISABLE:
        case FROLOV_BFP2_CHAN_RESET_CTRS:
        case FROLOV_BFP2_CHAN_WRK_ENABLE:
        case FROLOV_BFP2_CHAN_WRK_DISABLE:
        case FROLOV_BFP2_CHAN_LOOP_START:
        case FROLOV_BFP2_CHAN_LOOP_STOP:
            A = chan - FROLOV_BFP2_CHAN_MOD_ENABLE;
            if (*value == CX_VALUE_COMMAND)
            {
                if (chan == FROLOV_BFP2_CHAN_LOOP_START)
                    *rflags = x2rflags(DO_NAF(ref, N1, 0, 17, &numperiods));
                else
                    *rflags = 0;
                *rflags |= x2rflags(DO_NAF(ref, N1, A, 25, &junk));
            }
            else
                *rflags  = 0;

            *value  = 0;
            break;

        case FROLOV_BFP2_CHAN_NUMPERIODS:
            if (*value < 0)     *value = 0;
            if (*value > 65535) *value = 65535;
            numperiods = *value;
            *rflags = x2rflags(DO_NAF(ref, N1, 0, 17, value));
            break;

        default:
            return A_READ_FUNC_NAME(ref, N1, chan, value, rflags);
    }

    return 0;
}
