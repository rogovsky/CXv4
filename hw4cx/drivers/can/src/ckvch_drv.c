#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/ckvch_drv_i.h"


/* CKVCH specifics */

enum
{
    DEVTYPE   = 8, /* CKVCH is 8 */
};

enum
{
    DESC_GET_STATUS = 0x01,
    DESC_WRITE_OUTS = 0x02,
};

static int num_outs_db[4] =
{
    0,  // 00 - absent, impossible
    4,  // 01 - 4x2_1
    1,  // 10 - 1x8_1
    2,  // 11 - 2x4_1
};
static int max_vals_db[4] =
{
    0,  //
    1,  //
    7,  //
    3,  //
};
static int shift_ks_db[4] =
{
    0,
    1,
    0,
    2,
};

/**/

typedef struct
{
    cankoz_vmt_t    *lvmt;
    int              devid;
    int              handle;

    int              force_config;

    int              num_outs;
    struct
    {
        int    rcvd;
        int    pend;

        int32  cur_val;
        int32  req_val;
        int32  req_msk;
    }                out;

    int              config;
    int              hw_ver;
    int              sw_ver;
} privrec_t;

static psp_paramdescr_t ckvch_params[] =
{
    PSP_P_FLAG("detect", privrec_t, force_config, CKVCH_CONFIG_ABSENT, 1),
    PSP_P_FLAG("4x2_1",  privrec_t, force_config, CKVCH_CONFIG_4x2_1,  0),
    PSP_P_FLAG("1x8_1",  privrec_t, force_config, CKVCH_CONFIG_1x8_1,  0),
    PSP_P_FLAG("2x4_1",  privrec_t, force_config, CKVCH_CONFIG_2x4_1,  0),
    PSP_P_END()
};

//////////////////////////////////////////////////////////////////////

static void send_wrouts_cmds(privrec_t *me)
{
  uint8 o_val = (me->out.cur_val &~ me->out.req_msk) |
                (me->out.req_val &  me->out.req_msk);

    if (me->lvmt->q_enqueue_v(me->handle, SQ_REPLACE_NOTFIRST,
                              SQ_TRIES_ONS, 0,
                              NULL/*!!!*/, NULL,
                              1, 2, DESC_WRITE_OUTS, o_val) == SQ_NOTFOUND)
        me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 1, DESC_GET_STATUS);

    me->out.pend    = 0;
}

//////////////////////////////////////////////////////////////////////

static void ckvch_ff(int devid, void *devptr, int is_a_reset);
static void ckvch_in(int devid, void *devptr,
                     int desc,  size_t dlc, uint8 *data);

static int ckvch_init_d(int devid, void *devptr, 
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
                               ckvch_ff, ckvch_in,
                               CKVCH_NUMCHANS * 2/*!!!*/,
                               CANKOZ_LYR_OPTION_FFPROC_BEFORE_RESET |
                               CANKOZ_LYR_OPTION_FFPROC_AFTER_RESET);
    if (me->handle < 0) return me->handle;

    SetChanReturnType(devid, CKVCH_CHAN_NUM_OUTS,    1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, CKVCH_CHAN_CONFIG_BITS, 1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, CKVCH_CHAN_HW_VER,      1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, CKVCH_CHAN_SW_VER,      1, IS_AUTOUPDATED_TRUSTED);

    if (me->force_config != CKVCH_CONFIG_ABSENT)
    {
        me->config   = me->force_config;
        me->num_outs = num_outs_db[me->config];
        ReturnInt32Datum(devid, CKVCH_CHAN_CONFIG_BITS, me->config,   0);
        ReturnInt32Datum(devid, CKVCH_CHAN_NUM_OUTS,    me->num_outs, 0);
    }

    return DEVSTATE_OPERATING;
}

static void ckvch_ff(int devid, void *devptr, int is_a_reset)
{
  privrec_t *me = devptr;

    if (is_a_reset == CANKOZ_LYR_OPTION_FFPROC_BEFORE_RESET)
    {
        bzero(&(me->out), sizeof(me->out));
        if (me->force_config == CKVCH_CONFIG_ABSENT)
        {
            me->num_outs = 0;
            me->config   = 0;
        }
    }

    me->lvmt->get_dev_ver(me->handle, &(me->hw_ver), &(me->sw_ver), NULL);
    ReturnInt32Datum(me->devid, CKVCH_CHAN_HW_VER, me->hw_ver, 0);
    ReturnInt32Datum(me->devid, CKVCH_CHAN_SW_VER, me->sw_ver, 0);

    me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1, DESC_GET_STATUS);
}

