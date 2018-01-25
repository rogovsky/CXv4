#include <stdio.h>

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/iset_walker_drv_i.h"


enum
{
   C_OUT,
   C_OUT_CUR,

   SUBORD_NUMCHANS
};

static vdev_sodc_dsc_t hw2our_mapping[SUBORD_NUMCHANS] =
{
    [C_OUT]     = {"Iset",     VDEV_IMPR, -1},
    [C_OUT_CUR] = {"Iset_cur", VDEV_IMPR, -1},
};

static const char *devstate_names[] = {"_devstate"};

enum
{
    WORK_HEARTBEAT_PERIOD =    100000, // 100ms/10Hz
};

enum
{
    MAX_ALWD_VAL =   9999999,
    MIN_ALWD_VAL = -10000305
};

//--------------------------------------------------------------------

enum {MAX_STEPS = 20};

typedef struct
{
    int                 devid;

    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];

    int                 q;

    int                 cur_step;
    int                 steps_count;
    int32               steps[MAX_STEPS];
} privrec_t;

static inline void SndCVal(privrec_t *me, int sodc, int32 val)
{
    cda_snd_ref_data(me->cur[sodc].ref, CXDTYPE_INT32, 1, &val);
}

//--------------------------------------------------------------------

enum
{
    WLK_STATE_UNKNOWN = 0,
    WLK_STATE_STOPPED,
    WLK_STATE_WALKING,

    WLK_STATE_count
};

//--------------------------------------------------------------------

static void ReturnCommandChannels(privrec_t *me)
{
    ReturnInt32Datum(me->devid, ISET_WALKER_CHAN_START,
                     me->ctx.cur_state == WLK_STATE_STOPPED? 0 : CX_VALUE_DISABLED_MASK,
                     0);
    ReturnInt32Datum(me->devid, ISET_WALKER_CHAN_STOP,
                     me->ctx.cur_state == WLK_STATE_WALKING? 0 : CX_VALUE_DISABLED_MASK,
                     0);
}

static void StartStep(privrec_t *me)
{
    SndCVal(me, C_OUT, me->steps[me->cur_step]);
    ReturnInt32Datum(me->devid, ISET_WALKER_CHAN_CUR_STEP, me->cur_step, 0);
}

static void SwchToSTOPPED(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    me->cur_step = -1;

    ReturnInt32Datum(me->devid, ISET_WALKER_CHAN_CUR_STEP, me->cur_step, 0);
    ReturnCommandChannels(me);
}

static void SwchToWALKING(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

////fprintf(stderr, "%s count=%d\n", __FUNCTION__, me->steps_count);
    if (me->steps_count <= 0)
    {
        vdev_set_state(&(me->ctx), WLK_STATE_STOPPED);
        return;
    }
    me->cur_step = 0;
    StartStep(me);
    ReturnCommandChannels(me);
}

static vdev_state_dsc_t state_descr[] =
{
    [WLK_STATE_STOPPED] = {0, -1, NULL, SwchToSTOPPED, NULL},
    [WLK_STATE_WALKING] = {0, -1, NULL, SwchToWALKING, NULL},
};

//////////////////////////////////////////////////////////////////////

static void iset_walker_sodc_cb(int devid, void *devptr,
                                int sodc, int32 val);

