#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/weld02_drv_i.h"


enum
{
    DEVTYPE = 0x0F,
};

enum
{
    DESC_GETMES      = 0x03,
    DESC_SET_AA_base = 0x70, // "with AutoAnswer"
    DESC_SET_base    = 0x80,
    DESC_QRY_base    = 0x90,
};

enum {HEARTBEAT_USECS = 1*1000*1000};

static int max_set_values[WELD02_SET_n_count] =
{
    4095,
    4095,
    4095,
    4095,
    4095, //???
    4095, //???
    3,
    1,
};


/*  */

typedef struct
{
    cankoz_vmt_t    *lvmt;
    int              devid;
    int              handle;

    int              supports_adc9;

    struct
    {
        int32  vals[WELD02_NUMCHANS];
        int    read[WELD02_NUMCHANS];
    } known;
} privrec_t;


static psp_paramdescr_t weld02_params[] =
{
    PSP_P_END()
};


static void weld02_ff (int devid, void *devptr, int is_a_reset);
static void weld02_in (int devid, void *devptr,
                       int desc, size_t dlc, uint8 *data);
static void weld02_hbt(int devid, void *devptr,
                       sl_tid_t tid,
                       void *privptr);
static void weld02_rw_p(int devid, void *devptr,
                        int action,
                        int count, int *addrs,
                        cxdtype_t *dtypes, int *nelems, void **values);

static int weld02_init_d(int devid, void *devptr, 
                         int businfocount, int businfo[],
                         const char *auxinfo)
{
  privrec_t *me    = (privrec_t *) devptr;
    
    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "ENTRY %s(%s)", __FUNCTION__, auxinfo);

    /* Initialize interface */
    me->lvmt   = GetLayerVMT(devid);
    me->devid  = devid;
    me->handle = me->lvmt->add(devid, devptr,
                               businfocount, businfo,
                               DEVTYPE,
                               weld02_ff, weld02_in,
                               WELD02_NUMCHANS*2/*!!!*/,
                               CANKOZ_LYR_OPTION_NONE);
    if (me->handle < 0) return me->handle;

    ////sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, weld02_hbt, NULL);

    SetChanRDs       (devid, WELD02_CHAN_SET_UH,     1,   4095.0/6000,     0.0);
    SetChanRDs       (devid, WELD02_CHAN_SET_UL,     1,   4095.0/6000,     0.0);
    SetChanRDs       (devid, WELD02_CHAN_SET_UN,     1,   4095.0/125,      0.0);
    SetChanRDs       (devid, WELD02_CHAN_SET_UP,     1,   4095.0/250/*!!!???*/, 0.0);
    SetChanRDs       (devid, WELD02_CHAN_SET_IH,     1,       10,          0.0);
    SetChanRDs       (devid, WELD02_CHAN_SET_IL,     1,       10,          0.0);
    SetChanRDs       (devid, WELD02_CHAN_MES_UH,     1,   4095.0/6000,     0.0);
    SetChanRDs       (devid, WELD02_CHAN_MES_UL,     1,   4095.0/6000,     0.0);
    SetChanRDs       (devid, WELD02_CHAN_MES_UN,     1,   4095.0/125,      0.0);
    SetChanRDs       (devid, WELD02_CHAN_MES_UP,     1,   4095.0/2.5,      0.0);
    SetChanRDs       (devid, WELD02_CHAN_MES_IH,     1, (40950.0/2500)*4,  0.0);
    SetChanRDs       (devid, WELD02_CHAN_MES_IL,     1, (40950.0/2500)*4,  0.0);
    SetChanRDs       (devid, WELD02_CHAN_MES_U_HEAT, 1,   4095.0/2.5,      0.0);
    SetChanRDs       (devid, WELD02_CHAN_MES_U_HIGH, 1,   4095.0/6000,     0.0);
    SetChanRDs       (devid, WELD02_CHAN_MES_U_POWR, 1,   4095.0/50.0,     0.0);
    SetChanRDs       (devid, WELD02_CHAN_MES_I_INDR, 1,   4095.0/250.0,    0.0);

    return DEVSTATE_OPERATING;
}

static void weld02_term_d(int devid, void *devptr __attribute__((unused)))
{
  privrec_t *me    = (privrec_t *) devptr;

}

static inline void ReSendSetting(int devid, privrec_t *me, int cn)
{
  static cxdtype_t  dt_int32 = CXDTYPE_INT32;
  static int        nels_1   = 1;

  void             *vp       = me->known.vals + cn;

    if (me->known.read[cn])
        weld02_rw_p(devid, me,
                    DRVA_WRITE,
                    1, &cn,
                    &dt_int32,
                    &nels_1,
                    &vp);
}

