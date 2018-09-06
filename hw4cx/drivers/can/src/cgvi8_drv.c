#include "timeval_utils.h"

#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/cgvi8_drv_i.h"


/*=== CGVI8 specifics ==============================================*/

enum
{
    DEVTYPE_CGVI8  = 6,  /* CGVI8 is 6 */
    DEVTYPE_CGVI8M = 32,
};

enum
{
    DESC_WRITE_n_BASE   = 0,
    DESC_READ_n_BASE    = 0x10,

    DESC_WRITE_MODE     = 0xF0,
    DESC_WRITE_BASEBYTE = 0xF1,
    DESC_PROGSTART      = 0xF7
};


// Note: 1quant=100ns

static inline int32 prescaler2factor(int prescaler)
{
    return 1 << (prescaler & 0x0F);
}

static inline int32  code2quants(uint16 code,   int prescaler)
{
    return code * prescaler2factor(prescaler);
}

static inline uint16 quants2code(int32  quants, int prescaler)
{
    return quants / prescaler2factor(prescaler);
}


//////////////////////////////////////////////////////////////////////

#define DEBUG_CGVI8M_MASK 1
typedef struct
{
    cankoz_vmt_t    *lvmt;
    int              devid;
    int              handle;

    int                  hw_ver;
    int                  sw_ver;

    int32                cur_codes     [CGVI8_CHAN_OUT_count];
    int                  cur_codes_read[CGVI8_CHAN_OUT_count];

#if DEBUG_CGVI8M_MASK
        int rd_req_sent;
#endif
    struct
    {
        /* General behavioral properties */
        int    rcvd;          // Was mode ever read since start/reset
        int    pend;          // A write request is pending

        /* "Output mask" */
        uint8  cur_val;       // Current known-to-be-in-device value
        uint8  req_val;       // Required-for-write value
        uint8  req_msk;       // Mask showing which bits of req_val should be written

        /* Prescaler */
        uint8  cur_prescaler; // Current known-to-be-in-device
        uint8  req_prescaler; // Required-for-write value
        uint8  wrt_prescaler; // Was a write-to-prescaler requested?
#if DEBUG_CGVI8M_MASK
        uint8  rd_req_sent;
        uint8  last_known_mask;
#endif
    }                    mode;
} privrec_t;

//////////////////////////////////////////////////////////////////////

static void send_wrmode_cmd(privrec_t *me)
{
    me->lvmt->q_enq_ons_v(me->handle, SQ_ALWAYS, 3,
                          DESC_WRITE_MODE,
                          (me->mode.cur_val &~ me->mode.req_msk) |
                          (me->mode.req_val &  me->mode.req_msk),
                          me->mode.wrt_prescaler? me->mode.req_prescaler
                                                : me->mode.cur_prescaler);

    me->mode.pend          = 0;
//    me->mode.wrt_prescaler = 0; /* With this uncommented, prescaler stops changing upon mode loads */
}

//////////////////////////////////////////////////////////////////////

static void cgvi8_ff (int devid, void *devptr, int is_a_reset);
static void cgvi8_in (int devid, void *devptr,
                      int desc,  size_t dlc, uint8 *data);

#if DEBUG_CGVI8M_MASK
static void cgvi8_mask_hbt(int devid, void *devptr,
                           sl_tid_t tid, void *privptr)
{
  privrec_t *me = devptr;

    sl_enq_tout_after(devid, devptr, 60*1000000, cgvi8_mask_hbt, NULL);
    me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT,
                      1, CANKOZ_DESC_GETDEVSTAT);
}
#endif
static int  cgvi8_init_d(int devid, void *devptr,
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
                               DEVTYPE_CGVI8,
                               cgvi8_ff, cgvi8_in,
                               CGVI8_NUMCHANS*2/*!!!*/,
                               CANKOZ_LYR_OPTION_FFPROC_BEFORE_RESET |
                               CANKOZ_LYR_OPTION_FFPROC_AFTER_RESET);
    if (me->handle < 0) return me->handle;
    me->lvmt->add_devcode(me->handle, DEVTYPE_CGVI8M);
    me->lvmt->has_regs(me->handle, CGVI8_CHAN_REGS_base);

    SetChanRDs       (devid, CGVI8_CHAN_QUANT_n_base, CGVI8_CHAN_OUT_count,
                      10.0, 0.0);
    SetChanReturnType(devid, CGVI8_CHAN_HW_VER, 1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, CGVI8_CHAN_SW_VER, 1, IS_AUTOUPDATED_TRUSTED);
#if DEBUG_CGVI8M_MASK
    cgvi8_mask_hbt(devid, devptr, -1, NULL);
#endif

    return DEVSTATE_OPERATING;
}

