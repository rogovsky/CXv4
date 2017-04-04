#include <stdio.h>

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/vepp4_gimn_drv_i.h"


enum
{
    PKS_VAL,
    PKS_RATE,
    PKS_CUR,

    GVI_B0,
    GVI_B1,
    GVI_8B,

    VSDC_I,

    SUBORD_NUMCHANS
};

enum
{
    SUBDEV_PKS,
    SUBDEV_GVI,
    SUBDEV_VSDC,
};

static vdev_sodc_dsc_t hw2our_mapping[SUBORD_NUMCHANS] =
{
    [PKS_VAL]  = {"pks.out0",   VDEV_IMPR | VDEV_TUBE, VEPP4_GIMN_CHAN_PKS_VAL,    SUBDEV_PKS},
    [PKS_RATE] = {"pks.out_rate0",  VDEV_PRIV,             -1,                         SUBDEV_PKS},
    [PKS_CUR]  = {"pks.out_cur0",   VDEV_PRIV,             -1,                         SUBDEV_PKS},
    [GVI_B0]   = {"gvi.outrb0", VDEV_IMPR | VDEV_TUBE, VEPP4_GIMN_CHAN_GVI_OUTRB0, SUBDEV_GVI},
    [GVI_B1]   = {"gvi.outrb1", VDEV_IMPR | VDEV_TUBE, VEPP4_GIMN_CHAN_GVI_OUTRB1, SUBDEV_GVI},
    [GVI_8B]   = {"gvi.outr8b", VDEV_IMPR,             -1,                         SUBDEV_GVI},
    [VSDC_I]   = {"vsdc.int0",  VDEV_PRIV,             -1,                         SUBDEV_VSDC},
};

static const char *devstate_names[] =
{
    [SUBDEV_PKS]  = "pks._devstate",
    [SUBDEV_GVI]  = "gvi._devstate",
    [SUBDEV_VSDC] = "vsdc._devstate",
};

//
enum
{
    WORK_HEARTBEAT_PERIOD =    100000, // 100ms/10Hz
};

static int32 sign_of(int32 v)
{
    if      (v < 0) return -1;
    else if (v > 0) return +1;
    else            return  0;
}


//--------------------------------------------------------------------

typedef struct
{
    int                 devid;

    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];

    int32               out_val;
    int                 out_val_set;

    int32               cur_sign;
    int32               rqd_sign;
} privrec_t;

static inline void SndCVal(privrec_t *me, int sodc, int32 val)
{
    cda_snd_ref_data(me->cur[sodc].ref, CXDTYPE_INT32, 1, &val);
}

//--------------------------------------------------------------------

enum
{
    GIMN_STATE_UNKNOWN = 0,
    GIMN_STATE_DETERMINE,    // 1

    GIMN_STATE_IS_OFF,       // 2

    GIMN_STATE_SW_ON_ON,     // 3
    GIMN_STATE_SW_ON_SET,    // 4

    GIMN_STATE_IS_ON,        // 5

    GIMN_STATE_SW_OFF_ZERO,  // 6
    GIMN_STATE_SW_OFF_OFF,   // 7

    GIMN_STATE_RV_ZERO,      // 8
    GIMN_STATE_RV_WAIT_1,
    GIMN_STATE_RV_WAIT_2,
    GIMN_STATE_RV_WAIT_3,
    GIMN_STATE_RV_OFF,
};

//--------------------------------------------------------------------

static void SwchToUNKNOWN(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    vdev_forget_all(&me->ctx);
}

static void SwchToDETERMINE(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;
  int        targ_state;

    switch (me->cur[GVI_8B].v.i32 & 3)
    {
        case 0: targ_state = GIMN_STATE_IS_OFF; me->cur_sign = +1; break;
        case 1: targ_state = GIMN_STATE_IS_ON;  me->cur_sign = +1; break;
        case 2: targ_state = GIMN_STATE_IS_ON;  me->cur_sign = -1; break;
        case 3: targ_state = GIMN_STATE_IS_OFF; me->cur_sign = +1;
                SndCVal(me, GVI_8B, (me->cur[GVI_8B].v.i32) &~ 3);
    }

    if (!me->out_val_set)
    {
        me->out_val     = me->cur[PKS_VAL].v.i32 * me->cur_sign;
        me->out_val_set = 1; /* Yes, we do it only ONCE, since later OUT_CUR may change upon our request, without any client's write */

        ReturnInt32Datum(me->devid, VEPP4_GIMN_CHAN_CODE_SET, me->out_val, 0);
    }

    vdev_set_state(&(me->ctx), targ_state);
}

