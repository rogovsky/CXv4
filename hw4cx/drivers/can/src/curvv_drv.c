#include "timeval_utils.h"

#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/curvv_drv_i.h"


/*=== CURVV specifics ==============================================*/

enum
{
    DEVTYPE = 10
};

enum
{
    DESC_READ_PWR_TTL = 0xE8,
    DESC_WRITE_PWR    = 0xE9,
};

//////////////////////////////////////////////////////////////////////

typedef struct
{
    cankoz_vmt_t    *lvmt;
    int              devid;
    int              handle;

    struct
    {
        int     rcvd;
        int     pend;
        
        int32   cur_val; /*!!! Note: these 3 should be uint8, and cur_val be copied into int32 for ReturnChan*() */
        int32   req_val;
        int32   req_msk;
        int32   cur_inp;
    }                    ptregs; // 'pt' stands for Pwr,Ttl
} privrec_t;

//////////////////////////////////////////////////////////////////////

static void send_wrpwr_cmds(privrec_t *me)
{
    if (me->lvmt->q_enqueue_v(me->handle, SQ_REPLACE_NOTFIRST,
                              SQ_TRIES_ONS, 0,
                              NULL, NULL,
                              1, 2,
                              DESC_WRITE_PWR,
                              (me->ptregs.cur_val &~ me->ptregs.req_msk) |
                              (me->ptregs.req_val &  me->ptregs.req_msk)
                             ) == SQ_NOTFOUND)
        me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 1,
                          DESC_READ_PWR_TTL);
    me->ptregs.pend    = 0;
}

//////////////////////////////////////////////////////////////////////

static void curvv_ff (int devid, void *devptr, int is_a_reset);
static void curvv_in (int devid, void *devptr,
                      int desc,  size_t dlc, uint8 *data);

static int  curvv_init_d(int devid, void *devptr,
                         int businfocount, int businfo[],
                         const char *auxinfo)
{
  privrec_t *me = devptr;

    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "%s([%d]:\"%s\")",
                __FUNCTION__, businfocount, auxinfo);

    /* Initialize interface */
    me->lvmt   = GetLayerVMT(devid);
    me->devid  = devid;
    me->handle = me->lvmt->add(devid, devptr,
                               businfocount, businfo,
                               DEVTYPE,
                               curvv_ff, curvv_in,
                               CURVV_NUMCHANS*2/*!!!*/,
                               CANKOZ_LYR_OPTION_FFPROC_BEFORE_RESET |
                               CANKOZ_LYR_OPTION_FFPROC_AFTER_RESET);
    if (me->handle < 0) return me->handle;
    me->lvmt->has_regs(me->handle, CURVV_CHAN_REGS_base);

    SetChanReturnType(devid, CURVV_CHAN_HW_VER, 1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, CURVV_CHAN_SW_VER, 1, IS_AUTOUPDATED_TRUSTED);

    return DEVSTATE_OPERATING;
}

static void curvv_ff (int devid, void *devptr, int is_a_reset)
{
  privrec_t *me = devptr;
  int        hw_ver;
  int        sw_ver;

    if (is_a_reset == CANKOZ_LYR_OPTION_FFPROC_BEFORE_RESET)
        bzero(&(me->ptregs), sizeof(me->ptregs));

    me->lvmt->get_dev_ver(me->handle, &hw_ver, &sw_ver, NULL);
    ReturnInt32Datum(me->devid, CURVV_CHAN_HW_VER, hw_ver, 0);
    ReturnInt32Datum(me->devid, CURVV_CHAN_SW_VER, sw_ver, 0);
}

