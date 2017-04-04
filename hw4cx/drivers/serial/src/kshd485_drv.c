#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "cxsd_driver.h"
#include "piv485_lyr.h"
#include "drv_i/kshd485_drv_i.h"


/**** KSHD485 instruction set description ***************************/

/* KSHD485 commands */
enum
{
    KCMD_GET_INFO      = 1,
    KCMD_REPEAT_REPLY  = 2,
    KCMD_GET_STATUS    = 3,
    KCMD_GO            = 4,
    KCMD_GO_WO_ACCEL   = 5,
    KCMD_SET_CONFIG    = 6,
    KCMD_SET_SPEEDS    = 7,
    KCMD_STOP          = 8,
    KCMD_SWITCHOFF     = 9,
    KCMD_SAVE_CONFIG   = 10,
    KCMD_PULSE         = 11,
    KCMD_GET_REMSTEPS  = 12,
    KCMD_GET_CONFIG    = 13,
    KCMD_GET_SPEEDS    = 14,
    KCMD_TEST_FREQ     = 15,
    KCMD_CALIBR_PREC   = 16,
    KCMD_GO_PREC       = 17,
    KCMD_OUT_IMPULSE   = 18,
    KCMD_SET_ABSCOORD  = 19,
    KCMD_GET_ABSCOORD  = 20,
    KCMD_SYNC_MULTIPLE = 21,
    KCMD_SAVE_USERDATA = 22,
    KCMD_RETR_USERDATA = 23,

    /* Not a real command */
    KCMD_0xFF = 0xFF
};

/* STOPCOND bits (for GO and GO_WO_ACCEL commands */
enum
{
    KSHD485_STOPCOND_KP = 1 << 2,
    KSHD485_STOPCOND_KM = 1 << 3,
    KSHD485_STOPCOND_S0 = 1 << 4,
    KSHD485_STOPCOND_S1 = 1 << 5
};

/**/
enum
{
    KSHD485_CONFIG_HALF     = 1 << 0,
    KSHD485_CONFIG_KM       = 1 << 2,
    KSHD485_CONFIG_KP       = 1 << 3,
    KSHD485_CONFIG_SENSOR   = 1 << 4,
    KSHD485_CONFIG_SOFTK    = 1 << 5,
    KSHD485_CONFIG_LEAVEK   = 1 << 6,
    KSHD485_CONFIG_ACCLEAVE = 1 << 7
};

/********************************************************************/

enum {KSHD485_OUTQUEUESIZE = 15};


typedef struct
{
    piv485_vmt_t *lvmt
;
    int           devid;
    int           handle;

    struct
    {
        int  info;
        int  config;
        int  speeds;
    }             got;

    int32         cache[KSHD485_NUMCHANS];
} privrec_t;

static inline int IsReady(privrec_t *me)
{
    return (me->cache[KSHD485_CHAN_STATUS_BYTE] & (1 << 0/*!!!READY*/)) != 0;
}

static inline int DevVersion(privrec_t *me)
{
    return me->cache[KSHD485_CHAN_DEV_VERSION];
}

//////////////////////////////////////////////////////////////////////

static int RequestKshdParams(privrec_t *me)
{
    me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 1, KCMD_GET_INFO);
    me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 1, KCMD_GET_STATUS);
    me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 1, KCMD_GET_CONFIG);
    me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 1, KCMD_GET_SPEEDS);

    return 0;
}

static void UploadConfig(privrec_t *me)
{
    me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 5,
                      KCMD_SET_CONFIG,
                      me->cache[KSHD485_CHAN_WORK_CURRENT],
                      me->cache[KSHD485_CHAN_HOLD_CURRENT],
                      me->cache[KSHD485_CHAN_HOLD_DELAY],
                      me->cache[KSHD485_CHAN_CONFIG_BYTE]);
}

static void SetConfigByte(privrec_t *me, uint32 val, uint32 mask, int immed)
{
  int  n;

    me->cache[KSHD485_CHAN_CONFIG_BYTE] =
        (me->cache[KSHD485_CHAN_CONFIG_BYTE] &~ mask) |
        (val                                 &  mask);
    ReturnInt32Datum(me->devid, KSHD485_CHAN_CONFIG_BYTE,
                     me->cache [KSHD485_CHAN_CONFIG_BYTE], 0);

    for (n = 0;  n <= 7;  n++)
        if ((mask & (1 << n)) != 0)
        {
            me->cache[KSHD485_CHAN_CONFIG_BIT_base + n] =
                (val >> n) & 1;
            ReturnInt32Datum(me->devid, KSHD485_CHAN_CONFIG_BIT_base + n,
                             me->cache [KSHD485_CHAN_CONFIG_BIT_base + n], 0);
        }

    if (immed  &&  me->got.config) UploadConfig(me);
}