static int  IsAlwdSW_ON_ON(void *devptr, int prev_state)
{
    return prev_state == GIMN_STATE_IS_OFF;
}

static void SwchToSW_ON_ON(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;
  int32      bits;

    if (sign_of(me->out_val) != 0 &&
        sign_of(me->out_val) != me->cur_sign)
    {
        /* Call polarity-switch sequence, than leading back to SW_ON_ON */
        vdev_set_state(&(me->ctx), GIMN_STATE_RV_ZERO);
    }
    else
        SndCVal(me, PKS_VAL, abs(me->out_val));

    bits = (me->cur_sign >= 0) ? 1 : 2;
    SndCVal(me, GVI_8B, ((me->cur[GVI_8B].v.i32) &~ 3) | bits);
}

static void SwchToSW_ON_SET(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, PKS_VAL, abs(me->out_val));
}

static int  IsAlwdSW_OFF_ZERO(void *devptr, int prev_state)
{
  privrec_t *me = devptr;

    return
        /* Regular "ON" state... */
        prev_state == GIMN_STATE_IS_ON      ||
        /* ...and "switching-to-on" stages */
        prev_state == GIMN_STATE_SW_ON_ON   ||
        prev_state == GIMN_STATE_SW_ON_SET  ||
        /* ...or the state bit is just "on" */
        (me->cur[GVI_8B].v.i32 & 3) != 0
        ;
}

static void SwchToSW_OFF_ZERO(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, PKS_VAL, 0);
}

static void SwchToSW_OFF_OFF(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, GVI_8B, (me->cur[GVI_8B].v.i32) &~ 3);
}

static void SwchToRV_ZERO(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, PKS_VAL, 0);
}

static void SwchToRV_OFF(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    me->cur_sign = sign_of(me->out_val);
    SndCVal(me, GVI_8B, (me->cur[GVI_8B].v.i32) &~ 3);
}

static vdev_state_dsc_t state_descr[] =
{
    [GIMN_STATE_UNKNOWN]      = {0,       -1,                     NULL,               SwchToUNKNOWN,      NULL},
    [GIMN_STATE_DETERMINE]    = {0,       -1,                     NULL,               SwchToDETERMINE,    NULL},
    [GIMN_STATE_IS_OFF]       = {0,       -1,                     NULL,               NULL,               NULL},
    [GIMN_STATE_SW_ON_ON]     = {1000000, GIMN_STATE_SW_ON_SET,   NULL,               SwchToSW_ON_ON,     NULL},
    [GIMN_STATE_SW_ON_SET]    = { 500000, GIMN_STATE_IS_ON,       NULL,               SwchToSW_ON_SET,    NULL},
    [GIMN_STATE_IS_ON]        = {0,       -1,                     NULL,               NULL,               NULL},
    [GIMN_STATE_SW_OFF_ZERO]  = { 500000, GIMN_STATE_SW_OFF_OFF,  NULL,               SwchToSW_OFF_ZERO,  NULL},
    [GIMN_STATE_SW_OFF_OFF]   = {1000000, GIMN_STATE_IS_OFF,      NULL,               SwchToSW_OFF_OFF,   NULL},
    [GIMN_STATE_RV_ZERO]      = { 100000, GIMN_STATE_RV_WAIT_1,   NULL,               SwchToRV_ZERO,      NULL},
    [GIMN_STATE_RV_WAIT_1]    = {0,       GIMN_STATE_RV_WAIT_2,   NULL,               NULL,               NULL},
    [GIMN_STATE_RV_WAIT_2]    = {0,       GIMN_STATE_RV_WAIT_3,   NULL,               NULL,               NULL},
    [GIMN_STATE_RV_WAIT_3]    = {0,       GIMN_STATE_RV_OFF,      NULL,               NULL,               NULL},
    [GIMN_STATE_RV_OFF]       = { 100000, GIMN_STATE_SW_ON_ON,    NULL,               SwchToRV_OFF,       NULL},
};

//--------------------------------------------------------------------

