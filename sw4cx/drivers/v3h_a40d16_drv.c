#include <stdio.h>

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/v3h_a40d16_drv_i.h"


enum
{
    A40_DMS,
    A40_STS,
    A40_MES,
    A40_MU1,
    A40_MU2,
    A40_ILK,
    A40_SW_OFF,

    D16_OUT,
    D16_OUT_RATE,
    D16_OUT_CUR,
    D16_OPR,
    D16_SW_ON,

    SUBORD_NUMCHANS,
};

enum
{
    SUBDEV_A40,
    SUBDEV_D16
};

enum {EIGHTDEVS = 8};

static vdev_sodc_dsc_t cdac2v3h_mappings[EIGHTDEVS][SUBORD_NUMCHANS] =
{
    {
        [D16_OUT]      = {"d16.out0",      VDEV_IMPR,             -1,                        SUBDEV_D16},
        [D16_OUT_RATE] = {"d16.out_rate0",             VDEV_TUBE, V3H_A40D16_CHAN_ISET_RATE, SUBDEV_D16},
        [D16_OUT_CUR]  = {"d16.out_cur0",  VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ISET_CUR,  SUBDEV_D16},
        [D16_OPR]      = {"d16.inprb0",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_OPR,       SUBDEV_D16},
        [D16_SW_ON]    = {"d16.outrb0",    VDEV_PRIV,             -1,                        SUBDEV_D16},
        
        [A40_ILK]      = {"a40.inprb0",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ILK,       SUBDEV_A40},
        [A40_SW_OFF]   = {"a40.outrb0",    VDEV_PRIV,             -1,                        SUBDEV_A40},
        [A40_DMS]      = {"a40.adc0",                  VDEV_TUBE, V3H_A40D16_CHAN_DMS,       SUBDEV_A40},
        [A40_STS]      = {"a40.adc1",                  VDEV_TUBE, V3H_A40D16_CHAN_STS,       SUBDEV_A40},
        [A40_MES]      = {"a40.adc2",                  VDEV_TUBE, V3H_A40D16_CHAN_MES,       SUBDEV_A40},
        [A40_MU1]      = {"a40.adc3",                  VDEV_TUBE, V3H_A40D16_CHAN_MU1,       SUBDEV_A40},
        [A40_MU2]      = {"a40.adc4",                  VDEV_TUBE, V3H_A40D16_CHAN_MU2,       SUBDEV_A40},
    },
    {
        [D16_OUT]      = {"d16.out1",      VDEV_IMPR,             -1,                        SUBDEV_D16},
        [D16_OUT_RATE] = {"d16.out_rate1",             VDEV_TUBE, V3H_A40D16_CHAN_ISET_RATE, SUBDEV_D16},
        [D16_OUT_CUR]  = {"d16.out_cur1",  VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ISET_CUR,  SUBDEV_D16},
        [D16_OPR]      = {"d16.inprb1",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_OPR,       SUBDEV_D16},
        [D16_SW_ON]    = {"d16.outrb1",    VDEV_PRIV,             -1,                        SUBDEV_D16},

        [A40_ILK]      = {"a40.inprb1",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ILK,       SUBDEV_A40},
        [A40_SW_OFF]   = {"a40.outrb1",    VDEV_PRIV,             -1,                        SUBDEV_A40},
        [A40_DMS]      = {"a40.adc5",                  VDEV_TUBE, V3H_A40D16_CHAN_DMS,       SUBDEV_A40},
        [A40_STS]      = {"a40.adc6",                  VDEV_TUBE, V3H_A40D16_CHAN_STS,       SUBDEV_A40},
        [A40_MES]      = {"a40.adc7",                  VDEV_TUBE, V3H_A40D16_CHAN_MES,       SUBDEV_A40},
        [A40_MU1]      = {"a40.adc8",                  VDEV_TUBE, V3H_A40D16_CHAN_MU1,       SUBDEV_A40},
        [A40_MU2]      = {"a40.adc9",                  VDEV_TUBE, V3H_A40D16_CHAN_MU2,       SUBDEV_A40},
    },
    {
        [D16_OUT]      = {"d16.out2",      VDEV_IMPR,             -1,                        SUBDEV_D16},
        [D16_OUT_RATE] = {"d16.out_rate2",             VDEV_TUBE, V3H_A40D16_CHAN_ISET_RATE, SUBDEV_D16},
        [D16_OUT_CUR]  = {"d16.out_cur2",  VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ISET_CUR,  SUBDEV_D16},
        [D16_OPR]      = {"d16.inprb2",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_OPR,       SUBDEV_D16},
        [D16_SW_ON]    = {"d16.outrb2",    VDEV_PRIV,             -1,                        SUBDEV_D16},

        [A40_ILK]      = {"a40.inprb2",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ILK,       SUBDEV_A40},
        [A40_SW_OFF]   = {"a40.outrb2",    VDEV_PRIV,             -1,                        SUBDEV_A40},
        [A40_DMS]      = {"a40.adc10",                 VDEV_TUBE, V3H_A40D16_CHAN_DMS,       SUBDEV_A40},
        [A40_STS]      = {"a40.adc11",                 VDEV_TUBE, V3H_A40D16_CHAN_STS,       SUBDEV_A40},
        [A40_MES]      = {"a40.adc12",                 VDEV_TUBE, V3H_A40D16_CHAN_MES,       SUBDEV_A40},
        [A40_MU1]      = {"a40.adc13",                 VDEV_TUBE, V3H_A40D16_CHAN_MU1,       SUBDEV_A40},
        [A40_MU2]      = {"a40.adc14",                 VDEV_TUBE, V3H_A40D16_CHAN_MU2,       SUBDEV_A40},
    },
    {
        [D16_OUT]      = {"d16.out3",      VDEV_IMPR,             -1,                        SUBDEV_D16},
        [D16_OUT_RATE] = {"d16.out_rate3",             VDEV_TUBE, V3H_A40D16_CHAN_ISET_RATE, SUBDEV_D16},
        [D16_OUT_CUR]  = {"d16.out_cur3",  VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ISET_CUR,  SUBDEV_D16},
        [D16_OPR]      = {"d16.inprb3",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_OPR,       SUBDEV_D16},
        [D16_SW_ON]    = {"d16.outrb3",    VDEV_PRIV,             -1,                        SUBDEV_D16},

        [A40_ILK]      = {"a40.inprb3",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ILK,       SUBDEV_A40},
        [A40_SW_OFF]   = {"a40.outrb3",    VDEV_PRIV,             -1,                        SUBDEV_A40},
        [A40_DMS]      = {"a40.adc15",                 VDEV_TUBE, V3H_A40D16_CHAN_DMS,       SUBDEV_A40},
        [A40_STS]      = {"a40.adc16",                 VDEV_TUBE, V3H_A40D16_CHAN_STS,       SUBDEV_A40},
        [A40_MES]      = {"a40.adc17",                 VDEV_TUBE, V3H_A40D16_CHAN_MES,       SUBDEV_A40},
        [A40_MU1]      = {"a40.adc18",                 VDEV_TUBE, V3H_A40D16_CHAN_MU1,       SUBDEV_A40},
        [A40_MU2]      = {"a40.adc19",                 VDEV_TUBE, V3H_A40D16_CHAN_MU2,       SUBDEV_A40},
    },
    {
        [D16_OUT]      = {"d16.out4",      VDEV_IMPR,             -1,                        SUBDEV_D16},
        [D16_OUT_RATE] = {"d16.out_rate4",             VDEV_TUBE, V3H_A40D16_CHAN_ISET_RATE, SUBDEV_D16},
        [D16_OUT_CUR]  = {"d16.out_cur4",  VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ISET_CUR,  SUBDEV_D16},
        [D16_OPR]      = {"d16.inprb4",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_OPR,       SUBDEV_D16},
        [D16_SW_ON]    = {"d16.outrb4",    VDEV_PRIV,             -1,                        SUBDEV_D16},

        [A40_ILK]      = {"a40.inprb4",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ILK,       SUBDEV_A40},
        [A40_SW_OFF]   = {"a40.outrb4",    VDEV_PRIV,             -1,                        SUBDEV_A40},
        [A40_DMS]      = {"a40.adc20",                 VDEV_TUBE, V3H_A40D16_CHAN_DMS,       SUBDEV_A40},
        [A40_STS]      = {"a40.adc21",                 VDEV_TUBE, V3H_A40D16_CHAN_STS,       SUBDEV_A40},
        [A40_MES]      = {"a40.adc22",                 VDEV_TUBE, V3H_A40D16_CHAN_MES,       SUBDEV_A40},
        [A40_MU1]      = {"a40.adc23",                 VDEV_TUBE, V3H_A40D16_CHAN_MU1,       SUBDEV_A40},
        [A40_MU2]      = {"a40.adc24",                 VDEV_TUBE, V3H_A40D16_CHAN_MU2,       SUBDEV_A40},
    },
    {
        [D16_OUT]      = {"d16.out5",      VDEV_IMPR,             -1,                        SUBDEV_D16},
        [D16_OUT_RATE] = {"d16.out_rate5",             VDEV_TUBE, V3H_A40D16_CHAN_ISET_RATE, SUBDEV_D16},
        [D16_OUT_CUR]  = {"d16.out_cur5",  VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ISET_CUR,  SUBDEV_D16},
        [D16_OPR]      = {"d16.inprb5",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_OPR,       SUBDEV_D16},
        [D16_SW_ON]    = {"d16.outrb5",    VDEV_PRIV,             -1,                        SUBDEV_D16},

        [A40_ILK]      = {"a40.inprb5",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ILK,       SUBDEV_A40},
        [A40_SW_OFF]   = {"a40.outrb5",    VDEV_PRIV,             -1,                        SUBDEV_A40},
        [A40_DMS]      = {"a40.adc25",                 VDEV_TUBE, V3H_A40D16_CHAN_DMS,       SUBDEV_A40},
        [A40_STS]      = {"a40.adc26",                 VDEV_TUBE, V3H_A40D16_CHAN_STS,       SUBDEV_A40},
        [A40_MES]      = {"a40.adc27",                 VDEV_TUBE, V3H_A40D16_CHAN_MES,       SUBDEV_A40},
        [A40_MU1]      = {"a40.adc28",                 VDEV_TUBE, V3H_A40D16_CHAN_MU1,       SUBDEV_A40},
        [A40_MU2]      = {"a40.adc29",                 VDEV_TUBE, V3H_A40D16_CHAN_MU2,       SUBDEV_A40},
    },
    {
        [D16_OUT]      = {"d16.out6",      VDEV_IMPR,             -1,                        SUBDEV_D16},
        [D16_OUT_RATE] = {"d16.out_rate6",             VDEV_TUBE, V3H_A40D16_CHAN_ISET_RATE, SUBDEV_D16},
        [D16_OUT_CUR]  = {"d16.out_cur6",  VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ISET_CUR,  SUBDEV_D16},
        [D16_OPR]      = {"d16.inprb6",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_OPR,       SUBDEV_D16},
        [D16_SW_ON]    = {"d16.outrb6",    VDEV_PRIV,             -1,                        SUBDEV_D16},

        [A40_ILK]      = {"a40.inprb6",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ILK,       SUBDEV_A40},
        [A40_SW_OFF]   = {"a40.outrb6",    VDEV_PRIV,             -1,                        SUBDEV_A40},
        [A40_DMS]      = {"a40.adc30",                 VDEV_TUBE, V3H_A40D16_CHAN_DMS,       SUBDEV_A40},
        [A40_STS]      = {"a40.adc31",                 VDEV_TUBE, V3H_A40D16_CHAN_STS,       SUBDEV_A40},
        [A40_MES]      = {"a40.adc32",                 VDEV_TUBE, V3H_A40D16_CHAN_MES,       SUBDEV_A40},
        [A40_MU1]      = {"a40.adc33",                 VDEV_TUBE, V3H_A40D16_CHAN_MU1,       SUBDEV_A40},
        [A40_MU2]      = {"a40.adc34",                 VDEV_TUBE, V3H_A40D16_CHAN_MU2,       SUBDEV_A40},
    },
    {
        [D16_OUT]      = {"d16.out7",      VDEV_IMPR,             -1,                        SUBDEV_D16},
        [D16_OUT_RATE] = {"d16.out_rate7",             VDEV_TUBE, V3H_A40D16_CHAN_ISET_RATE, SUBDEV_D16},
        [D16_OUT_CUR]  = {"d16.out_cur7",  VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ISET_CUR,  SUBDEV_D16},
        [D16_OPR]      = {"d16.inprb7",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_OPR,       SUBDEV_D16},
        [D16_SW_ON]    = {"d16.outrb7",    VDEV_PRIV,             -1,                        SUBDEV_D16},

        [A40_ILK]      = {"a40.inprb7",    VDEV_IMPR | VDEV_TUBE, V3H_A40D16_CHAN_ILK,       SUBDEV_A40},
        [A40_SW_OFF]   = {"a40.outrb7",    VDEV_PRIV,             -1,                        SUBDEV_A40},
        [A40_DMS]      = {"a40.adc35",                 VDEV_TUBE, V3H_A40D16_CHAN_DMS,       SUBDEV_A40},
        [A40_STS]      = {"a40.adc36",                 VDEV_TUBE, V3H_A40D16_CHAN_STS,       SUBDEV_A40},
        [A40_MES]      = {"a40.adc37",                 VDEV_TUBE, V3H_A40D16_CHAN_MES,       SUBDEV_A40},
        [A40_MU1]      = {"a40.adc38",                 VDEV_TUBE, V3H_A40D16_CHAN_MU1,       SUBDEV_A40},
        [A40_MU2]      = {"a40.adc39",                 VDEV_TUBE, V3H_A40D16_CHAN_MU2,       SUBDEV_A40},
    },
};

