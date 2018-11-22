#include <stdio.h>

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/mps20_ceac124_drv_i.h"


enum
{
    C124C_OUT,
    C124C_OUT_RATE,
    C124C_DISABLE,
    C124C_SW_ON,
    C124C_SW_OFF,

    C124C_IMES,
    C124C_UMES,

    C124C_OUT_CUR,

    C124C_ILK,
    C124C_OPR_S1,
    C124C_OPR_S2,

    SUBORD_NUMCHANS
};

static vdev_sodc_dsc_t cdac2mps_mapping[SUBORD_NUMCHANS] =
{
    [C124C_OUT]      = {"out0",           VDEV_IMPR,             -1},
    [C124C_OUT_RATE] = {"out_rate0",                  VDEV_TUBE, MPS20_CEAC124_CHAN_ISET_RATE},
    [C124C_DISABLE]  = {"outrb0",                     VDEV_TUBE, MPS20_CEAC124_CHAN_DISABLE},
    [C124C_SW_ON]    = {"outrb1",         VDEV_PRIV,             -1},
    [C124C_SW_OFF]   = {"outrb2",         VDEV_PRIV,             -1},

    [C124C_IMES]     = {"adc0",                       VDEV_TUBE, MPS20_CEAC124_CHAN_IMES},
    [C124C_UMES]     = {"adc1",                       VDEV_TUBE, MPS20_CEAC124_CHAN_UMES},

    [C124C_OUT_CUR]  = {"out_cur0",       VDEV_IMPR | VDEV_TUBE, MPS20_CEAC124_CHAN_ISET_CUR},

    [C124C_ILK]      = {"inprb0",         VDEV_IMPR | VDEV_TUBE, MPS20_CEAC124_CHAN_ILK},
    [C124C_OPR_S1]   = {"inprb1",         VDEV_IMPR,             -1},
    [C124C_OPR_S2]   = {"inprb2",         VDEV_IMPR,             -1},
};                               

static int ourc2sodc[MPS20_CEAC124_NUMCHANS];

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

typedef struct
{
    int                 devid;

    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];

    int                 is_singlebit;

    int32               out_val;
    int                 out_val_set;

    int32               c_ilk;
} privrec_t;

static inline void SndCVal(privrec_t *me, int sodc, int32 val)
{
    cda_snd_ref_data(me->cur[sodc].ref, CXDTYPE_INT32, 1, &val);
}

//--------------------------------------------------------------------

enum
{
    MPS_STATE_UNKNOWN   = 0,
    MPS_STATE_DETERMINE,         // 1
    MPS_STATE_INTERLOCK,         // 2

    MPS_STATE_CUT_OFF,           // 3

    MPS_STATE_IS_OFF,            // 4

    MPS_STATE_SW_ON_ENABLE,      // 5
    MPS_STATE_SW_ON_SET_ON,      // 6
    MPS_STATE_SW_ON_DRP_ON,      // 7
    MPS_STATE_SW_ON_UP,          // 8

    MPS_STATE_IS_ON,             // 9

    MPS_STATE_SW_OFF_DOWN,       // 10
    MPS_STATE_SW_OFF_CHK_I,      // 11
    MPS_STATE_SW_OFF_CHK_E,      // 12

    MPS_STATE_count
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

    ReturnInt32Datum(me->devid, MPS20_CEAC124_CHAN_C_ILK, me->c_ilk, 0);

    if (!me->out_val_set)
    {
        me->out_val     = me->cur[C124C_OUT_CUR].v.i32;
        me->out_val_set = 1; /* Yes, we do it only ONCE, since later OUT_CUR may change upon our request, without any client's write */

        ReturnInt32Datum(me->devid, MPS20_CEAC124_CHAN_ISET, me->out_val, 0);
    }

    ReturnInt32Datum(me->devid,
                     MPS20_CEAC124_CHAN_ISET_CUR, me->cur[C124C_OUT_CUR].v.i32, 0);

    if ( me->cur[C124C_OPR_S1].v.i32 == 0  &&  
        (me->is_singlebit  ||
         me->cur[C124C_OPR_S2].v.i32 == 0))
        vdev_set_state(&(me->ctx), MPS_STATE_IS_ON);
    else
        vdev_set_state(&(me->ctx), MPS_STATE_IS_OFF);
}