static void SetStopCond(privrec_t *me, uint32 val, uint32 mask)
{
  int  n;

    me->cache[KSHD485_CHAN_STOPCOND_BYTE] =
        (me->cache[KSHD485_CHAN_STOPCOND_BYTE] &~ mask) |
        (val                                   &  mask);
    ReturnInt32Datum(me->devid, KSHD485_CHAN_STOPCOND_BYTE,
                     me->cache [KSHD485_CHAN_STOPCOND_BYTE], 0);

    for (n = 0;  n <= 7;  n++)
        if ((mask & (1 << n)) != 0)
        {
            me->cache[KSHD485_CHAN_STOPCOND_BIT_base + n] = (val >> n) & 1;
            ReturnInt32Datum(me->devid, KSHD485_CHAN_STOPCOND_BIT_base + n,
                             me->cache [KSHD485_CHAN_STOPCOND_BIT_base + n], 0);
        }
}

static int SendGo(privrec_t *me, int cmd, int32 steps, uint8 stopcond)
{
    if (1)
    {
        me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 5,
                          KCMD_SET_CONFIG,
                          me->cache[KSHD485_CHAN_WORK_CURRENT],
                          me->cache[KSHD485_CHAN_HOLD_CURRENT],
                          me->cache[KSHD485_CHAN_HOLD_DELAY],
                          me->cache[KSHD485_CHAN_CONFIG_BYTE]);
        me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 7,
                          KCMD_SET_SPEEDS,
                          (me->cache[KSHD485_CHAN_MIN_VELOCITY] >> 8) & 0xFF,
                          (me->cache[KSHD485_CHAN_MIN_VELOCITY]     ) & 0xFF,
                          (me->cache[KSHD485_CHAN_MAX_VELOCITY] >> 8) & 0xFF,
                          (me->cache[KSHD485_CHAN_MAX_VELOCITY]     ) & 0xFF,
                          (me->cache[KSHD485_CHAN_ACCELERATION] >> 8) & 0xFF,
                          (me->cache[KSHD485_CHAN_ACCELERATION]     ) & 0xFF);
        me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 6,
                          cmd,
                          (steps >> 24) & 0xFF,
                          (steps >> 16) & 0xFF,
                          (steps >>  8) & 0xFF,
                           steps        & 0xFF,
                           stopcond);

        return 1;
    }

    return 0;
}

static int kshd485_is_status_req_cmp_func(int dlc, uint8 *data,
                                          void *privptr __attribute__((unused)))
{
    return data[0] >= KCMD_GET_STATUS  &&  data[0] <= KCMD_PULSE;
}
static int StatusReqInQueue(privrec_t *me)
{
    return me->lvmt->q_foreach(me->handle,
                               kshd485_is_status_req_cmp_func, NULL,
                               SQ_FIND_FIRST) != SQ_NOTFOUND;
}

//////////////////////////////////////////////////////////////////////

typedef struct
{
    int32  steps;
    int32  stopcond;
} kshd485_initvals_t;

static psp_paramdescr_t text2initvals[] =
{
    PSP_P_INT("steps",    kshd485_initvals_t, steps,    1000, 0, 0),
    PSP_P_INT("stopcond", kshd485_initvals_t, stopcond, 0,    0, 255),
    PSP_P_END()
};

static void kshd485_in(int devid, void *devptr,
                       int cmd, int dlc, uint8 *data);
static void kshd485_md(int devid, void *devptr,
                       int *dlc_p, uint8 *data);