static int ourc2sodc[V3H_A40D16_NUMCHANS];

static const char *devstate_names[] = {"a40._devstate", "d16._devstate"};

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

    int                 vn;

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
    V3H_STATE_UNKNOWN   = 0,
    V3H_STATE_DETERMINE,         // 1
    V3H_STATE_INTERLOCK,         // 2
    zV3H_STATE_RST_ILK_SET,      // 3
    zV3H_STATE_RST_ILK_DRP,      // 4
    zV3H_STATE_RST_ILK_CHK,      // 5

    V3H_STATE_IS_OFF,            // 6

    V3H_STATE_SW_ON_ENABLE,      // 7
    V3H_STATE_SW_ON_SET_ON,      // 8
    V3H_STATE_SW_ON_DRP_ON,      // 9
    V3H_STATE_SW_ON_UP,          // 10

    V3H_STATE_IS_ON,             // 11

    V3H_STATE_SW_OFF_DOWN,       // 12
    V3H_STATE_SW_OFF_CHK_I,      // 13
    V3H_STATE_SW_OFF_CHK_E,      // 14

    V3H_STATE_count
};

//--------------------------------------------------------------------

static void SwchToUNKNOWN(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    vdev_forget_all(&(me->ctx));
}

static void SwchToDETERMINE(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    ReturnInt32Datum(me->devid, V3H_A40D16_CHAN_C_ILK, me->c_ilk, 0);

    if (!me->out_val_set)
    {
        me->out_val     = me->cur[D16_OUT_CUR].v.i32;
        me->out_val_set = 1; /* Yes, we do it only ONCE, since later OUT_CUR may change upon our request, without any client's write */

        ReturnInt32Datum(me->devid, V3H_A40D16_CHAN_ISET, me->out_val, 0);
    }

    ReturnInt32Datum(me->devid,
                     V3H_A40D16_CHAN_ISET_CUR, me->cur[D16_OUT_CUR].v.i32, 0);

    if (me->cur[D16_OPR].v.i32)
        vdev_set_state(&(me->ctx), V3H_STATE_IS_ON);
    else
        vdev_set_state(&(me->ctx), V3H_STATE_IS_OFF);
}