static void SwchToINTERLOCK(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, C124C_OUT,       0);
    SndCVal(me, C124C_SW_ON,     0);
    SndCVal(me, C124C_SW_OFF,    1);
}

static void SwchToCUT_OFF(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, C124C_OUT,       0);
    SndCVal(me, C124C_SW_ON,     0);
    SndCVal(me, C124C_SW_OFF,    1);
}

static int  IsAlwdSW_ON_ENABLE(void *devptr, int prev_state)
{
  privrec_t *me = devptr;

    return
        (prev_state == MPS_STATE_IS_OFF   ||
         prev_state == MPS_STATE_CUT_OFF  ||
         prev_state == MPS_STATE_INTERLOCK)  &&
        me->cur[C124C_OUT_CUR].v.i32 == 0    &&
        me->cur[C124C_ILK    ].v.i32 == 0;
}

static void SwchToON_ENABLE(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, C124C_SW_OFF, 0);
}

static void SwchToON_SET_ON(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, C124C_SW_ON,  1);
}

static void SwchToON_DRP_ON(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, C124C_SW_ON,  0);
}

static int  IsAlwdSW_ON_UP(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return
        ( me->cur[C124C_OPR_S1].v.i32 == 0  &&  
         (me->is_singlebit  ||
          me->cur[C124C_OPR_S2].v.i32 == 0));
}

static void SwchToON_UP    (void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, C124C_OUT,       me->out_val);
}

static int  IsAlwdIS_ON    (void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return (abs(me->cur[C124C_OUT_CUR].v.i32) - me->out_val) <= 305;
}

static int  IsAlwdSW_OFF_DOWN(void *devptr, int prev_state)
{
  privrec_t *me = devptr;

    return
        /* Regular "ON" state... */
        prev_state == MPS_STATE_IS_ON         ||
        /* ...and "switching-to-on" stages */
        prev_state == MPS_STATE_SW_ON_ENABLE  ||
        prev_state == MPS_STATE_SW_ON_SET_ON  ||
        prev_state == MPS_STATE_SW_ON_DRP_ON  ||
        prev_state == MPS_STATE_SW_ON_UP      ||
        /* ...or the state bits are just "on" (i.e. ==0 (belikov's are inverted)) */
        ( me->cur[C124C_OPR_S1].v.i32 == 0  &&
         (me->is_singlebit  ||
          me->cur[C124C_OPR_S2].v.i32 == 0))  ||
        /* ...or DAC current value is !=0 */
        me->cur[C124C_OUT_CUR].v.i32 != 0
        ;
}

static void SwchToSW_OFF_DOWN(void *devptr, int prev_state)
{
  privrec_t *me = devptr;

    SndCVal(me, C124C_OUT,    0);
    if (prev_state != MPS_STATE_IS_ON) SndCVal(me, C124C_SW_ON, 0);
}

static int  IsAlwdSW_OFF_CHK_I(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    ////fprintf(stderr, "\tout_cur=%d\n", me->cur[C124C_OUT_CUR].v.i32);
    return me->cur[C124C_OUT_CUR].v.i32 == 0;
}

static void SwchToSW_OFF_CHK_I(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, C124C_SW_ON,  0);
    SndCVal(me, C124C_SW_OFF, 1);
}

static int  IsAlwdSW_OFF_CHK_E(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    ////fprintf(stderr, "\topr=%d\n", me->cur[C124C_OPR].v.i32);
    return
        ( me->cur[C124C_OPR_S1].v.i32 != 0  &&
         (me->is_singlebit  ||
          me->cur[C124C_OPR_S2].v.i32 != 0));
}

