#include <stdio.h>

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/liu_doorilks_drv_i.h"


enum
{
    INPRB0,
    INPRB1,
    INPRB2,
    INPRB3,
    SUBORD_NUMCHANS
};

static vdev_sodc_dsc_t hw2our_mapping[SUBORD_NUMCHANS] =
{
    [INPRB0] = {"inprb0", VDEV_TUBE, LIU_DOORILKS_CHAN_ILK_n_base + 0},
    [INPRB1] = {"inprb1", VDEV_TUBE, LIU_DOORILKS_CHAN_ILK_n_base + 1},
    [INPRB2] = {"inprb2", VDEV_TUBE, LIU_DOORILKS_CHAN_ILK_n_base + 2},
    [INPRB3] = {"inprb3", VDEV_TUBE, LIU_DOORILKS_CHAN_ILK_n_base + 3},
};

static const char *devstate_names[] = {"_devstate"};

typedef struct {
    int                 devid;

    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];

    int32               mode;
    int32               used_vals[LIU_DOORILKS_ILK_count];
} privrec_t;

static int32 modes[][LIU_DOORILKS_CHAN_ILK_n_base] =
{
    {0, 0, 0, 0},  // None
    {1, 1, 1, 1},  // Work
};

//--------------------------------------------------------------------

static void calc_ilk_state(privrec_t *me)
{
  int32      val;
  rflags_t   rflags;
  cx_time_t  ts;
  int        ts_got;
  int        ilk;

    for (ilk = 0, val = 1, rflags = 0, ts_got = 0;
         ilk < LIU_DOORILKS_ILK_count;
         ilk++)
        if (me->used_vals[ilk])
        {
            val    &= me->cur[INPRB0 + ilk].v.i32;
            rflags |= me->cur[INPRB0 + ilk].flgs;
            if (ts_got)
            {
                if (CX_TIME_IS_AFTER(ts, me->cur[INPRB0 + ilk].ts))
                    ts = me->cur[INPRB0 + ilk].ts;
            }
            else
            {
                ts = me->cur[INPRB0 + ilk].ts;
                ts_got = 1;
            }
        }

    if (ts_got)
        ReturnInt32DatumTimed(me->devid, LIU_DOORILKS_CHAN_SUM_STATE,
                              val, rflags, ts);
    else
        ReturnInt32Datum     (me->devid, LIU_DOORILKS_CHAN_SUM_STATE,
                              val, rflags);
}

static void set_ilk_mode  (privrec_t *me, int mode)
{
  int  ilk;

    me->mode = mode;
    for (ilk = 0;  ilk < LIU_DOORILKS_ILK_count;  ilk++)
    {
        me->used_vals[ilk] = modes[mode][ilk];
        ReturnInt32Datum(me->devid, LIU_DOORILKS_CHAN_USED_n_base + ilk,
                         me->used_vals[ilk], 0);
    }
    calc_ilk_state(me);
}

//--------------------------------------------------------------------

static void liu_doorilks_sodc_cb(int devid, void *devptr,
                                 int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;

    if (sodc >= INPRB0  &&  sodc <= INPRB3)
        calc_ilk_state(me);
}

static int liu_doorilks_init_d(int devid, void *devptr,
                               int businfocount, int businfo[],
                               const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;

    me->devid = devid;

    SetChanReturnType(devid, LIU_DOORILKS_CHAN_SUM_STATE,  1,
                      IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, LIU_DOORILKS_CHAN_ILK_n_base, LIU_DOORILKS_ILK_count,
                      IS_AUTOUPDATED_YES);
    set_ilk_mode(me, LIU_DOORILKS_MODE_TEST);

    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = hw2our_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = NULL;
    me->ctx.sodc_cb        = liu_doorilks_sodc_cb;

    return vdev_init(&(me->ctx), devid, devptr, -1, auxinfo);
}

static void liu_doorilks_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void liu_doorilks_rw_p(int devid, void *devptr,
                              int action,
                              int count, int *addrs,
                              cxdtype_t *dtypes, int *nelems, void **values) 
{
  privrec_t *me = devptr;
  int             n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int             chn;   // channel
  int32           val;
  int             ilk;

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

        if      (chn == LIU_DOORILKS_CHAN_SUM_STATE  ||
                 (chn >= LIU_DOORILKS_CHAN_ILK_n_base  &&
                  chn <  LIU_DOORILKS_CHAN_ILK_n_base + LIU_DOORILKS_ILK_count))
        {
            // Do-nothing -- those are returned upon events
        }
        else if (chn == LIU_DOORILKS_CHAN_MODE)
        {
            if (action == DRVA_WRITE)
            {
                if (val < 0)              val = 0;
                if (val > countof(modes)) val = countof(modes);
                if (val != me->mode) set_ilk_mode(me, val);
            }
            ReturnInt32Datum(devid, chn, me->mode, 0);
        }
        else if (chn >= LIU_DOORILKS_CHAN_USED_n_base  &&
                 chn <  LIU_DOORILKS_CHAN_USED_n_base + LIU_DOORILKS_ILK_count)
        {
            ilk = chn - LIU_DOORILKS_CHAN_USED_n_base;
            if (action == DRVA_WRITE)
            {
                me->used_vals[ilk] = (val != 0);
                calc_ilk_state(me);
            }
            ReturnInt32Datum(devid, chn, me->used_vals[ilk], 0);
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(liu_doorilks, "LIU2 door-interlocks driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   liu_doorilks_init_d,
                   liu_doorilks_term_d,
                   liu_doorilks_rw_p);