static void SwchToINTERLOCK(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, D16_OUT,    0); // Drop setting
    SndCVal(me, D16_SW_ON,  0); // Drop "on" bit
    SndCVal(me, A40_SW_OFF, 0); // Set "off"
}

static int  IsAlwdSW_ON_ENABLE(void *devptr, int prev_state)
{
  privrec_t *me = devptr;
  int        ilk;

    ilk = me->cur[A40_ILK].v.i32;

    return
        (prev_state == V3H_STATE_IS_OFF  ||
         prev_state == V3H_STATE_INTERLOCK)    &&
        me->cur[D16_OUT_CUR].v.i32 == 0  &&
        ilk == 0;
}

static void SwchToON_ENABLE(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, A40_SW_OFF, 1); // Drop "off"
}

static void SwchToON_SET_ON(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, D16_SW_ON,  1); // Set "on" bit
}

static void SwchToON_DRP_ON(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, D16_SW_ON,  0); // Drop "on" bit
}

/* 26.09.2012: added to proceed to UP only if OPR is really on */
static int  IsAlwdSW_ON_UP(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return me->cur[D16_OPR].v.i32 != 0;
}

static void SwchToON_UP    (void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, D16_OUT, me->out_val);
}

static int  IsAlwdIS_ON    (void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return (abs(me->cur[D16_OUT_CUR].v.i32) - me->out_val) <= 305;
}