static vdev_state_dsc_t state_descr[] =
{
    [MPS_STATE_UNKNOWN]      = {0,       -1,                     NULL,               SwchToUNKNOWN,      NULL},
    [MPS_STATE_DETERMINE]    = {0,       -1,                     NULL,               SwchToDETERMINE,    NULL},
    [MPS_STATE_INTERLOCK]    = {0,       -1,                     NULL,               SwchToINTERLOCK,    NULL},

    [MPS_STATE_CUT_OFF]      = {0,       -1,                     NULL,               SwchToCUT_OFF,      NULL},

    [MPS_STATE_IS_OFF]       = {0,       -1,                     NULL,               NULL,               NULL},

    [MPS_STATE_SW_ON_ENABLE] = { 500000, MPS_STATE_SW_ON_SET_ON, IsAlwdSW_ON_ENABLE, SwchToON_ENABLE,    NULL},
    [MPS_STATE_SW_ON_SET_ON] = {1000000, MPS_STATE_SW_ON_DRP_ON, NULL,               SwchToON_SET_ON,    NULL},
    [MPS_STATE_SW_ON_DRP_ON] = { 500000, MPS_STATE_SW_ON_UP,     NULL,               SwchToON_DRP_ON,    NULL},
    [MPS_STATE_SW_ON_UP]     = {1,       MPS_STATE_IS_ON,        IsAlwdSW_ON_UP,     SwchToON_UP,        NULL},

    [MPS_STATE_IS_ON]        = {0,       -1,                     IsAlwdIS_ON,        NULL,               (int[]){C124C_OUT_CUR, -1}},

    [MPS_STATE_SW_OFF_DOWN]  = {0,       MPS_STATE_SW_OFF_CHK_I, IsAlwdSW_OFF_DOWN,  SwchToSW_OFF_DOWN,  NULL},
    [MPS_STATE_SW_OFF_CHK_I] = {0,       MPS_STATE_SW_OFF_CHK_E, IsAlwdSW_OFF_CHK_I, SwchToSW_OFF_CHK_I, (int[]){C124C_OUT_CUR, -1}},
    [MPS_STATE_SW_OFF_CHK_E] = {1,       MPS_STATE_IS_OFF,       IsAlwdSW_OFF_CHK_E, NULL,               (int[]){C124C_OPR_S1, C124C_OPR_S2, -1}},
};

static int state_important_channels[countof(state_descr)][SUBORD_NUMCHANS];

//////////////////////////////////////////////////////////////////////

static int mps20_ceac124_init_mod(void)
{
  int  sn;
  int  x;

    /* Fill interesting_chan[][] mapping */
    bzero(state_important_channels, sizeof(state_important_channels));
    for (sn = 0;  sn < countof(state_descr);  sn++)
        if (state_descr[sn].impr_chans != NULL)
            for (x = 0;  state_descr[sn].impr_chans[x] >= 0;  x++)
                state_important_channels[sn][state_descr[sn].impr_chans[x]] = 1;

    /* ...and ourc->sodc mapping too */
    for (x = 0;  x < countof(ourc2sodc);  x++)
        ourc2sodc[x] = -1;
    for (x = 0;  x < countof(cdac2mps_mapping);  x++)
        if (cdac2mps_mapping[x].mode != 0  &&  cdac2mps_mapping[x].ourc >= 0)
            ourc2sodc[cdac2mps_mapping[x].ourc] = x;

    return 0;
}

//////////////////////////////////////////////////////////////////////

static void mps20_ceac124_rw_p(int devid, void *devptr,
                               int action,
                               int count, int *addrs,
                               cxdtype_t *dtypes, int *nelems, void **values);
static void mps20_ceac124_sodc_cb(int devid, void *devptr,
                                  int sodc, int32 val);

static vdev_sr_chan_dsc_t state_related_channels[] =
{
    {MPS20_CEAC124_CHAN_SWITCH_ON,  MPS_STATE_SW_ON_ENABLE, 0, CX_VALUE_DISABLED_MASK},
    {MPS20_CEAC124_CHAN_SWITCH_OFF, MPS_STATE_SW_OFF_DOWN,  0, CX_VALUE_DISABLED_MASK},
    {MPS20_CEAC124_CHAN_IS_ON,      -1,                     0, 0},
    {MPS20_CEAC124_CHAN_VDEV_CONDITION, -1,                 0, 0},
    {-1,                            -1,                     0, 0},
};

