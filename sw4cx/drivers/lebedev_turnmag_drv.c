#include <stdio.h>

#include "timeval_utils.h"

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/lebedev_turnmag_drv_i.h"


enum
{
    C_GO_BURY,
    C_GO_RING,
    C_INPREG8,
    C_I_MOTOR,
    SUBORD_NUMCHANS
};

static vdev_sodc_dsc_t hw2our_mapping[SUBORD_NUMCHANS] =
{
    [C_GO_BURY] = {"outrb6", VDEV_PRIV, -1},
    [C_GO_RING] = {"outrb7", VDEV_PRIV, -1},
    [C_INPREG8] = {"inpr8b", VDEV_PRIV, -1},
    [C_I_MOTOR] = {"adc31",  VDEV_TUBE, LEBEDEV_TURNMAG_CHAN_I_MOTOR},
};

static const char *devstate_names[] = {"_devstate"};

enum 
{
    SEL_WAIT_TIME   = 300,  // 300s to wait while stepping motor moves
    CHAN_TIME_R     = 1000, // channel<->seconds coeff
};

enum
{
    UV2I = 1000000, /* 1000000uv/A */
};

typedef struct {
    int                 devid;

    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];

    int32               i_lim;
    int32               i_lim_time;

    int32               n_time;

    sl_tid_t            tid;
    struct timeval      time_endwait;

    struct timeval      time_exc_start;

    int                 target;
} privrec_t;

static inline void SndCVal(privrec_t *me, int sodc, int32 val)
{
    cda_snd_ref_data(me->cur[sodc].ref, CXDTYPE_INT32, 1, &val);
}

//--------------------------------------------------------------------

static void DoStop (privrec_t *me)
{
    if (me->tid >= 0)
    {
        sl_deq_tout(me->tid);
        me->tid = -1;
    }
    me->time_endwait.tv_sec = 0;

    me->target = 0;

    SndCVal(me, C_GO_BURY, 0);
    SndCVal(me, C_GO_RING, 0);
    ReturnInt32Datum(me->devid, LEBEDEV_TURNMAG_CHAN_GO_TIME_LEFT, 0, 0);
    ReturnInt32Datum(me->devid, LEBEDEV_TURNMAG_CHAN_GOING_DIR,    0, 0);
}

static void tout_p(int devid, void *devptr,
                   sl_tid_t tid __attribute__((unused)),
                   void *privptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    DoStop(me);
}

static void DoStart(privrec_t *me, int duration, int dir)
{
  struct timeval  delta;

    if (me->tid >= 0)
    {
        sl_deq_tout(me->tid);
        me->tid = -1;
    }

    if (duration > 0)
    {
////    fprintf(stderr, "duration=%d\n", duration);
        gettimeofday(&(me->time_endwait), NULL);
        delta.tv_sec  =  duration / CHAN_TIME_R;
        delta.tv_usec = (duration % CHAN_TIME_R) * (1000000 / CHAN_TIME_R);
        timeval_add(&(me->time_endwait), &(me->time_endwait), &delta);
        ReturnInt32Datum(me->devid, LEBEDEV_TURNMAG_CHAN_GO_TIME_LEFT,
                         duration, 0);
        me->tid = sl_enq_tout_at(me->devid, me,
                                 &(me->time_endwait), tout_p, NULL);
    }

    me->target = (dir << 1) - 1;

    SndCVal(me, C_GO_BURY, dir == 0? 1 : 0);
    SndCVal(me, C_GO_RING, dir == 1? 1 : 0);
    ReturnInt32Datum(me->devid, LEBEDEV_TURNMAG_CHAN_GOING_DIR, me->target, 0);
}