static void cgvi8_ff (int devid, void *devptr, int is_a_reset)
{
  privrec_t *me = devptr;

    if (is_a_reset == CANKOZ_LYR_OPTION_FFPROC_BEFORE_RESET)
    {
        bzero(me->cur_codes_read, sizeof(me->cur_codes_read));
        bzero(&(me->mode), sizeof(me->mode));
    }

    me->lvmt->get_dev_ver(me->handle, &(me->hw_ver), &(me->sw_ver), NULL);
    ReturnInt32Datum(me->devid, CGVI8_CHAN_HW_VER, me->hw_ver, 0);
    ReturnInt32Datum(me->devid, CGVI8_CHAN_SW_VER, me->sw_ver, 0);
}

static void cgvi8_in (int devid, void *devptr,
                      int desc,  size_t dlc, uint8 *data)
{
  privrec_t  *me       = (privrec_t *) devptr;
  int         l;       // Line #
  int32       code;    // Code -- in device/Kozak encoding
  int32       val;     // Value -- in quants

  int32       status_b;
  int32       mask;
  int32       prescaler;
  int32       basebyte;

    switch (desc)
    {
        case DESC_READ_n_BASE ... DESC_READ_n_BASE+CGVI8_CHAN_OUT_count-1:
            if (dlc < 3) return;
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);

            l    = desc - DESC_READ_n_BASE;
            code = data[1] + (data[2] << 8);
            val  = code2quants(code, me->mode.cur_prescaler);

            me->cur_codes     [l] = code;
            me->cur_codes_read[l] = 1;
            
            ReturnInt32Datum(devid, CGVI8_CHAN_RAW_V_n_base + l, code, 0);
            ReturnInt32Datum(devid, CGVI8_CHAN_QUANT_n_base + l, val,  0);
            break;

        case CANKOZ_DESC_GETDEVSTAT:
            if (dlc < 4) return;
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);

            /* Extract data... */
            status_b  = data[1];
            mask      = data[2];
            prescaler = data[3];
#if DEBUG_CGVI8M_MASK
            if (me->mode.rd_req_sent == 0)
            {
                if (mask != me->mode.last_known_mask)
                {
                    DoDriverLog(devid, DRIVERLOG_WARNING,
                                "GETDEVSTAT: mask=%d, != last_known_mask=%d",
                                mask, me->mode.last_known_mask);
                    me->mode.last_known_mask = mask;
                }
                return;
            }
            me->mode.rd_req_sent = 0;
            me->mode.last_known_mask = mask;
#endif
            if (me->hw_ver >= 1  &&  me->sw_ver >= 5)
                basebyte  = data[4];
            else
                basebyte  = 0;
            DoDriverLog(devid, 0 | DRIVERLOG_C_PKTINFO,
                        "GETDEVSTAT: status=0x%02x mask=0x%02X prescaler=%d basebyte=%d",
                        status_b, mask, prescaler, basebyte);

            /* ...and store it */
            me->mode.cur_val       = mask;
            me->mode.cur_prescaler = prescaler;

            me->mode.rcvd = 1;

            /* Do we have a pending write request? */
            if (me->mode.pend)
            {
                send_wrmode_cmd(me);
#if DEBUG_CGVI8M_MASK
                (me->mode.rd_req_sent = 1),
#endif
                me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 1, CANKOZ_DESC_GETDEVSTAT);
            }

            /* Finally, return received data */
            ReturnInt32Datum    (devid, CGVI8_CHAN_MASK8,     mask,      0);
            ReturnInt32Datum    (devid, CGVI8_CHAN_PRESCALER, prescaler, 0);
            if (me->hw_ver >= 1  &&  me->sw_ver >= 5)
                ReturnInt32Datum(devid, CGVI8_CHAN_BASEBYTE,  basebyte,  0);
            for (l = 0;  l < CGVI8_CHAN_OUT_count;  l++)
            {
                ReturnInt32Datum(devid, CGVI8_CHAN_MASK1_n_base + l,
                                 (mask & (1 << l)) != 0, 0);
                if (me->cur_codes_read[l])
                {
                    val = code2quants(me->cur_codes[l], me->mode.cur_prescaler);
                    ReturnInt32Datum(devid,
                                     CGVI8_CHAN_QUANT_n_base + l, val, 0);
                }
            }

            break;
    }
}

