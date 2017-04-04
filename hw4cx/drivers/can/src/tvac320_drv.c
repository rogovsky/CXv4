#include "timeval_utils.h"

#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/tvac320_drv_i.h"


/*=== TVAC320 specifics ============================================*/

enum
{
    DEVTYPE = 36
};

enum
{
    DESC_WRT_U         = 0x07,
    DESC_GET_U         = 0x0F,
    DESC_MES_U         = 0x1F,

    DESC_WRT_U_MIN     = 0x27,
    DESC_GET_U_MIN     = 0x2F,

    DESC_WRT_U_MAX     = 0x37,
    DESC_GET_U_MAX     = 0x3F,

    DESC_ENG_TVR       = 0x47,
    DESC_OFF_TVR       = 0x4F,

    DESC_WRT_ILKS_MASK = 0xD7,
    DESC_GET_ILKS_MASK = 0xDF,
    DESC_RST_ILKS      = 0xE7,
    DESC_MES_ILKS      = 0xEF,

    DESC_REBOOT        = 0xF7,
};

//////////////////////////////////////////////////////////////////////

typedef struct
{
    cankoz_vmt_t    *lvmt;
    int              devid;
    int              handle;

    struct
    {
        int    rcvd;
        int    pend;

        uint8  cur_val;
        uint8  req_val;
        uint8  req_msk;
    }                    msks;
} privrec_t;

//////////////////////////////////////////////////////////////////////

static void send_wrmsk_cmd(privrec_t *me)
{
    me->lvmt->q_enq_ons_v(me->handle, SQ_ALWAYS, 2,
                          DESC_WRT_ILKS_MASK,
                          (me->msks.cur_val &~ me->msks.req_msk) |
                          (me->msks.req_val &  me->msks.req_msk));
    me->msks.pend    = 0;
}

//////////////////////////////////////////////////////////////////////

static void tvac320_ff (int devid, void *devptr, int is_a_reset);
static void tvac320_in (int devid, void *devptr,
                        int desc,  size_t dlc, uint8 *data);

static int  tvac320_init_d(int devid, void *devptr,
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
                               tvac320_ff, tvac320_in,
                               TVAC320_NUMCHANS*2/*!!!*/);
    if (me->handle < 0) return me->handle;

    SetChanRDs       (devid, TVAC320_CHAN_SET_U,     1, 10, 0);
    SetChanRDs       (devid, TVAC320_CHAN_SET_U_MIN, 1, 10, 0);
    SetChanRDs       (devid, TVAC320_CHAN_SET_U_MAX, 1, 10, 0);
    SetChanRDs       (devid, TVAC320_CHAN_MES_U,     1, 10, 0);
    SetChanReturnType(devid, TVAC320_CHAN_HW_VER, 1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, TVAC320_CHAN_SW_VER, 1, IS_AUTOUPDATED_TRUSTED);

    return DEVSTATE_OPERATING;
}

static void tvac320_ff (int devid, void *devptr, int is_a_reset)
{
  privrec_t *me = devptr;
  int        hw_ver;
  int        sw_ver;

    me->lvmt->get_dev_ver(me->handle, &hw_ver, &sw_ver, NULL);
    ReturnInt32Datum(me->devid, TVAC320_CHAN_HW_VER, hw_ver, 0);
    ReturnInt32Datum(me->devid, TVAC320_CHAN_SW_VER, sw_ver, 0);

    if (is_a_reset)
        bzero(&(me->msks), sizeof(me->msks));
}

static void tvac320_in (int devid, void *devptr,
                        int desc,  size_t dlc, uint8 *data)
{
  privrec_t  *me       = (privrec_t *) devptr;

  int         x;
  int32       bits_vals  [8];
  int         bits_addrs [8];
  cxdtype_t   bits_dtypes[8];
  int         bits_nelems[8];
  rflags_t    bits_rflags[8];
  void       *bits_ptrs  [8];

#define ON_RW_IN(CHAN_N, DESC_WRT, DESC_GET)                     \
    case DESC_WRT:                                               \
    case DESC_GET:                                               \
        me->lvmt->q_erase_and_send_next_v(me->handle, -1, desc); \
        ReturnInt32Datum(devid, CHAN_N,                          \
                         data[1] + ((data[2] & 0x0F) << 8), 0);  \
        break;

#define ON_RDS_IN(CHAN_N)                                    \
    me->lvmt->q_erase_and_send_next_v(me->handle, -1, desc); \
    for (x = 0;  x < 8;  x++)                                \
    {                                                        \
        bits_vals  [x] = (data[1] >> x) & 1;                 \
        bits_addrs [x] = CHAN_N + x;                         \
        bits_dtypes[x] = CXDTYPE_INT32;                      \
        bits_nelems[x] = 1;                                  \
        bits_rflags[x] = 0;                                  \
        bits_ptrs  [x] = bits_vals + x;                      \
    }                                                        \
    ReturnDataSet(devid, 8,                                  \
                  bits_addrs, bits_dtypes, bits_nelems,      \
                  bits_ptrs,  bits_rflags, NULL);            \
    break;

    switch (desc)
    {
        ON_RW_IN(TVAC320_CHAN_SET_U,     DESC_GET_U,     DESC_WRT_U);
        ON_RW_IN(TVAC320_CHAN_SET_U_MIN, DESC_GET_U_MIN, DESC_WRT_U_MIN);
        ON_RW_IN(TVAC320_CHAN_SET_U_MAX, DESC_GET_U_MAX, DESC_WRT_U_MAX);

        case DESC_MES_U:
            me->lvmt->q_erase_and_send_next_v(me->handle, -1, desc);
            ReturnInt32Datum(devid, TVAC320_CHAN_MES_U,
                             data[1] + (data[2] << 8), 0);
            break;

        case DESC_ENG_TVR:
        case DESC_OFF_TVR:
        case CANKOZ_DESC_GETDEVSTAT:
            ON_RDS_IN(TVAC320_CHAN_STAT_n_base);
            
        case DESC_MES_ILKS:
            ON_RDS_IN(TVAC320_CHAN_ILK_n_base);

        case DESC_WRT_ILKS_MASK:
        case DESC_GET_ILKS_MASK:
            /* Store */
            me->msks.cur_val = data[1];
            me->msks.rcvd    = 1;
            /* Do we have a pending write request? */
            if (me->msks.pend)
            {
                /*!!! should use if(enq(REPLACE_NOTFIRST)==NOTFOUND) enq(ALWAYS */
                send_wrmsk_cmd(me);
                me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                                  DESC_GET_ILKS_MASK);
            }
            /* And perform standard mask actions */
            ON_RDS_IN(TVAC320_CHAN_LKM_n_base);

        case DESC_RST_ILKS:
        case DESC_REBOOT:
            me->lvmt->q_erase_and_send_next_v(me->handle, -1, desc);
            break;
    }
}