static int  IsAlwdSW_OFF_DOWN(void *devptr, int prev_state)
{
  privrec_t *me = devptr;

    return
        /* Regular "ON" state... */
        prev_state == V3H_STATE_IS_ON         ||
        /* ...and "switching-to-on" stages */
        prev_state == V3H_STATE_SW_ON_ENABLE  ||
        prev_state == V3H_STATE_SW_ON_SET_ON  ||
        prev_state == V3H_STATE_SW_ON_DRP_ON  ||
        prev_state == V3H_STATE_SW_ON_UP      ||
        /* ...or the state bit is just "on" */
        me->cur[D16_OPR].v.i32 != 0           ||
        me->cur[D16_OUT_CUR].v.i32 != 0
        ;
}

static void SwchToSW_OFF_DOWN(void *devptr, int prev_state)
{
  privrec_t *me = devptr;

    SndCVal(me, D16_OUT, 0);
    if (prev_state != V3H_STATE_IS_ON) SndCVal(me, D16_SW_ON, 0);
}

static int  IsAlwdSW_OFF_CHK_I(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    ////fprintf(stderr, "\tout_cur=%d\n", me->cur[D16_OUT_CUR].v.i32);
    return me->cur[D16_OUT_CUR].v.i32 == 0;
}

static void SwchToSW_OFF_CHK_I(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, A40_SW_OFF, 0); // Set "off"
}