static void lebedev_turnmag_sodc_cb(int devid, void *devptr,
                                    int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;
  int             lim_ring;
  int             lim_bury;
  struct timeval  now;
  struct timeval  delta;
  struct timeval  limit;

    if      (sodc == C_INPREG8)
    {
        ReturnInt32DatumTimed(devid, LEBEDEV_TURNMAG_CHAN_CUR_DIRECTION,
                              (val >> 6) & 0x3,
                              me->cur[sodc].flgs, me->cur[sodc].ts);
        /* Note: we return INVERTED values (val=0: lim_sw is ON) */
        lim_bury = (val & (1 << 6)) == 0;
        lim_ring = (val & (1 << 7)) == 0;
        ReturnInt32DatumTimed(devid, LEBEDEV_TURNMAG_CHAN_LIM_SW_BURY,
                              lim_bury,
                              me->cur[sodc].flgs, me->cur[sodc].ts);
        ReturnInt32DatumTimed(devid, LEBEDEV_TURNMAG_CHAN_LIM_SW_RING,
                              lim_ring,
                              me->cur[sodc].flgs, me->cur[sodc].ts);

        if ((lim_bury  &&  me->target < 0)  ||
            (lim_ring  &&  me->target > 0))
            DoStop(me);
    }
    else if (sodc == C_I_MOTOR)
    {
        if (me->time_exc_start.tv_sec == 0)
        {
            if (abs(val) > me->i_lim)
            {
                gettimeofday(&(me->time_exc_start), NULL);
            }
        }
        else
        {
            if (abs(val) > me->i_lim)
            {
                gettimeofday(&now, NULL);
                timeval_subtract(&delta, &now, &(me->time_exc_start));
                limit.tv_sec  =  me->i_lim_time / CHAN_TIME_R;
                limit.tv_usec = (me->i_lim_time % CHAN_TIME_R) * (1000000 / CHAN_TIME_R);
                if (TV_IS_AFTER(delta, limit))
                    DoStop(me);
            }
            else
            {
                me->time_exc_start.tv_sec = 0;
                ReturnInt32Datum(devid, LEBEDEV_TURNMAG_CHAN_I_EXC_TIME, 0, 0);
            }
        }
    }
}

static int lebedev_turnmag_init_d(int devid, void *devptr,
                                  int businfocount, int businfo[],
                                  const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;
  int             pos;

    me->devid      = devid;

    me->i_lim      = 1.5 * UV2I;
    me->i_lim_time = 1.0 * CHAN_TIME_R;

    me->n_time     = 1.0 * CHAN_TIME_R;

    SetChanRDs(devid, LEBEDEV_TURNMAG_CHAN_I_LIM,        1, UV2I,        0); /*!!! Should tube/tunnel source channel's */
    SetChanRDs(devid, LEBEDEV_TURNMAG_CHAN_I_MOTOR,      1, UV2I,        0); /*!!! Should tube/tunnel source channel's */
    SetChanRDs(devid, LEBEDEV_TURNMAG_CHAN_I_LIM_TIME,   1, CHAN_TIME_R, 0);
    SetChanRDs(devid, LEBEDEV_TURNMAG_CHAN_I_EXC_TIME,   1, CHAN_TIME_R, 0);
    SetChanRDs(devid, LEBEDEV_TURNMAG_CHAN_N_S,          1, CHAN_TIME_R, 0);
    SetChanRDs(devid, LEBEDEV_TURNMAG_CHAN_GO_TIME_LEFT, 1, CHAN_TIME_R, 0);

    SetChanReturnType(devid, LEBEDEV_TURNMAG_CHAN_CUR_DIRECTION, 1,
                      IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, LEBEDEV_TURNMAG_CHAN_LIM_SW_base,   2,
                      IS_AUTOUPDATED_YES);
//    SetChanReturnType(devid, LEBEDEV_TURNMAG_CHAN_GOING_DIR,     1,
//                      IS_AUTOUPDATED_TRUSTED);

    ReturnInt32Datum(devid, LEBEDEV_TURNMAG_CHAN_GOING_DIR, 0, 0);

    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = hw2our_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = NULL;
    me->ctx.sodc_cb        = lebedev_turnmag_sodc_cb;

    return vdev_init(&(me->ctx), devid, devptr, -1, auxinfo);
}