static int kshd485_init_d(int devid, void *devptr,
                          int businfocount, int businfo[],
                          const char *auxinfo)
{
  privrec_t          *me = (privrec_t *)devptr;

  kshd485_initvals_t  opts;

    /*!!! Parse init-vals */
    if (psp_parse(auxinfo, NULL,
                  &(opts),
                  '=', " \t", "",
                  text2initvals) != PSP_R_OK)
    {
        DoDriverLog(devid, 0, "psp_parse(auxinfo): %s", psp_error());
        return -CXRF_CFG_PROBL;
    }
    me->cache[KSHD485_CHAN_STOPCOND_BYTE] = opts.stopcond;
    me->cache[KSHD485_CHAN_NUM_STEPS]     = opts.steps;

    me->lvmt   = GetLayerVMT(devid);
    me->devid  = devid;
    me->handle = me->lvmt->add(devid, me,
                               businfocount, businfo,
                               kshd485_in, kshd485_md,
                               KSHD485_OUTQUEUESIZE);
    if (me->handle < 0) return me->handle;

    RequestKshdParams(me);

    SetChanRDs(devid, KSHD485_CHAN_DEV_VERSION, 1, 10.0, 0.0);

    return DEVSTATE_OPERATING;
}

static void kshd485_rw_p(int devid, void *devptr,
                         int action,
                         int count, int *addrs,
                         cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t      *me = (privrec_t *)devptr;
  int             n;        // channel N in values[] (loop ctl var)
  int             chn;      // channel indeX
  int32           val;      // Value
  rflags_t        flags;
  int32           nsteps;
  int32           stopcond;

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

        if      (chn == KSHD485_CHAN_DEV_VERSION  ||
                 chn == KSHD485_CHAN_DEV_SERIAL)
        {
            if (me->got.info)
            {
                val   = me->cache[chn];
                flags = 0;
            }
            else
            {
                val   = -1;
                flags = CXRF_IO_TIMEOUT;
            }
            ReturnInt32Datum(devid, chn, val, flags);
        }
        else if (chn == KSHD485_CHAN_STEPS_LEFT)
        {
            if (0)
                me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 1,
                                  KCMD_GET_REMSTEPS);
        }
        else if (chn == KSHD485_CHAN_ABSCOORD  &&  DevVersion(me) >= 21)
        {
            if (action == DRVA_WRITE)
            {
                if (me->lvmt->q_enqueue_v(me->handle, SQ_REPLACE_NOTFIRST,
                                          SQ_TRIES_INF, 0,
                                          NULL, NULL,
                                          1, 5,
                                          KCMD_SET_ABSCOORD,
                                          (val >> 24) & 0xFF,
                                          (val >> 16) & 0xFF,
                                          (val >>  8) & 0xFF,
                                           val        & 0xFF) == SQ_NOTFOUND)
                    me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 1,
                                      KCMD_GET_ABSCOORD);
            }
            else
                me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                                  KCMD_GET_ABSCOORD);
        }
        else if (chn == KSHD485_CHAN_STATUS_BYTE   ||
                 (chn >= KSHD485_CHAN_STATUS_base  &&
                  chn <= KSHD485_CHAN_STATUS_base + 7))
        {
            if (!StatusReqInQueue(me))
                me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 1,
                                  KCMD_GET_STATUS);
        }
        else if (chn == KSHD485_CHAN_WORK_CURRENT  ||
                 chn == KSHD485_CHAN_HOLD_CURRENT  ||
                 chn == KSHD485_CHAN_HOLD_DELAY)
        {
            /*!!!MIN/MAX!!!*/
            if (action == DRVA_WRITE)
                me->cache[chn] = val;
            if (me->got.config)
            {
                ReturnInt32Datum(devid, chn, me->cache[chn], 0);
                UploadConfig(me);
            }
        }
        else if (chn == KSHD485_CHAN_MIN_VELOCITY  ||
                 chn == KSHD485_CHAN_MAX_VELOCITY  ||
                 chn == KSHD485_CHAN_ACCELERATION)
        {
            /*!!!MIN/MAX!!!*/
            if (action == DRVA_WRITE)
                me->cache[chn] = val;
            if (me->got.speeds)
                ReturnInt32Datum(devid, chn, me->cache[chn], 0);
        }
        else if (chn == KSHD485_CHAN_CONFIG_BYTE  ||
                 (chn >= KSHD485_CHAN_CONFIG_BIT_base  &&
                  chn <= KSHD485_CHAN_CONFIG_BIT_base + 7))
        {
            if (action == DRVA_WRITE)
            {
                if (chn == KSHD485_CHAN_CONFIG_BYTE)
                    SetConfigByte(me, val, 0xFF, 1);
                else
                    SetConfigByte(me,
                                  (val != 0) << (chn - KSHD485_CHAN_CONFIG_BIT_base),
                                  (1)        << (chn - KSHD485_CHAN_CONFIG_BIT_base),
                                  1);
            }
            else
                if (me->got.config)
                    ReturnInt32Datum(devid, chn, me->cache[chn], 0);
        }
        else if (chn == KSHD485_CHAN_STOPCOND_BYTE  ||
                 (chn >= KSHD485_CHAN_STOPCOND_BIT_base  &&
                  chn <= KSHD485_CHAN_STOPCOND_BIT_base + 7))
        {
            if (action == DRVA_WRITE)
            {
                if (chn == KSHD485_CHAN_STOPCOND_BYTE)
                    SetStopCond(me, val, 0xFF);
                else
                    SetStopCond(me,
                                (val != 0) << (chn - KSHD485_CHAN_STOPCOND_BIT_base),
                                (1)        << (chn - KSHD485_CHAN_STOPCOND_BIT_base));
            }
            else
                ReturnInt32Datum(devid, chn, me->cache[chn], 0);
        }
        else if (chn == KSHD485_CHAN_NUM_STEPS)
        {
            if (action == DRVA_WRITE)
                me->cache[chn] = val;
            ReturnInt32Datum(devid, chn, me->cache[chn], 0);
        }
        else if (chn == KSHD485_CHAN_GO  ||
                 chn == KSHD485_CHAN_GO_WO_ACCEL)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                SendGo(me,
                       chn == KSHD485_CHAN_GO? KCMD_GO : KCMD_GO_WO_ACCEL,
                       me->cache[KSHD485_CHAN_NUM_STEPS],
                       me->cache[KSHD485_CHAN_STOPCOND_BYTE]);
            ReturnInt32Datum(devid, chn, me->cache[chn], 0);
        }
        else if (chn == KSHD485_CHAN_STOP  ||  chn == KSHD485_CHAN_SWITCHOFF)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1, 
                                  chn == KSHD485_CHAN_STOP? KCMD_STOP : KCMD_SWITCHOFF);
            ReturnInt32Datum(devid, chn, me->cache[chn], 0);
        }
        else if (chn == KSHD485_CHAN_GO_N_STEPS  ||  chn == KSHD485_CHAN_GO_WOA_N_STEPS)
        {
            if (action == DRVA_WRITE  &&  val != 0)
            {
                nsteps = val & 0x00FFFFFF;
                if (nsteps & 0x00800000) nsteps |= 0xFF000000; // Sign-extend 24->32 bits
                stopcond = (val >> 24) & 0xFF;
                if (stopcond == 0) stopcond = me->cache[KSHD485_CHAN_STOPCOND_BYTE];
                SendGo(me,
                       chn == KSHD485_CHAN_GO_N_STEPS? KCMD_GO : KCMD_GO_WO_ACCEL,
                       nsteps, stopcond);
            }
            ReturnInt32Datum(devid, chn, me->cache[chn], 0);
        }
        else if (chn == KSHD485_CHAN_DO_RESET)
        {
            ReturnInt32Datum(devid, chn, me->cache[chn], 0);
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
            {
                me->lvmt->q_clear(me->handle);
                SetDevState(devid, DEVSTATE_NOTREADY,  0, NULL);
                SetDevState(devid, DEVSTATE_OPERATING, 0, NULL);
            }
        }
        else if (chn == KSHD485_CHAN_SAVE_CONFIG)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1, 
                                  KCMD_SAVE_CONFIG);
            ReturnInt32Datum(devid, chn, me->cache[chn], 0);
        }
        else
        {
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);
        }

 NEXT_CHANNEL:;
    }
}