static int  IsAlwdSW_OFF_CHK_E(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    ////fprintf(stderr, "\topr=%d\n", me->cur[D16_OPR].v.i32);
    return me->cur[D16_OPR].v.i32 == 0;
}

static vdev_state_dsc_t state_descr[] =
{
    [V3H_STATE_UNKNOWN]      = {0,       -1,                     NULL,               SwchToUNKNOWN,      NULL},
    [V3H_STATE_DETERMINE]    = {0,       -1,                     NULL,               SwchToDETERMINE,    NULL},
    [V3H_STATE_INTERLOCK]    = {0,       -1,                     NULL,               SwchToINTERLOCK,    NULL},

    [V3H_STATE_IS_OFF]       = {0,       -1,                     NULL,               NULL,               NULL},

    [V3H_STATE_SW_ON_ENABLE] = { 500000, V3H_STATE_SW_ON_SET_ON, IsAlwdSW_ON_ENABLE, SwchToON_ENABLE,    NULL},
    [V3H_STATE_SW_ON_SET_ON] = {1500000, V3H_STATE_SW_ON_DRP_ON, NULL,               SwchToON_SET_ON,    NULL},
    [V3H_STATE_SW_ON_DRP_ON] = { 500000, V3H_STATE_SW_ON_UP,     NULL,               SwchToON_DRP_ON,    NULL},
    [V3H_STATE_SW_ON_UP]     = {1,       V3H_STATE_IS_ON,        IsAlwdSW_ON_UP,     SwchToON_UP,        NULL},

    [V3H_STATE_IS_ON]        = {0,       -1,                     IsAlwdIS_ON,        NULL,               (int[]){D16_OUT_CUR, -1}},

    [V3H_STATE_SW_OFF_DOWN]  = {0,       V3H_STATE_SW_OFF_CHK_I, IsAlwdSW_OFF_DOWN,  SwchToSW_OFF_DOWN,  NULL},
    [V3H_STATE_SW_OFF_CHK_I] = {0,       V3H_STATE_SW_OFF_CHK_E, IsAlwdSW_OFF_CHK_I, SwchToSW_OFF_CHK_I, (int[]){D16_OUT_CUR, -1}},
    [V3H_STATE_SW_OFF_CHK_E] = {1,       V3H_STATE_IS_OFF,       IsAlwdSW_OFF_CHK_E, NULL,               (int[]){D16_OPR,     -1}},
};

