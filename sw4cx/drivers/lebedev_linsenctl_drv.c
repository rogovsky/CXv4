#include <stdio.h>

#include "timeval_utils.h"

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/lebedev_linsenctl_drv_i.h"


enum
{
    CAN_BIT,
    SUBORD_NUMCHANS
};

static vdev_sodc_dsc_t hw2our_mapping[SUBORD_NUMCHANS] =
{
    [CAN_BIT] = {"", VDEV_PRIV, -1},
};

static const char *devstate_names[] = {"_devstate"};

enum 
{
    SEL_WAIT_TIME   = 40,   // 40s to wait while stepping motor moves
    CHAN_TIME_R     = 1000, // channel<->seconds coeff
};

typedef struct {
    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];

    sl_tid_t            tid;
    struct timeval      time_endwait;
} privrec_t;

static void lebedev_linsenctl_sodc_cb(int devid, void *devptr,
                                      int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;
  struct timeval  now;
  struct timeval  timediff;

    val = val != 0; // Booleanize
    gettimeofday(&now, NULL);
    if (timeval_subtract(&timediff, &(me->time_endwait), &now) == 0)
        val |= CX_VALUE_DISABLED_MASK;

    ReturnInt32DatumTimed(devid, LEBEDEV_LINSENCTL_CHAN_IN_STATE,
                          val,
                          me->cur[CAN_BIT].flgs,
                          me->cur[CAN_BIT].ts);
}

static void tout_p(int devid, void *devptr,
                   sl_tid_t tid __attribute__((unused)),
                   void *privptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    me->tid = -1;
    lebedev_linsenctl_sodc_cb(devid, devptr, CAN_BIT, me->cur[CAN_BIT].v.i32);
}

static int lebedev_linsenctl_init_d(int devid, void *devptr,
                                    int businfocount, int businfo[],
                                    const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;

    SetChanRDs(devid, LEBEDEV_LINSENCTL_CHAN_TIME_LEFT, 1, CHAN_TIME_R, 0);

    me->tid = -1;

    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = hw2our_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = NULL;
    me->ctx.sodc_cb        = lebedev_linsenctl_sodc_cb;

    return vdev_init(&(me->ctx), devid, devptr, -1, auxinfo);
}

static void lebedev_linsenctl_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void lebedev_linsenctl_rw_p(int devid, void *devptr,
                                   int action,
                                   int count, int *addrs,
                                   cxdtype_t *dtypes, int *nelems, void **values) 
{
  privrec_t      *me = devptr;
  int             n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int             chn;   // channel
  int32           bitval;

  struct timeval  now;
  struct timeval  timediff;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];

        if      (chn == LEBEDEV_LINSENCTL_CHAN_IN_STATE)
        {
            if (action == DRVA_WRITE  &&
                (dtypes[0] == CXDTYPE_INT32  ||  dtypes[0] == CXDTYPE_UINT32))
            {
                bitval = (*((int32*)(values[0]))) != 0; // Booleanize
                cda_snd_ref_data(me->cur[CAN_BIT].ref, CXDTYPE_INT32, 1, &bitval);

                gettimeofday(&(me->time_endwait), NULL);
                me->time_endwait.tv_sec += SEL_WAIT_TIME;
                ReturnInt32Datum(devid, LEBEDEV_LINSENCTL_CHAN_TIME_LEFT,
                                 SEL_WAIT_TIME * CHAN_TIME_R, 0);
                if (me->tid >= 0)
                {
                    sl_deq_tout(me->tid);
                    me->tid = -1;
                }
                me->tid = sl_enq_tout_at(devid, me,
                                         &(me->time_endwait), tout_p, NULL);
            }
        }
        else if (chn == LEBEDEV_LINSENCTL_CHAN_TIME_LEFT)
        {
            gettimeofday(&now, NULL);
            ReturnInt32Datum(devid, LEBEDEV_LINSENCTL_CHAN_TIME_LEFT,
                             (timeval_subtract(&timediff,
                                               &(me->time_endwait),
                                               &now)) ? 0
                                                      : timediff.tv_sec * CHAN_TIME_R + 
                                                        timediff.tv_usec / (1000000 / CHAN_TIME_R),
                             0);
        }
    }
}


DEFINE_CXSD_DRIVER(lebedev_linsenctl, "Lebedev's Linac-sensor control driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   lebedev_linsenctl_init_d,
                   lebedev_linsenctl_term_d,
                   lebedev_linsenctl_rw_p);