static void kshd485_in(int devid, void *devptr,
                       int cmd, int dlc, uint8 *data)
{
  privrec_t      *me = (privrec_t *)devptr;

  int             prev_ready;

  int             x;

    if      (cmd == KCMD_GET_INFO)
    {
        me->got.info = 1;
        me->cache[KSHD485_CHAN_DEV_VERSION] = data[2] * 10  + data[3];
        me->cache[KSHD485_CHAN_DEV_SERIAL]  = data[4] * 256 + data[5];
        ReturnInt32Datum(devid,    KSHD485_CHAN_DEV_VERSION,
                         me->cache[KSHD485_CHAN_DEV_VERSION], 0);
        ReturnInt32Datum(devid,    KSHD485_CHAN_DEV_SERIAL,
                         me->cache[KSHD485_CHAN_DEV_SERIAL],  0);
    }
    else if (cmd >= KCMD_GET_STATUS  &&  cmd <= KCMD_PULSE)
    {
        prev_ready = IsReady(me);
        me->cache[KSHD485_CHAN_STATUS_BYTE] = data[0];
        ReturnInt32Datum(devid,    KSHD485_CHAN_STATUS_BYTE,
                         me->cache[KSHD485_CHAN_STATUS_BYTE], 0);
        for (x = 0;  x < 8;  x++)
        {
            me->cache[KSHD485_CHAN_STATUS_base + x] = (data[0] >> x) & 1;
            ReturnInt32Datum(devid,    KSHD485_CHAN_STATUS_base + x,
                             me->cache[KSHD485_CHAN_STATUS_base + x], 0);
        }

        if (prev_ready == 0  &&  IsReady(me))
        {
            if (DevVersion(me) >= 20)
                me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                                  KCMD_GET_REMSTEPS);
            if (DevVersion(me) >= 21)
                me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                                  KCMD_GET_ABSCOORD);
        }
    }
    else if (cmd == KCMD_GET_REMSTEPS)
    {
        me->cache[KSHD485_CHAN_STEPS_LEFT] =
            (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
        ReturnInt32Datum(devid,    KSHD485_CHAN_STEPS_LEFT,
                         me->cache[KSHD485_CHAN_STEPS_LEFT], 0);
    }
    else if (cmd == KCMD_GET_ABSCOORD)
    {
        me->cache[KSHD485_CHAN_ABSCOORD] =
            (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
        ReturnInt32Datum(devid,    KSHD485_CHAN_ABSCOORD,
                         me->cache[KSHD485_CHAN_ABSCOORD], 0);
    }
    else if (cmd == KCMD_GET_CONFIG)
    {
        me->got.config = 1;
        for (x = 0;  x < 3;  x++)
        {
            me->cache                 [KSHD485_CONFIG_CHAN_base + x] = data[x];
            ReturnInt32Datum(devid,    KSHD485_CONFIG_CHAN_base + x,
                             me->cache[KSHD485_CONFIG_CHAN_base + x], 0);
        }
        SetConfigByte(me, data[3], 0xFF, 0);
    }
    else if (cmd == KCMD_GET_SPEEDS)
    {
        me->got.speeds = 1;
        for (x = 0;  x < 3;  x++)
        {
            me->cache[KSHD485_SPEEDS_CHAN_base + x] =
                (data[x*2] << 8) + data[x*2 + 1];
            ReturnInt32Datum(devid,    KSHD485_SPEEDS_CHAN_base + x,
                             me->cache[KSHD485_SPEEDS_CHAN_base + x], 0);
        }
    }
    else
    {
        /* How can this happen? 'cmd' is the code of last-sent packet */
        DoDriverLog(devid, DRIVERLOG_ALERT,
                    "%s: !!! unknown packet type %d, length=%d",
                    __FUNCTION__, cmd, dlc);
    }
}

static void kshd485_md(int devid, void *devptr,
                       int *dlc_p, uint8 *data)
{
  privrec_t      *me = (privrec_t *)devptr;

    if (!IsReady(me)  &&
        (
         *data == KCMD_GO          ||  *data == KCMD_GO_WO_ACCEL  ||
         *data == KCMD_SET_CONFIG  ||  *data == KCMD_SET_SPEEDS   ||
         *data == KCMD_SWITCHOFF   ||  *data == KCMD_SAVE_CONFIG  ||
         *data == KCMD_GO_PREC
         ||
         *data == KCMD_GET_REMSTEPS  ||
         *data == KCMD_SET_ABSCOORD  ||  *data == KCMD_GET_ABSCOORD
        ))
    {
        *dlc_p = 1; data[0] = KCMD_GET_STATUS;
    }
}


DEFINE_CXSD_DRIVER(kshd485, "KShD485 driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   2, 2,
                   PIV485_LYR_NAME, PIV485_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   kshd485_init_d, NULL, kshd485_rw_p);