static int state_important_channels[countof(state_descr)][SUBORD_NUMCHANS];

//////////////////////////////////////////////////////////////////////

static int v3h_a40d16_init_mod(void)
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
    for (x = 0;  x < countof(cdac2v3h_mappings[0]);  x++)
        if (cdac2v3h_mappings[0][x].mode != 0  &&  cdac2v3h_mappings[0][x].ourc >= 0)
            ourc2sodc[cdac2v3h_mappings[0][x].ourc] = x;

    return 0;
}

//////////////////////////////////////////////////////////////////////

static void v3h_a40d16_rw_p(int devid, void *devptr,
                            int action,
                            int count, int *addrs,
                            cxdtype_t *dtypes, int *nelems, void **values);
static void v3h_a40d16_sodc_cb(int devid, void *devptr,
                               int sodc, int32 val);

static vdev_sr_chan_dsc_t state_related_channels[] =
{
    {V3H_A40D16_CHAN_SWITCH_ON,  V3H_STATE_SW_ON_ENABLE, 0,                 CX_VALUE_DISABLED_MASK},
    {V3H_A40D16_CHAN_SWITCH_OFF, V3H_STATE_SW_OFF_DOWN,  0,                 CX_VALUE_DISABLED_MASK},
    {V3H_A40D16_CHAN_IS_ON,      -1,                     0,                 0},
    {-1,                         -1,                     0,                 0},
};