static void vepp4_gimn_sodc_cb(int devid, void *devptr,
                               int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;

    /* If not all info is gathered yet, then there's nothing else to do yet */
    if (me->ctx.cur_state == GIMN_STATE_UNKNOWN) return;

    if (sodc == VSDC_I  &&
        (
         me->ctx.cur_state  == GIMN_STATE_RV_WAIT_1  ||
         me->ctx.cur_state  == GIMN_STATE_RV_WAIT_2  ||
         me->ctx.cur_state  == GIMN_STATE_RV_WAIT_3
        )
       )
       vdev_set_state(&(me->ctx), me->ctx.cur_state + 1);
    
}

static int vepp4_gimn_init_d(int devid, void *devptr,
                             int businfocount, int businfo[],
                             const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;

    me->devid = devid;

    me->cur_sign = +1;

    SetChanReturnType(devid, VEPP4_GIMN_CHAN_PKS_VAL,    1, IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, VEPP4_GIMN_CHAN_GVI_OUTRB0, 1, IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, VEPP4_GIMN_CHAN_GVI_OUTRB1, 1, IS_AUTOUPDATED_YES);

    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = hw2our_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = NULL;
    me->ctx.sodc_cb        = vepp4_gimn_sodc_cb;

    me->ctx.our_numchans             = VEPP4_GIMN_NUMCHANS;
    me->ctx.chan_state_n             = VEPP4_GIMN_CHAN_GIMN_STATE;
    me->ctx.state_unknown_val        = GIMN_STATE_UNKNOWN;
    me->ctx.state_determine_val      = GIMN_STATE_DETERMINE;
    me->ctx.state_count              = countof(state_descr);
    me->ctx.state_descr              = state_descr;
    me->ctx.state_related_channels   = NULL; //state_related_channels;
    me->ctx.state_important_channels = NULL; //state_important_channels;

    return vdev_init(&(me->ctx), devid, devptr, WORK_HEARTBEAT_PERIOD, auxinfo);
}

static void vepp4_gimn_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void vepp4_gimn_rw_p(int devid, void *devptr,
                            int action,
                            int count, int *addrs,
                            cxdtype_t *dtypes, int *nelems, void **values) 
{
  privrec_t *me = devptr;
  int             n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int             chn;   // channel
  int32           val;

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

        if      (chn == VEPP4_GIMN_CHAN_CODE_SET)
        {
            if (action == DRVA_WRITE)
            {
                if (val < -65535) val = -65535;
                if (val > +65535) val = +65535;

                me->out_val     = val;
                me->out_val_set = 1;

                if (me->ctx.cur_state == GIMN_STATE_IS_ON)
                {
                    if (sign_of(me->out_val) != 0  &&
                        sign_of(me->out_val) != me->cur_sign)
                    {
                        /* Call polarity-switch sequence, than leading back to SW_ON_ON */
                        vdev_set_state(&(me->ctx), GIMN_STATE_RV_ZERO);
                    }
                    else
                        SndCVal(me, PKS_VAL, abs(me->out_val));
                }
            }
            if (me->out_val_set)
                ReturnInt32Datum(devid, chn, me->out_val, 0);
        }
        else if (chn == VEPP4_GIMN_CHAN_SWITCH)
        {
            if (action == DRVA_WRITE)
            {
                if (val != 0)
                {
                    if (IsAlwdSW_ON_ON   (me, me->ctx.cur_state))
                        vdev_set_state(&(me->ctx), GIMN_STATE_SW_ON_ON);
                }
                else
                {
                    if (IsAlwdSW_OFF_ZERO(me, me->ctx.cur_state))
                        vdev_set_state(&(me->ctx), GIMN_STATE_SW_OFF_ZERO);
                }
            }
            ReturnInt32Datum(devid, chn,
                             (me->ctx.cur_state >= GIMN_STATE_SW_ON_ON  &&
                              me->ctx.cur_state <= GIMN_STATE_IS_ON)
                             ||
                             (me->ctx.cur_state >= GIMN_STATE_RV_ZERO  &&
                              me->ctx.cur_state <= GIMN_STATE_RV_OFF),
                             0);
        }
        else if (chn == VEPP4_GIMN_CHAN_RESET_STATE)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                vdev_set_state(&(me->ctx), GIMN_STATE_DETERMINE);
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(vepp4_gimn, "VEPP4 gimn driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   vepp4_gimn_init_d,
                   vepp4_gimn_term_d,
                   vepp4_gimn_rw_p);