static void tvac320_rw_p(int devid, void *devptr,
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

// RW -- R/W channels
#define ON_RW_RW(CHAN_N, DESC_WRT, DESC_GET)                             \
    else if (chn == CHAN_N)                                              \
    {                                                                    \
        if (action == DRVA_WRITE)                                        \
        {                                                                \
            if (val < 0)    val = 0;                                     \
            if (val > 3000) val = 3000;                                  \
            me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 3,                  \
                              DESC_WRT,                                  \
                              val & 0xFF,                                \
                              (val >> 8) & 0x0F);                        \
        }                                                                \
        else                                                             \
        {                                                                \
            me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,          \
                              DESC_GET);                                 \
        }                                                                \
    }

// CMD -- command channels
#define ON_CMD_RW(CHAN_N, DESC_CMD)                                      \
    else if (chn == CHAN_N)                                              \
    {                                                                    \
        if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)           \
        {                                                                \
            me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,          \
                              DESC_CMD);                                 \
        }                                                                \
        ReturnInt32Datum(devid, chn, 0, 0);                              \
    }

// RDS -- ReaDable Statuses (STATuses and InterLocKs)
#define ON_RDS_RW(CHAN_N, DESC_CMD)                                  \
    else if (chn >= CHAN_N  &&  chn < CHAN_N + 8)                    \
    {                                                                \
        me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,          \
                          DESC_CMD);                                 \
    }

        if      (0);
        ON_RW_RW (TVAC320_CHAN_SET_U,       DESC_WRT_U,     DESC_GET_U)
        ON_RW_RW (TVAC320_CHAN_SET_U_MIN,   DESC_WRT_U_MIN, DESC_GET_U_MIN)
        ON_RW_RW (TVAC320_CHAN_SET_U_MAX,   DESC_WRT_U_MAX, DESC_GET_U_MAX)
        ON_CMD_RW(TVAC320_CHAN_TVR_ON,      DESC_ENG_TVR)
        ON_CMD_RW(TVAC320_CHAN_TVR_OFF,     DESC_OFF_TVR)
        ON_CMD_RW(TVAC320_CHAN_RST_ILK,     DESC_RST_ILKS)
        ON_CMD_RW(TVAC320_CHAN_REBOOT,      DESC_REBOOT)
        ON_RDS_RW(TVAC320_CHAN_STAT_n_base, CANKOZ_DESC_GETDEVSTAT)
        ON_RDS_RW(TVAC320_CHAN_ILK_n_base,  DESC_MES_ILKS)
        else if (chn == TVAC320_CHAN_MES_U)
        {
            me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                              DESC_MES_U);
        }
        else if (chn >= TVAC320_CHAN_LKM_n_base  &&
                 chn <  TVAC320_CHAN_LKM_n_base + TVAC320_CHAN_LKM_n_count)
        {
            /* Read? */
            if (action == DRVA_READ)
            {
                me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                                  DESC_GET_ILKS_MASK);
            }
            /* No, write */
            else
            {
                mask = 1 << (chn - TVAC320_CHAN_LKM_n_base);
                /* Decide, what to write... */
                if (val != 0) me->msks.req_val |=  mask;
                else          me->msks.req_val &=~ mask;
                me->msks.req_msk |= mask;

                me->msks.pend = 1;
                /* May we perform write right now? */
                if (me->msks.rcvd)
                {
                    /*!!! should use if(enq(REPLACE_NOTFIRST)==NOTFOUND) enq(ALWAYS */
                    send_wrmsk_cmd(me);
                    me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                                      DESC_GET_ILKS_MASK);
                }
                /* No, we should request read first... */
                else
                {
                    me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                                      DESC_GET_ILKS_MASK);
                }
            }
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(tvac320, "TVAC320 driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   tvac320_init_d, NULL, tvac320_rw_p);