static int v3h_a40d16_init_d(int devid, void *devptr,
                             int businfocount, int businfo[],
                             const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;
  const char     *p  = auxinfo;
  const char     *endp;
  char            hiername[100];
  size_t          len;

    me->devid = devid;

    if (p == NULL  ||  *p == '\0')
    {
        DoDriverLog(devid, 0,
                    "%s(): base-CANADC40+CANDAC16-cpoint-hierarchy name is required in auxinfo",
                    __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }
    /*!!!NNN */
    endp = strchr(p, '/');
    if (endp == NULL)
    {
        DoDriverLog(devid, 0,
                    "%s(): '/' expected in auxinfo",
                    __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }
    len = endp - p;
    if (len > sizeof(hiername) - 1)
        len = sizeof(hiername) - 1;
    memcpy(hiername, p, len); hiername[len] = '\0';

    p = endp + 1;
    if (*p < '0'  ||  *p >= '0' + EIGHTDEVS)
    {
        DoDriverLog(devid, 0,
                    "%s(): '/N' (N in [0...%d]) expected in auxinfo",
                    __FUNCTION__, EIGHTDEVS);
        return -CXRF_CFG_PROBL;
    }
    me->vn = *p - '0';

    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = cdac2v3h_mappings[me->vn];
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = v3h_a40d16_rw_p;
    me->ctx.sodc_cb        = v3h_a40d16_sodc_cb;

    me->ctx.our_numchans             = V3H_A40D16_NUMCHANS;
    me->ctx.chan_state_n             = V3H_A40D16_CHAN_V3H_STATE;
    me->ctx.state_unknown_val        = V3H_STATE_UNKNOWN;
    me->ctx.state_determine_val      = V3H_STATE_DETERMINE;
    me->ctx.state_count              = countof(state_descr);
    me->ctx.state_descr              = state_descr;
    me->ctx.state_related_channels   = state_related_channels;
    me->ctx.state_important_channels = state_important_channels;

    SetChanRDs       (devid, V3H_A40D16_CHAN_ISET,      1, 1000000.0, 0.0);
    SetChanRDs       (devid, V3H_A40D16_CHAN_ISET_RATE, 1, 1000000.0, 0.0);
    SetChanRDs       (devid, V3H_A40D16_CHAN_ISET_CUR,  1, 1000000.0, 0.0);
    SetChanRDs/*!!!*/(devid, V3H_A40D16_CHAN_RD_base,   5, 1000000.0, 0.0);
    SetChanReturnType(devid, V3H_A40D16_CHAN_ISET_CUR,  1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, V3H_A40D16_CHAN_ILK,       1, IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, V3H_A40D16_CHAN_C_ILK,     1, IS_AUTOUPDATED_TRUSTED);

    return vdev_init(&(me->ctx), devid, devptr, WORK_HEARTBEAT_PERIOD, hiername);
}

static void v3h_a40d16_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void v3h_a40d16_sodc_cb(int devid, void *devptr,
                               int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;

    /* If not all info is gathered yet, then there's nothing else to do yet */
    if (me->ctx.cur_state == V3H_STATE_UNKNOWN) return;

    /* Perform state transitions depending on received data */
    if (sodc == A40_ILK  &&  val != 0)
    {
        if (me->c_ilk != val)
        {
            me->c_ilk = val;
            ReturnInt32Datum(devid, V3H_A40D16_CHAN_C_ILK, val, 0);
        }
        vdev_set_state(&(me->ctx), V3H_STATE_INTERLOCK);
    }
    if (/* 26.09.2012: when OPR bit disappears, we should better switch off */
        sodc == D16_OPR  &&  val == 0  &&
        (me->ctx.cur_state == V3H_STATE_SW_ON_DRP_ON  ||
         me->ctx.cur_state == V3H_STATE_SW_ON_UP      ||
         me->ctx.cur_state == V3H_STATE_IS_ON))
    {
        vdev_set_state(&(me->ctx), V3H_STATE_SW_OFF_DOWN);
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

static void v3h_a40d16_rw_p(int devid, void *devptr,
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

        if      (sodc >= 0  &&  cdac2v3h_mappings[0][sodc].mode & VDEV_TUBE)
        {
            if (action == DRVA_WRITE)
                SndCVal(me, sodc, val);
            else
                if (me->cur[sodc].rcvd)
                    ReturnInt32DatumTimed(devid, chn, me->cur[sodc].v.i32,
                                          me->cur[sodc].flgs,
                                          me->cur[sodc].ts);
        }
        else if (chn == V3H_A40D16_CHAN_ISET)
        {
            ////fprintf(stderr, "ISET action=%d\n", action);
            if (action == DRVA_WRITE)
            {
                if (val > MAX_ALWD_VAL) val = MAX_ALWD_VAL;
                if (val < MIN_ALWD_VAL) val = MIN_ALWD_VAL;
                me->out_val     = val;
                me->out_val_set = 1;

                if (me->ctx.cur_state == V3H_STATE_IS_ON  ||
                    me->ctx.cur_state == V3H_STATE_SW_ON_UP)
                {
                    SndCVal(me, D16_OUT, me->out_val);
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
////if (chn != V3H_A40D16_CHAN_IS_ON) fprintf(stderr, "[%d] %d %s %d\n", devid, me->ctx.cur_state, chn==V3H_A40D16_CHAN_SWITCH_ON?"on":"off", val);
            ReturnInt32Datum(devid, chn, val, 0);
        }
        else if (chn == V3H_A40D16_CHAN_RESET_STATE)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                vdev_set_state(&(me->ctx), V3H_STATE_DETERMINE);
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == V3H_A40D16_CHAN_RESET_C_ILKS)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
            {
                me->c_ilk = 0;
                ReturnInt32Datum(devid, V3H_A40D16_CHAN_C_ILK, me->c_ilk, 0);
            }
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == V3H_A40D16_CHAN_IS_ON)
        {
            val = (me->ctx.cur_state >= V3H_STATE_SW_ON_ENABLE  &&
                   me->ctx.cur_state <= V3H_STATE_IS_ON);
            ReturnInt32Datum(devid, chn, val, 0);
        }
        else if (chn == V3H_A40D16_CHAN_V3H_STATE)
        {
            ReturnInt32Datum(devid, chn, me->ctx.cur_state, 0);
        }
        else if (chn >= V3H_A40D16_CHAN_C_ILK)
        {
            ReturnInt32Datum(devid, chn, me->c_ilk, 0);
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(v3h_a40d16, "CANADC40+CANDAC16-controlled VCh-300",
                   v3h_a40d16_init_mod, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   v3h_a40d16_init_d, v3h_a40d16_term_d, v3h_a40d16_rw_p);