static void cgvi8_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = (privrec_t *)devptr;

  int          n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int          chn;   // channel
  int          l;     // Line #
  int32        val;   // Value
  int          c_c;
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

        if      (chn >= CGVI8_CHAN_REGS_base  &&  chn <= CGVI8_CHAN_REGS_last)
        {
            me->lvmt->regs_rw(me->handle, action, chn, &val);
        }

        else if ((chn >= CGVI8_CHAN_RAW_V_n_base  &&
                  chn <= CGVI8_CHAN_RAW_V_n_base+CGVI8_CHAN_OUT_count-1)
                 ||
                 (chn >= CGVI8_CHAN_QUANT_n_base  &&
                  chn <= CGVI8_CHAN_QUANT_n_base+CGVI8_CHAN_OUT_count-1))
        {
            c_c = 0;
            if (chn < CGVI8_CHAN_QUANT_n_base)
            {
                l = chn - CGVI8_CHAN_RAW_V_n_base;
                if (action == DRVA_WRITE) c_c = val;
            }
            else
            {
                l = chn - CGVI8_CHAN_QUANT_n_base;
                if (action == DRVA_WRITE) c_c = quants2code(val, me->mode.cur_prescaler);
            }

            if (action == DRVA_WRITE)
            {
                if (c_c < 0)     c_c = 0;
                if (c_c > 65535) c_c = 65535;

                /*!!! Should use SQ_REPLACE_NOTFIRST and send READ only if NOTFOUND */
                me->lvmt->q_enq_ons_v(me->handle, SQ_ALWAYS, 3,
                                      DESC_WRITE_n_BASE + l,
                                      (c_c)      & 0xFF,
                                      (c_c >> 8) & 0xFF);
                me->lvmt->q_enq_v    (me->handle, SQ_ALWAYS, 1,
                                      DESC_READ_n_BASE  + l);
            }
            else
                me->lvmt->q_enq_v    (me->handle, SQ_IF_NONEORFIRST, 1,
                                      DESC_READ_n_BASE  + l);
        }
        else if ((chn >= CGVI8_CHAN_MASK1_n_base  &&
                  chn <= CGVI8_CHAN_MASK1_n_base+CGVI8_CHAN_OUT_count-1)
                 ||
                 chn == CGVI8_CHAN_MASK8
                 ||
                 chn == CGVI8_CHAN_PRESCALER)
        {
            /* Read? */
            if (action == DRVA_READ)
#if DEBUG_CGVI8M_MASK
                (me->mode.rd_req_sent = 1),
#endif
                me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT,
                                  1, CANKOZ_DESC_GETDEVSTAT);
            /* No, some form of write */
            else
            {
                /* Decide what to modify... */
                if      (chn == CGVI8_CHAN_MASK8)
                {
                    me->mode.req_val = val & 0xFF;
                    me->mode.req_msk =       0xFF;
                }
                else if (chn == CGVI8_CHAN_PRESCALER)
                {
                    if (val < 0)    val = 0;
                    if (val > 0x0F) val = 0x0F;
                    me->mode.req_prescaler = val;
                    me->mode.wrt_prescaler = 1;
                }
                else /* CGVI8_CHAN_MASK1_n_base...+8-1 */
                {
                    mask = 1 << (chn - CGVI8_CHAN_MASK1_n_base);
                    if (val != 0) me->mode.req_val |=  mask;
                    else          me->mode.req_val &=~ mask;
                    me->mode.req_msk |= mask;
                }

                me->mode.pend = 1;
                /* May we perform write right now? */
                if (me->mode.rcvd)
                {
                    /*!!! Should use SQ_REPLACE_NOTFIRST and send READ only if NOTFOUND */
                    send_wrmode_cmd(me);
#if DEBUG_CGVI8M_MASK
                    (me->mode.rd_req_sent = 1),
#endif
                    me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 1,
                                      CANKOZ_DESC_GETDEVSTAT);
                }
                /* No, we should request read first... */
                else
                {
#if DEBUG_CGVI8M_MASK
                    (me->mode.rd_req_sent = 1),
#endif
                    me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                                      CANKOZ_DESC_GETDEVSTAT);
                }
            }
        }
        else if (chn == CGVI8_CHAN_PROGSTART)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
            {
                me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                                      SQ_TRIES_DIR, 0,
                                      NULL, NULL,
                                      0, 1, DESC_PROGSTART);
            }
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == CGVI8_CHAN_BASEBYTE)
        {
            if (me->hw_ver >= 1  &&  me->sw_ver >= 5)
            {
                if (action == DRVA_WRITE)
                {
                    c_c = val;
                    if (c_c < 0)   c_c = 0;
                    if (c_c > 255) c_c = 255;
                    /*!!! Should use SQ_REPLACE_NOTFIRST and send READ only if NOTFOUND */
                    me->lvmt->q_enq_ons_v(me->handle, SQ_ALWAYS, 2,
                                          DESC_WRITE_BASEBYTE, c_c);
#if DEBUG_CGVI8M_MASK
                    (me->mode.rd_req_sent = 1),
#endif
                    me->lvmt->q_enq_v    (me->handle, SQ_ALWAYS, 1,
                                          CANKOZ_DESC_GETDEVSTAT);
                }
                else
#if DEBUG_CGVI8M_MASK
                    (me->mode.rd_req_sent = 1),
#endif
                    me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                                      CANKOZ_DESC_GETDEVSTAT);
            }
            else
                ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(cgvi8, "CGVI8 and GVI8M driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   cgvi8_init_d, NULL, cgvi8_rw_p);
