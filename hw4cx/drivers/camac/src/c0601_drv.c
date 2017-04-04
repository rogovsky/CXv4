#define ABSTRACT_NAME c0601
#include "camac_abstract_common.h"

#include "drv_i/c0601_drv_i.h"


/*********************************************************************
 
  A totally simplified driver for C0601.
  It supports reading temperature measurements only.
  At start, all interlocks are disabled (by writing limit=128).
 
*********************************************************************/


static rflags_t DO_RPTD_NAF(int ref, int N, int A, int F, int *data)
{
  int  tries;
  int  status;

    for (tries = 0,  status =  0;
         tries < 10  &&  (status & CAMAC_Q) == 0;
         tries++)
        status = DO_NAF(ref, N, A, F, data);

    if ((status & CAMAC_Q) == 0) return CXRF_IO_TIMEOUT;
    else                         return status2rflags(status);
}


ABSTRACT_INIT_FUNC()
{
  int  c;
  int  n;

    c = 0;
    DO_RPTD_NAF(ref, N1, 0, 16, &c);
    c = 128;
    for (n = 0;  n < 16;  n++)
        DO_RPTD_NAF(ref, N1, 1, 16, &c);

    return 0;
}

ABSTRACT_READ_FUNC()
{
  int  c;

    switch (chan)
    {
        case C0601_CHAN_T_n_BASE ... C0601_CHAN_T_n_BASE + C0601_CHAN_T_n_COUNT - 1:
            c = chan - C0601_CHAN_T_n_BASE;
            *rflags  = DO_RPTD_NAF(ref, N1, 1, 16, &c);
            if ((*rflags & CXRF_IO_TIMEOUT) != 0) break;
            *rflags |= DO_RPTD_NAF(ref, N1, 2, 0,  &c);
            *value   = c & 127;
            break;

        default:
            *value  = 0;
            *rflags = CXRF_UNSUPPORTED;
    }
    
    return 0;
}

ABSENT_FEED_FUNC
