
#include "cxsd_driver.h"

#include "drv_i/c0642_drv_i.h"


enum {SIGN_MASK = 0x8000, VALUE_MASK = 0x7FFF};


typedef struct
{
    int       N;

    int       SCALE;  // Scale in raw<->uV calculations: 305uV/bit for 10V, 200uV/bit for 6V
} c0642_privrec_t;

static psp_paramdescr_t c0642_params[] =
{
    PSP_P_FLAG("10", c0642_privrec_t, SCALE, 305, 1),
    PSP_P_FLAG("1",  c0642_privrec_t, SCALE, 305, 0), // for compatibility with old a_c0642.h only
    PSP_P_FLAG("6",  c0642_privrec_t, SCALE, 200, 0),
    PSP_P_END()
};


static int c0642_init_d(int devid, void *devptr,
                        int businfocount, int *businfo,
                        const char *auxinfo)
{
  c0642_privrec_t *me = (c0642_privrec_t*)devptr;
  CxAnyVal_t       q;

    me->N = businfo[0];

    SetChanRDs  (devid, C0642_CHAN_OUT_n_base, C0642_CHAN_OUT_n_count, 1000000.0, 0.0);
    q.i32 = me->SCALE;
    SetChanQuant(devid, C0642_CHAN_OUT_n_base, C0642_CHAN_OUT_n_count, q, CXDTYPE_INT32);

    return DEVSTATE_OPERATING;
}

static void c0642_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  c0642_privrec_t *me = (c0642_privrec_t*)devptr;

  int              n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int              chn;   // channel

  int32            value;
  rflags_t         rflags;

  int              v;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
        if (action == DRVA_WRITE)
        {
            if (nelems[n] != 1  ||
                (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            value = *((int32*)(values[n]));
            ////fprintf(stderr, " write #%d:=%d\n", chn, val);
        }
        else
            value = 0xFFFFFFFF; // That's just to make GCC happy

        if (action == DRVA_WRITE)
        {
            if (value > +32767*me->SCALE) value = +32767*me->SCALE;
            if (value < -32767*me->SCALE) value = -32767*me->SCALE;
            
            v = value / me->SCALE;
          
            if (value < 0)
                v = SIGN_MASK | (((-value) / me->SCALE) & VALUE_MASK);
            else
                v = (value / me->SCALE) & VALUE_MASK;
        
            rflags = status2rflags(DO_NAF(CAMAC_REF, me->N, chn, 16, &v));
        }
        else
        {
            rflags = status2rflags(DO_NAF(CAMAC_REF, me->N, chn, 0,  &v));
            value  = v * me->SCALE;
           
            if (v & SIGN_MASK)
                value = -((v & VALUE_MASK) * me->SCALE);
            else
                value = v * me->SCALE;
        }

        ReturnInt32Datum(devid, chn, value, rflags);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(c0642, "C0642",
                   NULL, NULL,
                   sizeof(c0642_privrec_t), c0642_params,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   c0642_init_d, NULL, c0642_rw_p);
