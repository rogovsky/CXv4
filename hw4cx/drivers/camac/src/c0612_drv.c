
#include "cxsd_driver.h"
#include "timeval_utils.h"

#include "drv_i/c0612_drv_i.h"


enum {ANY_A = 0};
enum {DEFAULT_D = 0, DEFAULT_T = 4};
enum
{
    TOU_HBT_USECS = 1000*1000,
    MAX_MES_USECS = (240 + 100)*1000,
};

typedef struct
{
    int             N;

    int             D;
    int             T;

    sl_tid_t        tid;
    struct timeval  dln; // DeadLiNe
    int             locked;
    int             cur;
    int             rqn;
    int             requested[C0612_CHAN_ADC_n_count];
} c0612_privrec_t;

static psp_paramdescr_t c0612_params[] =
{
    PSP_P_INT ("D", c0612_privrec_t, D, DEFAULT_D, 0, 1),
    PSP_P_INT ("T", c0612_privrec_t, T, DEFAULT_T, 0, 7),
    PSP_P_END()
};

static void LAM_CB(int devid, void *devptr);
static void TOU_CB(int devid, void *devptr, sl_tid_t tid, void *privptr);

static int c0612_init_d(int devid, void *devptr,
                        int businfocount, int *businfo,
                        const char *auxinfo)
{
  c0612_privrec_t *me = (c0612_privrec_t*)devptr;

  const char      *errstr;
  int              cword;

    me->N = businfo[0];

    SetChanRDs  (devid, C0612_CHAN_ADC_n_base, C0612_CHAN_ADC_n_count, 1000000.0, 0.0);

    me->cur = -1;
    me->rqn = 0;

    cword = 0;
    DO_NAF(CAMAC_REF, me->N, 3, 16, &cword);   // Stop, reset L

    if ((errstr = WATCH_FOR_LAM(devid, devptr, me->N, LAM_CB)) != NULL)
    {
        DoDriverLog(devid, DRIVERLOG_ERRNO, "WATCH_FOR_LAM(): %s", errstr);
        return -CXRF_DRV_PROBL;
    }

    me->tid = sl_enq_tout_after(devid, devptr, TOU_HBT_USECS, TOU_CB, NULL);

    return DEVSTATE_OPERATING;
}

static void StartMeasurement(int devid, c0612_privrec_t *me, int x)
{
  int              cword;
  int              junk;

    me->cur = x;

    cword = (me->D<<3) | me->T;
    DO_NAF(CAMAC_REF, me->N, 3, 16, &cword);   // Set cword, stop, reset L
    DO_NAF(CAMAC_REF, me->N, 2, 16, &x);       // Set "from"
    DO_NAF(CAMAC_REF, me->N, 1, 16, &x);       // Set "to"
    DO_NAF(CAMAC_REF, me->N, 0, 25, &junk);    // Start measurement

    gettimeofday     (&(me->dln), NULL);
    timeval_add_usecs(&(me->dln), &(me->dln), MAX_MES_USECS); // Shouldn't we use per-T times?
}

static void ReadAndReturnMeasurement(int devid, c0612_privrec_t *me, rflags_t rflags)
{
  int32            value;

  int              v;

  int              this_cur;
  int              left;
  int              x;

    x = me->cur;
    rflags |= status2rflags(DO_NAF(CAMAC_REF, me->N, 1, 16, &x));  // Set address, stop, reset L
    rflags |= status2rflags(DO_NAF(CAMAC_REF, me->N, 0, 0,  &v));  // Read the code

    if (me->cur < 0) return;  // A guard against "spontaneous"/inherited LAMs

    /* Overload? */
    if (v == 0x80000) // !!! Why 0x80000? Shouldn't it be 0x7ffff?
    {
        value   = 100000000; // 100V
        rflags |= CXRF_OVERLOAD;
    }
    else
    {
        /* Convert the value */

        /* 1. Do sign extension (20bits->bits_of(int) */
        if (v > 0x7FFFF) v -= 1048576; // v |= 0xFFF00000

        /* 2. Scale to uV */
        value = (2000 * v) / (1 << (me->T + (4*me->D)));
    }

    // Drop request
    me->requested[me->cur] = 0;
    me->rqn--;
    this_cur = me->cur;
    me->cur = -1;

    /*!!! A theoretically-possible race condition: if working as
          a driver INSIDE server, if another _rw request is issued
          while inside ReturnChanGroup() -- cur,rqn,requested[] operation
          will be affected.
          Thus, a "lock" is added, so that _rw_p() wouldn't
          call StartMeasurement() if "locked". */
    me->locked++;
    ReturnInt32Datum(devid, this_cur, value, rflags);
    me->locked--;

    /* Anything more to measure? */
    if (me->rqn > 0)
        for (left = C0612_CHAN_ADC_n_count, x = this_cur + 1;
             left > 0;
             left--,                        x++)
        {
            if (x >= C0612_CHAN_ADC_n_count) x = 0; // Loop/wrap
            if (me->requested[x])
            {
                StartMeasurement(devid, me, x);
                break;
            }
        }
}

static void LAM_CB(int devid, void *devptr)
{
//    DoDriverLog(devid, 0, "LAM_CB()!");
    fprintf(stderr, "LAM!\n");
    ReadAndReturnMeasurement(devid, devptr, 0);
}

static void TOU_CB(int devid, void *devptr, sl_tid_t tid, void *privptr)
{
  c0612_privrec_t *me = (c0612_privrec_t*)devptr;
  struct timeval   now;

    me->tid = -1;
    if (me->cur >= 0)
    {
        gettimeofday(&now, NULL);
        if (TV_IS_AFTER(now, me->dln))
            ReadAndReturnMeasurement(devid, devptr, CXRF_IO_TIMEOUT);
    }
    me->tid = sl_enq_tout_after(devid, devptr, TOU_HBT_USECS, TOU_CB, privptr);
}

static void c0612_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  c0612_privrec_t *me = (c0612_privrec_t*)devptr;

  int              n;
  int              chn;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];

        if (chn >= C0612_CHAN_ADC_n_count)
        {
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);
        }
        else if (me->requested[chn] == 0)
        {
            me->requested[chn] = 1;
            me->rqn++;
            if (me->rqn == 1  &&  me->locked == 0)
                StartMeasurement(devid, me, chn);
        }
        /* else do nothing */

    }
}


DEFINE_CXSD_DRIVER(c0612, "C0612",
                   NULL, NULL,
                   sizeof(c0612_privrec_t), c0612_params,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   c0612_init_d, NULL, c0612_rw_p);