static void lebedev_turnmag_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void lebedev_turnmag_rw_p(int devid, void *devptr,
                                 int action,
                                 int count, int *addrs,
                                 cxdtype_t *dtypes, int *nelems, void **values) 
{
  privrec_t      *me = devptr;
  int             n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int             chn;   // channel
  int32           val;

  struct timeval  now;
  struct timeval  timediff;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
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

        if      (chn == LEBEDEV_TURNMAG_CHAN_I_LIM)
        {
            if (action == DRVA_WRITE)
            {
                if (val < 0)         val = 0;
                if (val > 10 * UV2I) val = 10 * UV2I;
                me->i_lim = val;
            }
            ReturnInt32Datum(devid, chn, me->i_lim, 0);
        }
        else if (chn == LEBEDEV_TURNMAG_CHAN_I_LIM_TIME)
        {
            if (action == DRVA_WRITE)
            {
                if (val < 0)               val = 0;
                if (val > 5 * CHAN_TIME_R) val = 5 * CHAN_TIME_R;
                me->i_lim_time = val;
            }
            ReturnInt32Datum(devid, chn, me->i_lim_time, 0);
        }
        else if (chn == LEBEDEV_TURNMAG_CHAN_GO_BURY_MAX  ||
                 chn == LEBEDEV_TURNMAG_CHAN_GO_RING_MAX)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                DoStart(me, -1, chn - LEBEDEV_TURNMAG_CHAN_GO_MAX_d_base);
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == LEBEDEV_TURNMAG_CHAN_GO_BURY_100MS  ||
                 chn == LEBEDEV_TURNMAG_CHAN_GO_RING_100MS)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                DoStart(me, 0.1 * CHAN_TIME_R, chn - LEBEDEV_TURNMAG_CHAN_GO_100_d_base);
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == LEBEDEV_TURNMAG_CHAN_GO_BURY_N_S  ||
                 chn == LEBEDEV_TURNMAG_CHAN_GO_RING_N_S)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                DoStart(me, me->n_time, chn - LEBEDEV_TURNMAG_CHAN_GO_N_S_d_base);
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == LEBEDEV_TURNMAG_CHAN_N_S)
        {
            if (action == DRVA_WRITE)
            {
                if (val < 1)                 val = 1;
                if (val > 999 * CHAN_TIME_R) val = 999 * CHAN_TIME_R;
                me->n_time = val;
            }
            ReturnInt32Datum(devid, chn, me->n_time, 0);
        }
        else if (chn == LEBEDEV_TURNMAG_CHAN_STOP)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                DoStop(me);
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == LEBEDEV_TURNMAG_CHAN_I_EXC_TIME)
        {
            if (me->time_exc_start.tv_sec == 0)
                ReturnInt32Datum(devid, chn, 0, 0);
            else
            {
                gettimeofday(&now, NULL);
                timeval_subtract(&timediff, &now, &(me->time_exc_start));
//fprintf(stderr, "%ld.%06ld - %ld.%06ld = %ld.%06ld\n", now.tv_sec, now.tv_usec, me->time_exc_start.tv_sec, me->time_exc_start.tv_usec, timediff.tv_sec, timediff.tv_usec);
                ReturnInt32Datum(devid, chn,
                                 timediff.tv_sec  * CHAN_TIME_R +
                                 timediff.tv_usec / (1000000 / CHAN_TIME_R),
                                 0);
            }
        }
        else if (chn == LEBEDEV_TURNMAG_CHAN_GO_TIME_LEFT)
        {
            gettimeofday(&now, NULL);
            ReturnInt32Datum(devid, chn,
                             (timeval_subtract(&timediff,
                                               &(me->time_endwait),
                                               &now)) ? 0
                                                      : timediff.tv_sec * CHAN_TIME_R + 
                                                        timediff.tv_usec / (1000000 / CHAN_TIME_R),
                             0);
        }
        else if (chn == LEBEDEV_TURNMAG_CHAN_GOING_DIR)
        {
            ReturnInt32Datum(devid, chn, me->target, 0);
        }

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(lebedev_turnmag, "Lebedev's Turning Magnet driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   lebedev_turnmag_init_d,
                   lebedev_turnmag_term_d,
                   lebedev_turnmag_rw_p);