static void ckvch_in(int devid, void *devptr,
                     int desc,  size_t dlc, uint8 *data)
{
  privrec_t  *me     = (privrec_t *) devptr;

  int          l;     // Line #

  int          vals_addrs [CKVCH_CHAN_OUT_n_count];
  cxdtype_t    vals_dtypes[CKVCH_CHAN_OUT_n_count];
  int          vals_nelems[CKVCH_CHAN_OUT_n_count];
  rflags_t     vals_rflags[CKVCH_CHAN_OUT_n_count];
  int32        vals_vals  [CKVCH_CHAN_OUT_n_count];
  void        *vals_ptrs  [CKVCH_CHAN_OUT_n_count];

    switch (desc)
    {
        case DESC_GET_STATUS:
        case CANKOZ_DESC_GETDEVSTAT:
            if (dlc < 2) return;

            me->config   = me->force_config != CKVCH_CONFIG_ABSENT?
                           me->force_config : (data[1] >> 4) & 3;
            me->num_outs = num_outs_db[me->config];
            ReturnInt32Datum(devid, CKVCH_CHAN_CONFIG_BITS, me->config,   0);
            ReturnInt32Datum(devid, CKVCH_CHAN_NUM_OUTS,    me->num_outs, 0);

            for (l = 0;  l < CKVCH_CHAN_OUT_n_count;  l++)
            {
                vals_addrs [l] = CKVCH_CHAN_OUT_n_base + l;
                vals_dtypes[l] = CXDTYPE_INT32;
                vals_nelems[l] = 1;
                vals_rflags[l] = l < me->num_outs? 0 
                                                 : CXRF_UNSUPPORTED;
                vals_vals  [l] = l < me->num_outs? (data[1] >> shift_ks_db[me->config] * l) & max_vals_db[me->config]
                                                 : 0;
                vals_ptrs  [l] = vals_vals + l;
            }

            /* Do we have a pending write request? */
            if (me->out.pend)
                send_wrouts_cmds(me);

            ReturnDataSet(devid, CKVCH_CHAN_OUT_n_count,
                          vals_addrs, vals_dtypes, vals_nelems,
                          vals_ptrs,  vals_rflags, NULL);
            ReturnInt32Datum(devid, CKVCH_CHAN_OUT_4BITS, data[1] & 0x0F, 0);

            break;
    }
}

static void ckvch_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = (privrec_t *)devptr;

  int          n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int          chn;   // channel
  int          l;     // Line #
  int32        val;   // Value
  int32        mask;
  int          shift;

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

        if ((chn >= CKVCH_CHAN_OUT_n_base  &&
             chn <  CKVCH_CHAN_OUT_n_base + me->num_outs)  ||
             chn == CKVCH_CHAN_OUT_4BITS)
        {
            if (action == DRVA_WRITE)
            {
                if (chn == CKVCH_CHAN_OUT_4BITS)
                {
                    val &= 0x0F;
                    mask = 0x0F;
                }
                else
                {
                    l = chn - CKVCH_CHAN_OUT_n_base;
                    // 0. Impose limits
                    if (val < 0) val = 0;
                    if (val > max_vals_db[me->config])
                        val = max_vals_db[me->config];

                    // 1. Shift val and mask as required
                    shift = shift_ks_db[me->config] * l;
                    val   = val                     << shift;
                    mask  = max_vals_db[me->config] << shift;
                }

                // 2. Store data for sending
                me->out.req_val  = (me->out.req_val &~ mask) | val;
                me->out.req_msk |= mask;

                // 3. Request sending
                me->out.pend = 1;
                /* May we perform write right now? */
                if (me->out.rcvd)
                {
                    send_wrouts_cmds(me);
                }
                /* No, we should request read first... */
                else
                {
                    me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                                      DESC_GET_STATUS);
                }
            }
            else
                me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                                  DESC_GET_STATUS);
        }
        else if (chn == CKVCH_CHAN_NUM_OUTS  ||  chn == CKVCH_CHAN_CONFIG_BITS)
        {/* Do-nothing -- returned from _in() */}
        else if (chn == CKVCH_CHAN_HW_VER  ||  chn == CKVCH_CHAN_SW_VER)
        {/* Do-nothing -- returned from _ff() */}
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(ckvch, "CKVCH driver",
                   NULL, NULL,
                   sizeof(privrec_t), ckvch_params,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   ckvch_init_d, NULL, ckvch_rw_p);