static void curvv_in (int devid, void *devptr,
                      int desc,  size_t dlc, uint8 *data)
{
  privrec_t  *me       = (privrec_t *) devptr;
  int          n;
  int32        pwr_vals[CURVV_CHAN_PWR_Bn_count];
  int32        ttl_vals[CURVV_CHAN_TTL_Bn_count];

  enum {MAX_OF_COUNTS = CURVV_CHAN_PWR_Bn_count > CURVV_CHAN_TTL_Bn_count?
                        CURVV_CHAN_PWR_Bn_count : CURVV_CHAN_TTL_Bn_count};

  int          vals_addrs [MAX_OF_COUNTS];
  cxdtype_t    vals_dtypes[MAX_OF_COUNTS];
  int          vals_nelems[MAX_OF_COUNTS];
  rflags_t     vals_rflags[MAX_OF_COUNTS];
  void        *vals_ptrs  [MAX_OF_COUNTS];

    switch (desc)
    {
        case DESC_READ_PWR_TTL:
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);
            if (dlc < 6) return;

            /* Extract data into a ready-for-ReturnDataSet format */
            me->ptregs.cur_val = data[1];
            me->ptregs.cur_inp = data[3] | ((uint32)data[4] << 8) | ((uint32)data[5] << 16);
            for (n = 0;  n < CURVV_CHAN_PWR_Bn_count;  n++)
                pwr_vals[n] = (me->ptregs.cur_val & (1 << n)) != 0;
            for (n = 0;  n < CURVV_CHAN_TTL_Bn_count;  n++)
                ttl_vals[n] = (me->ptregs.cur_inp & (1 << n)) != 0;
            me->ptregs.rcvd = 1;

            /* Do we have a pending write request? */
            if (me->ptregs.pend != 0)
            {
                send_wrpwr_cmds(me);
            }
            
            /* And return requested data */
            for (n = 0;  n < MAX_OF_COUNTS;  n++)
            {
                vals_dtypes[n] = CXDTYPE_INT32;
                vals_nelems[n] = 1;
            }
            bzero(vals_rflags, sizeof(vals_rflags));

            for (n = 0;  n < CURVV_CHAN_PWR_Bn_count;  n++)
            {
                vals_addrs[n] = CURVV_CHAN_PWR_Bn_base + n;
                vals_ptrs [n] = pwr_vals + n;
            }
            ReturnDataSet   (devid, CURVV_CHAN_PWR_Bn_count,
                             vals_addrs, vals_dtypes, vals_nelems,
                             vals_ptrs,  vals_rflags, NULL);

            for (n = 0;  n < CURVV_CHAN_TTL_Bn_count;  n++)
            {
                vals_addrs[n] = CURVV_CHAN_TTL_Bn_base + n;
                vals_ptrs [n] = ttl_vals + n;
            }
            ReturnDataSet   (devid, CURVV_CHAN_TTL_Bn_count,
                             vals_addrs, vals_dtypes, vals_nelems,
                             vals_ptrs,  vals_rflags, NULL);

            ReturnInt32Datum(devid, CURVV_CHAN_PWR_8B,  me->ptregs.cur_val, 0);
            ReturnInt32Datum(devid, CURVV_CHAN_TTL_24B, me->ptregs.cur_inp, 0);
            break;
    }
}

static void curvv_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = (privrec_t *)devptr;

  int          n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int          chn;   // channel
  int32        val;   // Value
  uint8        mask;

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

        if      (chn >= CURVV_CHAN_REGS_base  &&  chn <= CURVV_CHAN_REGS_last)
        {
            me->lvmt->regs_rw(me->handle, action, chn, &val);
        }
        /* Read? */
        else if (
                 ((
                   (chn >= CURVV_CHAN_PWR_Bn_base  &&
                    chn <  CURVV_CHAN_PWR_Bn_base + CURVV_CHAN_PWR_Bn_count)
                   ||
                   chn == CURVV_CHAN_PWR_8B
                  )  &&  action == DRVA_READ)
                 ||
                 (  chn >= CURVV_CHAN_TTL_Bn_base  &&
                    chn <  CURVV_CHAN_TTL_Bn_base + CURVV_CHAN_TTL_Bn_count)
                 ||
                 chn   == CURVV_CHAN_TTL_24B
                )
        {
            me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                              DESC_READ_PWR_TTL);
        }
        /* Some form of write? */
        else if ((chn >= CURVV_CHAN_PWR_Bn_base  &&
                  chn <  CURVV_CHAN_PWR_Bn_base + CURVV_CHAN_PWR_Bn_count)
                 || chn == CURVV_CHAN_PWR_8B)
        {
            /* Decide, what to write... */
            if (chn == CURVV_CHAN_PWR_8B)
            {
                me->ptregs.req_val = val;
                me->ptregs.req_msk = 0xFF;
            }
            else
            {
                mask = 1 << (chn - CURVV_CHAN_PWR_Bn_base);
                if (val != 0) me->ptregs.req_val |=  mask;
                else          me->ptregs.req_val &=~ mask;
                me->ptregs.req_msk |= mask;
            }
    
            me->ptregs.pend = 1;
            /* May we perform write right now? */
            if (me->ptregs.rcvd)
            {
                send_wrpwr_cmds(me);
            }
            /* No, we should request read first... */
            else
            {
                me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                                  DESC_READ_PWR_TTL);
            }
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(curvv, "CURVV driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   curvv_init_d, NULL, curvv_rw_p);