static void weld02_ff (int devid, void *devptr, int is_a_reset)
{
  privrec_t *me    = (privrec_t *) devptr;
  int        sw_ver;

    me->lvmt->get_dev_ver(me->handle, NULL, &sw_ver, NULL);
    me->supports_adc9 = (sw_ver >= 9);

    SetChanRDs(devid, WELD02_CHAN_MES_UN, 1,
               (sw_ver <= 8)? 4095.0/125 : 4095.0/25, 
               0.0);

    if (is_a_reset)
    {
        ReSendSetting(devid, me, WELD02_CHAN_SET_UH);
        ReSendSetting(devid, me, WELD02_CHAN_SET_UL);
        ReSendSetting(devid, me, WELD02_CHAN_SET_UN);
    }
}

static void weld02_in (int devid, void *devptr __attribute__((unused)),
                       int desc, size_t dlc, uint8 *data)
{
  privrec_t  *me     = (privrec_t *) devptr;
  int         attr;
  int32       val;

    switch (desc)
    {
        case DESC_GETMES:
            if (dlc < 4) return;
            attr = data[1] & 0x1F;
            me->lvmt->q_erase_and_send_next_v(me->handle, 2, desc, attr);
            
            if (attr < WELD02_MES_n_count)
            {
                val    = data[2] + data[3] * 256;
                if (
                    (attr == WELD02_SET_n_IH  ||  attr == WELD02_SET_n_IL)
                    &&
                    ((data[1] & 0x40) == 0)
                   )
                    val *= 10;
                me->known.vals[WELD02_CHAN_MES_base + attr] = val;
                me->known.read[WELD02_CHAN_MES_base + attr] = 1;
                ReturnInt32Datum(devid, WELD02_CHAN_MES_base + attr, val, 0);
            }
            else
            {
                DoDriverLog(devid, 0, "%s(): DESC_GETMES attr=%d, >max=%d",
                            __FUNCTION__, attr, WELD02_MES_n_count-1);
            }
            break;

        case DESC_QRY_base ... DESC_QRY_base + WELD02_SET_n_count - 1:
            if (dlc < 4) return;
            attr = desc - DESC_QRY_base;
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);

            val    = (data[2] << 8) | data[3];
            ReturnInt32Datum(devid, WELD02_CHAN_SET_base + attr, val, 0);
            break;

        case DESC_SET_AA_base ... DESC_SET_AA_base + WELD02_SET_n_count - 1:
            if (dlc < 4) return;
            attr = desc - DESC_SET_AA_base;
            me->lvmt->q_erase_and_send_next_v(me->handle, -1, desc);

            val    = (data[2] << 8) | data[3];
            ReturnInt32Datum(devid, WELD02_CHAN_SET_base + attr, val, 0);
            break;
    }
}

static void weld02_hbt(int devid, void *devptr,
                       sl_tid_t tid,
                       void *privptr)
{
  privrec_t  *me    = (privrec_t *) devptr;

    sl_enq_tout_after(devid, devptr, HEARTBEAT_USECS, weld02_hbt, NULL);

    ReSendSetting(devid, me, WELD02_CHAN_SET_UH);
    ReSendSetting(devid, me, WELD02_CHAN_SET_UL);
    ReSendSetting(devid, me, WELD02_CHAN_SET_UN);
}

static void weld02_rw_p(int devid, void *devptr,
                        int action,
                        int count, int *addrs,
                        cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t  *me     = (privrec_t *) devptr;
  int         n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int         chn;   // channel
  int32       val;   // Value

  int         attr;
  int         code;

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

        if      (chn >= WELD02_CHAN_SET_base  &&
                 chn <= WELD02_CHAN_SET_base + WELD02_SET_n_count-1)
        {
            attr = chn - WELD02_CHAN_SET_base;
#define USE_AA 1
#if USE_AA
            if (action == DRVA_WRITE)
            {
                code = val;
                if (code < 0)                    code = 0;
                if (code > max_set_values[attr]) code = max_set_values[attr];

                me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 4,
                                  DESC_SET_AA_base + attr, 0,
                                  (code >> 8) & 0x0F, code & 0xFF);
            }
            else
            {
                me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                                  DESC_QRY_base + attr);
            }
#else
            if (action == DRVA_WRITE)
            {
                code = val;
                if (code < 0)                    code = 0;
                if (code > max_set_values[attr]) code = max_set_values[attr];

                me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                                      SQ_TRIES_DIR, 0,
                                      NULL, NULL,
                                      0, 4,
                                      DESC_SET_base + attr, 0,
                                      (code >> 8) & 0x0F, code & 0xFF);
            }

            /* Perform read anyway */
            me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                              DESC_QRY_base + attr);
#endif
        }
        else if (chn >= WELD02_CHAN_MES_base  &&
                 chn <= WELD02_CHAN_MES_base + WELD02_MES_n_count-1  &&
                 (chn != WELD02_CHAN_MES_I_INDR  ||  me->supports_adc9))
        {
            attr = chn - WELD02_CHAN_MES_base;
            me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 2,
                              DESC_GETMES, attr);
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

/* Metric */

DEFINE_CXSD_DRIVER(weld02, "Zharikov-Repkov WELD02 welding controller",
                   NULL, NULL,
                   sizeof(privrec_t), weld02_params,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   weld02_init_d, weld02_term_d, weld02_rw_p);