static int mps20_ceac124_init_d(int devid, void *devptr, 
                                int businfocount, int businfo[],
                                const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;
  const char     *p  = auxinfo;

    me->devid = devid;
    if (p == NULL  ||  *p == '\0')
    {
        DoDriverLog(devid, 0,
                    "%s(): base-CEAC124-device name is required in auxinfo",
                    __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }
    if (*p == '/')
    {
        me->is_singlebit = 1;
        p++;
        if (*p == '\0')
        {
            DoDriverLog(devid, 0,
                        "%s(): base-CEAC124-device name is required in auxinfo after '/'",
                        __FUNCTION__);
            return -CXRF_CFG_PROBL;
        }
    }
    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = cdac2mps_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = mps20_ceac124_rw_p;
    me->ctx.sodc_cb        = mps20_ceac124_sodc_cb;

    me->ctx.our_numchans             = MPS20_CEAC124_NUMCHANS;
    me->ctx.chan_state_n             = MPS20_CEAC124_CHAN_VDEV_STATE;
    me->ctx.state_unknown_val        = MPS_STATE_UNKNOWN;
    me->ctx.state_determine_val      = MPS_STATE_DETERMINE;
    me->ctx.state_count              = countof(state_descr);
    me->ctx.state_descr              = state_descr;
    me->ctx.state_related_channels   = state_related_channels;
    me->ctx.state_important_channels = state_important_channels;

    SetChanRDs       (devid, MPS20_CEAC124_CHAN_ISET,      1, 1000000.0, 0.0);
    SetChanRDs       (devid, MPS20_CEAC124_CHAN_ISET_RATE, 1, 1000000.0, 0.0);
    SetChanRDs       (devid, MPS20_CEAC124_CHAN_ISET_CUR,  1, 1000000.0, 0.0);
    SetChanRDs/*!!!*/(devid, MPS20_CEAC124_CHAN_RD_base,  16, 1000000.0, 0.0);
    SetChanReturnType(devid, MPS20_CEAC124_CHAN_ISET_CUR,  1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, MPS20_CEAC124_CHAN_ILK,       1, IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, MPS20_CEAC124_CHAN_C_ILK,     1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, MPS20_CEAC124_CHAN_OPR_S1,    1, IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, MPS20_CEAC124_CHAN_OPR_S2,    1, IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, MPS20_CEAC124_CHAN_VDEV_CONDITION,1,IS_AUTOUPDATED_TRUSTED);
    ReturnInt32Datum (devid, MPS20_CEAC124_CHAN_VDEV_CONDITION,
                             VDEV_PS_CONDITION_OFFLINE, 0);

    return vdev_init(&(me->ctx), devid, devptr, WORK_HEARTBEAT_PERIOD, p);
}

static void mps20_ceac124_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void mps20_ceac124_sodc_cb(int devid, void *devptr,
                                  int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;

    /* If not all info is gathered yet, then there's nothing else to do yet */
    if (me->ctx.cur_state == MPS_STATE_UNKNOWN) return;

    /* Perform state transitions depending on received data */
    if (sodc == C124C_ILK)
    {
        /* "Tube" first, ... */
        ReturnInt32Datum(devid, MPS20_CEAC124_CHAN_ILK, val, 0);
        if (val == 1  &&  me->c_ilk != val)
        {
            me->c_ilk = val;
            ReturnInt32Datum(devid, MPS20_CEAC124_CHAN_C_ILK, val, 0);
        }
        /* ...than check */
        if (val != 0)
            vdev_set_state(&(me->ctx), MPS_STATE_INTERLOCK);
    }
    else if (/* 26.09.2012: when OPR bit disappears, we should better switch off */
             (sodc == C124C_OPR_S1  ||  sodc == C124C_OPR_S2))
    {
        val = !val; // Invert
        /* "Tube" (inverted) first, ... */
        ReturnInt32Datum(devid,
                         sodc == C124C_OPR_S1? MPS20_CEAC124_CHAN_OPR_S1
                                             : MPS20_CEAC124_CHAN_OPR_S2,
                         val, 0);
        /* ...than check */
        if ((sodc == C124C_OPR_S1  ||  me->is_singlebit == 0)  &&
            val == 0 /* Note: this is INVERTED value */        &&
            (me->ctx.cur_state == MPS_STATE_SW_ON_DRP_ON  ||
             me->ctx.cur_state == MPS_STATE_SW_ON_UP      ||
             me->ctx.cur_state == MPS_STATE_IS_ON))
        {
            vdev_set_state(&(me->ctx), MPS_STATE_CUT_OFF);
        }
    }
}

static vdev_sr_chan_dsc_t *find_sdp(int ourc)
{
  vdev_sr_chan_dsc_t *sdp;

    for (sdp = state_related_channels;
         sdp != NULL  &&  sdp->ourc >= 0;
         sdp++)
        if (sdp->ourc == ourc) return sdp;

    return NULL;
}

static void mps20_ceac124_rw_p(int devid, void *devptr,
                               int action,
                               int count, int *addrs,
                               cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t          *me = (privrec_t *)devptr;
  int                 n;       // channel N in addrs[]/.../values[] (loop ctl var)
  int                 chn;     // channel
  int32               val;     // Value
  int                 sodc;
  vdev_sr_chan_dsc_t *sdp;

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

        sodc = ourc2sodc[chn];

        if      (sodc >= 0  &&  cdac2mps_mapping[sodc].mode & VDEV_TUBE)
        {
            if (action == DRVA_WRITE)
                SndCVal(me, sodc, val);
            else
                if (me->cur[sodc].rcvd)
                    ReturnInt32DatumTimed(devid, chn, me->cur[sodc].v.i32,
                                          me->cur[sodc].flgs,
                                          me->cur[sodc].ts);
        }
        else if (chn == MPS20_CEAC124_CHAN_ISET)
        {
            if (action == DRVA_WRITE)
            {
                if (val > MAX_ALWD_VAL) val = MAX_ALWD_VAL;
                if (val < MIN_ALWD_VAL) val = MIN_ALWD_VAL;
                me->out_val     = val;
                me->out_val_set = 1;

                if (me->ctx.cur_state == MPS_STATE_IS_ON  ||
                    me->ctx.cur_state == MPS_STATE_SW_ON_UP)
                {
                    SndCVal(me, C124C_OUT, me->out_val);
                }
            }
            if (me->out_val_set)
                ReturnInt32Datum(devid, chn, me->out_val, 0);
        }
        else if ((sdp = find_sdp(chn)) != NULL  &&
                 sdp->state >= 0                &&
                 state_descr[sdp->state].sw_alwd != NULL)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND  &&
                state_descr[sdp->state].sw_alwd(me, me->ctx.cur_state))
                vdev_set_state(&(me->ctx), sdp->state);

            val = state_descr[sdp->state].sw_alwd(me,
                                                  me->ctx.cur_state)? sdp->enabled_val
                                                                    : sdp->disabled_val;
            ReturnInt32Datum(devid, chn, val, 0);
        }
        else if (chn == MPS20_CEAC124_CHAN_RESET_STATE)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                vdev_set_state(&(me->ctx), MPS_STATE_DETERMINE);
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == MPS20_CEAC124_CHAN_RESET_C_ILKS)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
            {
                me->c_ilk = 0;
                ReturnInt32Datum(devid, MPS20_CEAC124_CHAN_C_ILK, me->c_ilk, 0);
            }
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == MPS20_CEAC124_CHAN_IS_ON)
        {
            val = (me->ctx.cur_state >= MPS_STATE_SW_ON_ENABLE  &&
                   me->ctx.cur_state <= MPS_STATE_IS_ON);
            ReturnInt32Datum(devid, chn, val, 0);
        }
        else if (chn == MPS20_CEAC124_CHAN_VDEV_CONDITION)
        {
            if      (me->ctx.cur_state == me->ctx.state_unknown_val  ||
                     me->ctx.cur_state == me->ctx.state_determine_val)
                val = VDEV_PS_CONDITION_OFFLINE;
            else if (me->ctx.cur_state == MPS_STATE_INTERLOCK)
                val = VDEV_PS_CONDITION_INTERLOCK;
            else if (me->ctx.cur_state == MPS_STATE_CUT_OFF)
                val = VDEV_PS_CONDITION_CUT_OFF;
            else if (me->ctx.cur_state == MPS_STATE_IS_OFF)
                val = VDEV_PS_CONDITION_IS_OFF;
            else
                val = VDEV_PS_CONDITION_IS_ON;
            ReturnInt32Datum(devid, chn, val, 0);
        }
        else if (chn == MPS20_CEAC124_CHAN_VDEV_STATE)
        {
            ReturnInt32Datum(devid, chn, me->ctx.cur_state, 0);
        }
        else if (chn == MPS20_CEAC124_CHAN_C_ILK)
        {
            ReturnInt32Datum(devid, chn, me->c_ilk, 0);
        }
        else if (chn == MPS20_CEAC124_CHAN_OPR_S1  ||
                 chn == MPS20_CEAC124_CHAN_OPR_S2)
            /* Returned from sodc_cb() */;
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(mps20_ceac124, "CEAC124-controlled MPS-20",
                   mps20_ceac124_init_mod, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   mps20_ceac124_init_d, mps20_ceac124_term_d, mps20_ceac124_rw_p);