static int iset_walker_init_d(int devid, void *devptr, 
                              int businfocount, int businfo[],
                              const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;
  const char     *p  = auxinfo;
  const char     *beg;
  size_t          len;
  char            buf[1000];
  int             q;
  char           *errp;

    me->devid = devid;
    me->q     = 305;

    if (p == NULL  ||  *p == '\0')
    {
        DoDriverLog(devid, 0,
                    "%s(): base-CDAC20-device name is required in auxinfo",
                    __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }
    beg = p;
    while (*p != '\0'  &&  !isspace(*p)) p++;
    len = p - beg;
    if (len > sizeof(buf) - 1)
        len = sizeof(buf) - 1;
    memcpy(buf, beg, len); buf[len] = '\0';

    while (*p != '0'  &&  isspace(*p)) p++;
    if (*p != '\0')
    {
        q = strtol(p, &errp, 10);
        if (errp != p  &&  me->q >= 0) me->q = q;
    }

    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = hw2our_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.sodc_cb        = iset_walker_sodc_cb;

    me->ctx.our_numchans             = ISET_WALKER_NUMCHANS;
    me->ctx.chan_state_n             = ISET_WALKER_CHAN_WALKER_STATE;
    me->ctx.state_unknown_val        = WLK_STATE_UNKNOWN;
    me->ctx.state_determine_val      = WLK_STATE_STOPPED;
    me->ctx.state_count              = countof(state_descr);
    me->ctx.state_descr              = state_descr;

    SetChanRDs       (devid, ISET_WALKER_CHAN_LIST,     1, 1000000.0, 0.0);
    SetChanReturnType(devid, ISET_WALKER_CHAN_CUR_STEP, 1, IS_AUTOUPDATED_TRUSTED);

    return vdev_init(&(me->ctx), devid, devptr, WORK_HEARTBEAT_PERIOD, buf);
}

static void iset_walker_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void iset_walker_sodc_cb(int devid, void *devptr,
                                int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;

    if (sodc == C_OUT_CUR  &&  me->ctx.cur_state == WLK_STATE_WALKING)
    {
        if (abs(val - me->steps[me->cur_step]) <= me->q)
        {
            me->cur_step++;
            if (me->cur_step < me->steps_count)
                StartStep(me);
            else
                vdev_set_state(&(me->ctx), WLK_STATE_STOPPED);
        }
    }
}

static void iset_walker_rw_p(int devid, void *devptr,
                             int action,
                             int count, int *addrs,
                             cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t          *me = (privrec_t *)devptr;
  int                 n;       // channel N in addrs[]/.../values[] (loop ctl var)
  int                 chn;     // channel
  int32               val;     // Value

#if __GNUC__ < 3
  int              r_addrs [1];
  cxdtype_t        r_dtypes[1];
  int              r_nelems[1];
  void            *r_values[1];
  static rflags_t  r_rflags[1] = {0};
#endif

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
////fprintf(stderr, "%s %d %d\n", __FUNCTION__, action, chn);
        if (action == DRVA_WRITE  &&  chn == ISET_WALKER_CHAN_LIST)
        {
            if ((dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            val = 0xFFFFFFFF; // That's just to make GCC happy
        }
        else
        if (action == DRVA_WRITE)
        {
            if (nelems[n] != 1  ||
                (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            val = *((int32*)(values[n]));
            ////fprintf(stderr, " write #%d:=%d\n", chn, val);
        }
        else
            val = 0xFFFFFFFF; // That's just to make GCC happy

        if      (chn == ISET_WALKER_CHAN_LIST)
        {
            if (action == DRVA_WRITE  &&
                me->ctx.cur_state != WLK_STATE_WALKING)
            {
                me->steps_count = nelems[n];
                if (me->steps_count < 0)
                    me->steps_count = 0;
                if (me->steps_count > countof(me->steps))
                    me->steps_count = countof(me->steps);
                if (me->steps_count != 0)
                    memcpy(me->steps, values[n],
                           sizeof(me->steps[0]) * me->steps_count);
            }
#if __GNUC__ < 3
            /* For some strange reason, RH-7.3's gcc-2.96 doesn't accept
               version below (in #else),
               firing an "invalid use of non-lvalue array" error x3 */
            r_addrs [0] = chn;
            r_dtypes[0] = CXDTYPE_INT32;
            r_nelems[0] = me->steps_count;
            r_values[0] = me->steps;
            ReturnDataSet(devid, 1,
                          r_addrs,
                          r_dtypes,
                          r_nelems,
                          r_values,
                          r_rflags,
                          NULL);
#else
            ReturnDataSet(devid, 1,
                          (int      []){chn},
                          (cxdtype_t[]){CXDTYPE_INT32},
                          (int      []){me->steps_count},
                          (void *   []){me->steps},
                          (rflags_t []){0},
                          NULL);
#endif
        }
        else if (chn == ISET_WALKER_CHAN_START)
        {
            if (action == DRVA_WRITE        &&
                val    == CX_VALUE_COMMAND  &&
                me->ctx.cur_state == WLK_STATE_STOPPED)
                vdev_set_state(&(me->ctx), WLK_STATE_WALKING);
            else
                ReturnInt32Datum(devid, chn,
                                 me->ctx.cur_state == WLK_STATE_STOPPED? 0 : CX_VALUE_DISABLED_MASK,
                                 0);
        }
        else if (chn == ISET_WALKER_CHAN_STOP)
        {
            if (action == DRVA_WRITE        &&
                val    == CX_VALUE_COMMAND  &&
                me->ctx.cur_state == WLK_STATE_WALKING)
                vdev_set_state(&(me->ctx), WLK_STATE_STOPPED);
            else
                ReturnInt32Datum(devid, chn,
                                 me->ctx.cur_state == WLK_STATE_WALKING? 0 : CX_VALUE_DISABLED_MASK,
                                 0);
        }
        else if (chn == ISET_WALKER_CHAN_CUR_STEP)
        {
            ReturnInt32Datum(devid, chn, me->cur_step, 0);
        }
        else if (chn == ISET_WALKER_CHAN_WALKER_STATE)
            ReturnInt32Datum(devid, chn, me->ctx.cur_state, 0);
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(iset_walker, "Iset value_list-walker",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   iset_walker_init_d, iset_walker_term_d, iset_walker_rw_p);
