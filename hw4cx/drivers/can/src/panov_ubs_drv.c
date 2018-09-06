#include "timeval_utils.h"

#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/panov_ubs_drv_i.h"


/*=== PANOV_UBS specifics ==========================================*/

enum
{
    DEVTYPE = 22
};

/*
 *  WRT  write
 *  GET  get
 *  ACT  activate //13.02.2011 -- obsolete
 *  MES  measure
 *  RST  reset
 *  ENG  engage (switch on)
 *  OFF  switch off
 *  CLB  calibrate
 */
enum
{
    DESC_ERROR             = 0x00,

    /* Starter1 */
    DESC_ST1_WRT_IHEAT     = 0x01,
    DESC_ST1_GET_IHEAT     = 0x09,
    DESC_ST1_ACT_IHEAT     = 0x11,
    DESC_ST1_MES_IHEAT     = 0x19,
    DESC_ST1_WRT_MIN_IHEAT = 0x21,
    DESC_ST1_GET_MIN_IHEAT = 0x29,
    DESC_ST1_WRT_MAX_IHEAT = 0x31,
    DESC_ST1_GET_MAX_IHEAT = 0x39,
    DESC_ST1_ENG_HEAT      = 0x41,
    DESC_ST1_OFF_HEAT      = 0x49,
    DESC_ST1_WRT_IARC      = 0x51,
    DESC_ST1_GET_IARC      = 0x59,
    DESC_ST1_ACT_IARC      = 0x61,
    DESC_ST1_MES_IARC      = 0x69,
    DESC_ST1_WRT_MIN_IARC  = 0x71,
    DESC_ST1_GET_MIN_IARC  = 0x79,
    DESC_ST1_WRT_MAX_IARC  = 0x81,
    DESC_ST1_GET_MAX_IARC  = 0x89,
    //
    DESC_ST1_GET_HEAT_TIME = 0x99,
    DESC_ST1_CLB_IHEAT     = 0xA1,
    DESC_ST1_GET_CST_IHEAT = 0xA9,
    DESC_ST1_CLB_IARC      = 0xB1,
    DESC_ST1_GET_CST_IARC  = 0xB9,
    //
    DESC_ST1_WRT_MASK      = 0xD1,
    DESC_ST1_GET_MASK      = 0xD9,
    DESC_ST1_RST_ILKS      = 0xE1,
    DESC_ST1_MES_ILKS      = 0xE9,
    DESC_ST1_REBOOT        = 0xF1,
    DESC_ST1_GET_STATE     = 0xF9,

    /* Starter2 */
    DESC_ST2_WRT_IHEAT     = 0x02,
    DESC_ST2_GET_IHEAT     = 0x0A,
    DESC_ST2_ACT_IHEAT     = 0x12,
    DESC_ST2_MES_IHEAT     = 0x1A,
    DESC_ST2_WRT_MIN_IHEAT = 0x22,
    DESC_ST2_GET_MIN_IHEAT = 0x2A,
    DESC_ST2_WRT_MAX_IHEAT = 0x32,
    DESC_ST2_GET_MAX_IHEAT = 0x3A,
    DESC_ST2_ENG_HEAT      = 0x42,
    DESC_ST2_OFF_HEAT      = 0x4A,
    DESC_ST2_WRT_IARC      = 0x52,
    DESC_ST2_GET_IARC      = 0x5A,
    DESC_ST2_ACT_IARC      = 0x62,
    DESC_ST2_MES_IARC      = 0x6A,
    DESC_ST2_WRT_MIN_IARC  = 0x72,
    DESC_ST2_GET_MIN_IARC  = 0x7A,
    DESC_ST2_WRT_MAX_IARC  = 0x82,
    DESC_ST2_GET_MAX_IARC  = 0x8A,
    //
    DESC_ST2_GET_HEAT_TIME = 0x9A,
    DESC_ST2_CLB_IHEAT     = 0xA2,
    DESC_ST2_GET_CST_IHEAT = 0xAA,
    DESC_ST2_CLB_IARC      = 0xB2,
    DESC_ST2_GET_CST_IARC  = 0xBA,
    //
    DESC_ST2_WRT_MASK      = 0xD2,
    DESC_ST2_GET_MASK      = 0xDA,
    DESC_ST2_RST_ILKS      = 0xE2,
    DESC_ST2_MES_ILKS      = 0xEA,
    DESC_ST2_REBOOT        = 0xF2,
    DESC_ST2_GET_STATE     = 0xFA,

    /* Degausser */
    DESC_DGS_WRT_UDGS      = 0x03,
    DESC_DGS_GET_UDGS      = 0x0B,
    DESC_DGS_ACT_UDGS      = 0x13,
    DESC_DGS_MES_UDGS      = 0x1B,
    DESC_DGS_WRT_MIN_UDGS  = 0x23,
    DESC_DGS_GET_MIN_UDGS  = 0x2B,
    DESC_DGS_WRT_MAX_UDGS  = 0x33,
    DESC_DGS_GET_MAX_UDGS  = 0x3B,
    DESC_DGS_ENG_UDGS      = 0x43,
    DESC_DGS_OFF_UDGS      = 0x4B,
    //
    DESC_DGS_CLB_UDGS      = 0xA3,
    DESC_DGS_GET_CST_UDGS  = 0xAB,
    //
    DESC_DGS_WRT_MASK      = 0xD3,
    DESC_DGS_GET_MASK      = 0xDB,
    DESC_DGS_RST_ILKS      = 0xE3,
    DESC_DGS_MES_ILKS      = 0xEB,
    DESC_DGS_REBOOT        = 0xF3,
    DESC_DGS_GET_STATE     = 0xFB,

    /* "All" -- commands for all 3 devices */
    DESC_ALL_WRT_PARAMS    = 0x06,
    DESC_ALL_GET_PARAMS    = 0x0E,
    DESC_ALL_ACT_PARAMS    = 0x16,
    DESC_ALL_MES_PARAMS    = 0x1E,
    DESC_ALL_WRT_MIN       = 0x26,
    DESC_ALL_GET_MIN       = 0x2E,
    DESC_ALL_WRT_MAX       = 0x36,
    DESC_ALL_GET_MAX       = 0x3E,
    DESC_ALL_GET_HEAT_TIME = 0x9E,
    DESC_ALL_GET_CST_PARAMS = 0xAE,
    DESC_ALL_WRT_ILKS_MASK = 0xD6,
    DESC_ALL_GET_ILKS_MASK = 0xDE,
    DESC_ALL_RST_ILKS      = 0xE6,
    DESC_ALL_MES_ILKS      = 0xEE,
    DESC_ALL_REBOOT        = 0xF6,
    DESC_ALL_GET_STATE     = 0xFE, // =CANKOZ_DESC_GETDEVSTAT

    /* UBS */
    DESC_UBS_WRT_THT_TIME  = 0x07,
    DESC_UBS_GET_THT_TIME  = 0x0F,
    DESC_UBS_WRT_MODE      = 0x17,
    DESC_UBS_GET_MODE      = 0x1F,
    DESC_UBS_REREAD_HWADDR = 0x27,
    DESC_UBS_GET_HWADDR    = 0x2F,
    //
    DESC_UBS_POWER_ON      = 0x47,
    DESC_UBS_POWER_OFF     = 0x4F,
    DESC_UBS_WRT_ARC_TIME  = 0x57,
    DESC_UBS_GET_ARC_TIME  = 0x5F,
    //
    DESC_UBS_SAVE_SETTINGS = 0xA7,
    //
    DESC_UBS_WRT_ILKS_MASK = 0xD7,
    DESC_UBS_GET_ILKS_MASK = 0xDF,
    DESC_UBS_RST_ILKS      = 0xE7,
    DESC_UBS_MES_ILKS      = 0xEF,
    DESC_UBS_REBOOT        = 0xF7,
};

enum
{
    ERR_NO       = 0,
    ERR_LENGTH   = 1,
    ERR_WRCMD    = 2,
    ERR_UNKCMD   = 3,
    ERR_CRASH    = 4,
    ERR_CHECKSUM = 5,
    ERR_SEQ      = 6,
    ERR_TIMEOUT  = 7,
    ERR_INTBUFF  = 8,
    ERR_ALLOCMEM = 9,
    ERR_OFFLINE  = 10,
    ERR_TSKRUN   = 11,
    ERR_BLOCK    = 12,
};

enum
{
    PERSISTENT_STAT_MASK =
        (1 << (PANOV_UBS_CHAN_STAT_ST1_REBOOT - PANOV_UBS_CHAN_STAT_n_base)) |
        (1 << (PANOV_UBS_CHAN_STAT_ST2_REBOOT - PANOV_UBS_CHAN_STAT_n_base)) |
        (1 << (PANOV_UBS_CHAN_STAT_DGS_REBOOT - PANOV_UBS_CHAN_STAT_n_base)) |
        (1 << (PANOV_UBS_CHAN_STAT_UBS_REBOOT - PANOV_UBS_CHAN_STAT_n_base))
};


typedef struct
{
    int8   nb;      // Number of data Bytes
    uint8  rd_desc; // Read
    uint8  wr_desc; // Write
    uint8  after_w; // Additional command to send AFTER write
} chan2desc_t;

static chan2desc_t chan2desc_map[] =
{
    //
    [PANOV_UBS_CHAN_SET_ST1_IHEAT] = {1, DESC_ST1_GET_IHEAT,     DESC_ST1_WRT_IHEAT,     0},
    ////[PANOV_UBS_CHAN_SET_ST1_IARC]  = {1, DESC_ST1_GET_IARC,      DESC_ST1_WRT_IARC,      0},
    [PANOV_UBS_CHAN_SET_ST2_IHEAT] = {1, DESC_ST2_GET_IHEAT,     DESC_ST2_WRT_IHEAT,     0},
    ////[PANOV_UBS_CHAN_SET_ST2_IARC]  = {1, DESC_ST2_GET_IARC,      DESC_ST2_WRT_IARC,      0},
    [PANOV_UBS_CHAN_SET_DGS_UDGS]  = {1, DESC_DGS_GET_UDGS,      DESC_DGS_WRT_UDGS,      0},
    [PANOV_UBS_CHAN_SET_THT_TIME]  = {1, DESC_UBS_GET_THT_TIME,  DESC_UBS_WRT_THT_TIME,  0},
    ////[PANOV_UBS_CHAN_SET_ARC_TIME]  = {1, DESC_UBS_GET_ARC_TIME,  DESC_UBS_WRT_ARC_TIME,  0},
    [PANOV_UBS_CHAN_SET_MODE]      = {1, DESC_UBS_GET_MODE,      DESC_UBS_WRT_MODE,      0},
    //
    [PANOV_UBS_CHAN_CLB_ST1_IHEAT] = {0, 0,                      DESC_ST1_CLB_IHEAT,     0},
    ////[PANOV_UBS_CHAN_CLB_ST1_IARC]  = {0, 0,                      DESC_ST1_CLB_IARC,      0},
    [PANOV_UBS_CHAN_CLB_ST2_IHEAT] = {0, 0,                      DESC_ST2_CLB_IHEAT,     0},
    ////[PANOV_UBS_CHAN_CLB_ST2_IARC]  = {0, 0,                      DESC_ST2_CLB_IARC,      0},
    [PANOV_UBS_CHAN_CLB_DGS_UDGS]  = {0, 0,                      DESC_DGS_CLB_UDGS,      0},
    //
    [PANOV_UBS_CHAN_MIN_ST1_IHEAT] = {1, DESC_ST1_GET_MIN_IHEAT, DESC_ST1_WRT_MIN_IHEAT, 0},
    ////[PANOV_UBS_CHAN_MIN_ST1_IARC]  = {1, DESC_ST1_GET_MIN_IARC,  DESC_ST1_WRT_MIN_IARC,  0},
    [PANOV_UBS_CHAN_MIN_ST2_IHEAT] = {1, DESC_ST2_GET_MIN_IHEAT, DESC_ST2_WRT_MIN_IHEAT, 0},
    ////[PANOV_UBS_CHAN_MIN_ST2_IARC]  = {1, DESC_ST2_GET_MIN_IARC,  DESC_ST2_WRT_MIN_IARC,  0},
    [PANOV_UBS_CHAN_MIN_DGS_UDGS]  = {1, DESC_DGS_GET_MIN_UDGS,  DESC_DGS_WRT_MIN_UDGS,  0},
    [PANOV_UBS_CHAN_MAX_ST1_IHEAT] = {1, DESC_ST1_GET_MAX_IHEAT, DESC_ST1_WRT_MAX_IHEAT, 0},
    ////[PANOV_UBS_CHAN_MAX_ST1_IARC]  = {1, DESC_ST1_GET_MAX_IARC,  DESC_ST1_WRT_MAX_IARC,  0},
    [PANOV_UBS_CHAN_MAX_ST2_IHEAT] = {1, DESC_ST2_GET_MAX_IHEAT, DESC_ST2_WRT_MAX_IHEAT, 0},
    ////[PANOV_UBS_CHAN_MAX_ST2_IARC]  = {1, DESC_ST2_GET_MAX_IARC,  DESC_ST2_WRT_MAX_IARC,  0},
    [PANOV_UBS_CHAN_MAX_DGS_UDGS]  = {1, DESC_DGS_GET_MAX_UDGS,  DESC_DGS_WRT_MAX_UDGS,  0},
    //
    [PANOV_UBS_CHAN_MES_ST1_IHEAT] = {1, DESC_ST1_MES_IHEAT,     0,                      0},
    [PANOV_UBS_CHAN_MES_ST1_IARC]  = {1, DESC_ST1_MES_IARC,      0,                      0},
    [PANOV_UBS_CHAN_MES_ST2_IHEAT] = {1, DESC_ST2_MES_IHEAT,     0,                      0},
    [PANOV_UBS_CHAN_MES_ST2_IARC]  = {1, DESC_ST2_MES_IARC,      0,                      0},
    [PANOV_UBS_CHAN_MES_DGS_UDGS]  = {1, DESC_DGS_MES_UDGS,      0,                      0},
    [PANOV_UBS_CHAN_CST_ST1_IHEAT] = {1, DESC_ST1_GET_CST_IHEAT, 0,                      0},
    ////[PANOV_UBS_CHAN_CST_ST1_IARC]  = {1, DESC_ST1_GET_CST_IARC,  0,                      0},
    [PANOV_UBS_CHAN_CST_ST2_IHEAT] = {1, DESC_ST2_GET_CST_IHEAT, 0,                      0},
    ////[PANOV_UBS_CHAN_CST_ST2_IARC]  = {1, DESC_ST2_GET_CST_IARC,  0,                      0},
    [PANOV_UBS_CHAN_CST_DGS_UDGS]  = {1, DESC_DGS_GET_CST_UDGS,  0,                      0},
    //
    [PANOV_UBS_CHAN_POWER_ON]      = {0, 0,                      DESC_UBS_POWER_ON,      CANKOZ_DESC_GETDEVSTAT},
    [PANOV_UBS_CHAN_POWER_OFF]     = {0, 0,                      DESC_UBS_POWER_OFF,     CANKOZ_DESC_GETDEVSTAT},
    [PANOV_UBS_CHAN_ST1_HEAT_ON]   = {0, 0,                      DESC_ST1_ENG_HEAT,      CANKOZ_DESC_GETDEVSTAT},
    [PANOV_UBS_CHAN_ST1_HEAT_OFF]  = {0, 0,                      DESC_ST1_OFF_HEAT,      CANKOZ_DESC_GETDEVSTAT},
    [PANOV_UBS_CHAN_ST2_HEAT_ON]   = {0, 0,                      DESC_ST2_ENG_HEAT,      CANKOZ_DESC_GETDEVSTAT},
    [PANOV_UBS_CHAN_ST2_HEAT_OFF]  = {0, 0,                      DESC_ST2_OFF_HEAT,      CANKOZ_DESC_GETDEVSTAT},
    [PANOV_UBS_CHAN_DGS_UDGS_ON]   = {0, 0,                      DESC_DGS_ENG_UDGS,      CANKOZ_DESC_GETDEVSTAT},
    [PANOV_UBS_CHAN_DGS_UDGS_OFF]  = {0, 0,                      DESC_DGS_OFF_UDGS,      CANKOZ_DESC_GETDEVSTAT},
    //
    [PANOV_UBS_CHAN_REBOOT_ALL]    = {0, 0,                      DESC_ALL_REBOOT,        0},
    [PANOV_UBS_CHAN_REBOOT_ST1]    = {0, 0,                      DESC_ST1_REBOOT,        0},
    [PANOV_UBS_CHAN_REBOOT_ST2]    = {0, 0,                      DESC_ST2_REBOOT,        0},
    [PANOV_UBS_CHAN_REBOOT_DGS]    = {0, 0,                      DESC_DGS_REBOOT,        0},
    [PANOV_UBS_CHAN_REBOOT_UBS]    = {0, 0,                      DESC_UBS_REBOOT,        0},
    [PANOV_UBS_CHAN_REREAD_HWADDR] = {0, 0,                      DESC_UBS_REREAD_HWADDR, 0},
    //
    [PANOV_UBS_CHAN_SAVE_SETTINGS] = {0, 0,                      DESC_UBS_SAVE_SETTINGS, 0},
    //
    [PANOV_UBS_CHAN_RST_ALL_ILK]   = {0, 0,                      DESC_ALL_RST_ILKS,      DESC_ALL_MES_ILKS},
    [PANOV_UBS_CHAN_RST_ST1_ILK]   = {0, 0,                      DESC_ST1_RST_ILKS,      DESC_ALL_MES_ILKS},
    [PANOV_UBS_CHAN_RST_ST2_ILK]   = {0, 0,                      DESC_ST2_RST_ILKS,      DESC_ALL_MES_ILKS},
    [PANOV_UBS_CHAN_RST_DGS_ILK]   = {0, 0,                      DESC_DGS_RST_ILKS,      DESC_ALL_MES_ILKS},
    [PANOV_UBS_CHAN_RST_UBS_ILK]   = {0, 0,                      DESC_UBS_RST_ILKS,      DESC_ALL_MES_ILKS},
    //
    [PANOV_UBS_CHAN_HWADDR]        = {1, DESC_UBS_GET_HWADDR,    0,                      0},
};

static int desc2chan_map[256];

static int panov_ubs_init_mod(void)
{
  int          x;
  int          chan;
  chan2desc_t *cdp;

    /* Initialize all with -1's first (since chan MAY be ==0) */
    for (x = 0;  x < countof(desc2chan_map);  x++) desc2chan_map[x] = -1;
  
    /* And than mark used cells with their client-channel numbers */
    for (chan = 0, cdp = chan2desc_map;
         chan < countof(chan2desc_map);
         chan++,   cdp++)
    {
        if (cdp->rd_desc != 0)
            desc2chan_map[cdp->rd_desc] = chan;
        if (cdp->wr_desc != 0)
            desc2chan_map[cdp->wr_desc] = chan;
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////

typedef struct
{
    cankoz_vmt_t    *lvmt;
    int              devid;
    int              handle;

    int32                hwaddr;

    uint32               acc_stat;

    uint32               ff_poweron_count;
    uint32               ff_watchdog_count;

    struct
    {
        int     rcvd;
        int     pend;

        uint8   cur_vals[8];
        uint8   req_vals[8];
        uint8   req_msks[8];
    }                    msks;
} privrec_t;

//////////////////////////////////////////////////////////////////////

static void send_wrmsks_cmd(privrec_t *me)
{
  uint8  data[8];
  uint8  vals[7];
  int    x;
  
    for (x = 0;  x <= 7;  x++)
        vals[x] = (me->msks.cur_vals[x] &~ me->msks.req_msks[x]) |
                  (me->msks.req_vals[x] &  me->msks.req_msks[x]);

    me->lvmt->q_enq_ons_v(me->handle, SQ_ALWAYS, 8,
                          DESC_ALL_WRT_ILKS_MASK,
                          vals[0],
                          vals[1],
                          vals[2],
                          vals[3],
                          vals[4],
                          vals[5],
                          vals[6]);

    me->msks.pend = 0;
}

//////////////////////////////////////////////////////////////////////

static void panov_ubs_ff (int devid, void *devptr, int is_a_reset);
static void panov_ubs_in (int devid, void *devptr,
                          int desc,  size_t dlc, uint8 *data);

static struct {int chan; double r;} rd_info[] =
{
    {PANOV_UBS_CHAN_SET_ST1_IHEAT, 255./2.91},
    {PANOV_UBS_CHAN_SET_ST2_IHEAT, 255./2.91},
    {PANOV_UBS_CHAN_SET_DGS_UDGS,  255./1200},
    {PANOV_UBS_CHAN_MES_ST1_IHEAT, 1.0/(2.91/255)},
    {PANOV_UBS_CHAN_MES_ST1_IARC,  255./32},
    {PANOV_UBS_CHAN_MES_ST2_IHEAT, 1.0/(2.91/255)},
    {PANOV_UBS_CHAN_MES_ST2_IARC,  255./32},
    {PANOV_UBS_CHAN_MES_DGS_UDGS,  255./1200},

    {PANOV_UBS_CHAN_MIN_ST1_IHEAT, 1.0/(2.91/255)},
    {PANOV_UBS_CHAN_MIN_ST2_IHEAT, 1.0/(2.91/255)},
    {PANOV_UBS_CHAN_MIN_DGS_UDGS , 255./1200},
    {PANOV_UBS_CHAN_MAX_ST1_IHEAT, 1.0/(2.91/255)},
    {PANOV_UBS_CHAN_MAX_ST2_IHEAT, 1.0/(2.91/255)},
    {PANOV_UBS_CHAN_MAX_DGS_UDGS , 255./1200},

    {PANOV_UBS_CHAN_CST_ST1_IHEAT, 255./100},
    {PANOV_UBS_CHAN_CST_ST2_IHEAT, 255./100},
    {PANOV_UBS_CHAN_CST_DGS_UDGS,  255./100},
};
static int  panov_ubs_init_d(int devid, void *devptr,
                             int businfocount, int businfo[],
                             const char *auxinfo)
{
  privrec_t *me = devptr;
  int        x;

    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "%s([%d]:\"%s\")",
                __FUNCTION__, businfocount, auxinfo);

    /* Initialize interface */
    me->lvmt   = GetLayerVMT(devid);
    me->devid  = devid;
    me->handle = me->lvmt->add(devid, devptr,
                               businfocount, businfo,
                               DEVTYPE,
                               panov_ubs_ff, panov_ubs_in,
                               PANOV_UBS_NUMCHANS*2/*!!!*/,
                               CANKOZ_LYR_OPTION_FFPROC_BEFORE_RESET);
    if (me->handle < 0) return me->handle;

    for (x = 0;  x < countof(rd_info);  x++)
        SetChanRDs(devid, rd_info[x].chan, 1, rd_info[x].r, 0);

    return DEVSTATE_OPERATING;
}

static void panov_ubs_ff (int devid, void *devptr, int is_a_reset)
{
  privrec_t *me = devptr;

    if (is_a_reset)
    {
        me->hwaddr         = -1;
        bzero(&(me->msks), sizeof(me->msks));
    }
}

static void panov_ubs_in (int devid, void *devptr,
                          int desc,  size_t dlc, uint8 *data)
{
  privrec_t  *me       = (privrec_t *) devptr;
  int         chan;
  int         err_desc;

  int         x;

  uint32      bits;
  enum       {bits_COUNT = (PANOV_UBS_CHAN_STAT_n_count > PANOV_UBS_CHAN_ILK_n_count?
                            PANOV_UBS_CHAN_STAT_n_count : PANOV_UBS_CHAN_ILK_n_count)};
  int32       bits_vals  [bits_COUNT];
  int         bits_addrs [bits_COUNT];
  cxdtype_t   bits_dtypes[bits_COUNT];
  int         bits_nelems[bits_COUNT];
  rflags_t    bits_rflags[bits_COUNT];
  void       *bits_ptrs  [bits_COUNT];

  int32       msks_vals  [PANOV_UBS_CHAN_LKM_n_count];
  int         msks_addrs [PANOV_UBS_CHAN_LKM_n_count];
  cxdtype_t   msks_dtypes[PANOV_UBS_CHAN_LKM_n_count];
  int         msks_nelems[PANOV_UBS_CHAN_LKM_n_count];
  rflags_t    msks_rflags[PANOV_UBS_CHAN_LKM_n_count];
  void       *msks_ptrs  [PANOV_UBS_CHAN_LKM_n_count];

    if (desc == DESC_UBS_GET_HWADDR)
    {
        me->lvmt->q_erase_and_send_next_v(me->handle, -1, desc);
        if (dlc < 2) return;
        me->hwaddr = data[1];
        ReturnInt32Datum(devid, PANOV_UBS_CHAN_HWADDR, me->hwaddr, 0);
        return;
    }
    if (desc == DESC_ALL_MES_PARAMS)
    {
        me->lvmt->q_erase_and_send_next_v(me->handle, -1, desc);
        if (dlc < 6) return;
        ReturnInt32Datum(devid, PANOV_UBS_CHAN_MES_ST1_IHEAT, data[1], 0);
        ReturnInt32Datum(devid, PANOV_UBS_CHAN_MES_ST1_IARC,  data[2], 0);
        ReturnInt32Datum(devid, PANOV_UBS_CHAN_MES_ST2_IHEAT, data[3], 0);
        ReturnInt32Datum(devid, PANOV_UBS_CHAN_MES_ST2_IARC,  data[4], 0);
        ReturnInt32Datum(devid, PANOV_UBS_CHAN_MES_DGS_UDGS,  data[5], 0);
        return;
    }
    if (desc > 0  &&  desc < countof(desc2chan_map)  &&
        (chan = desc2chan_map[desc]) >= 0)
    {
        me->lvmt->q_erase_and_send_next_v(me->handle, -1, desc);

        if (
            chan >= 0  &&  chan < countof(chan2desc_map)  &&  // Guard: channel IS present in table
            desc == chan2desc_map[chan].rd_desc           &&  // It is a READ command
            chan2desc_map[chan].nb != 0                       // It is a DATA, not a COMMAND channel
           )
        {
            if (dlc < chan2desc_map[chan].nb + 1) return;
            ReturnInt32Datum(devid, chan, data[1], 0);            
        }
        return;
    }
    switch (desc)
    {
        case CANKOZ_DESC_GETDEVATTRS:
            if (dlc > 4)
            {
                if      (data[4] == CANKOZ_IAMR_POWERON)
                {
                    me->ff_poweron_count++;
                    ReturnInt32Datum(devid, PANOV_UBS_CHAN_POWERON_CTR,
                                     me->ff_poweron_count, 0);
                }
                else if (data[4] == CANKOZ_IAMR_WATCHDOG)
                {
                    me->ff_watchdog_count++;
                    ReturnInt32Datum(devid, PANOV_UBS_CHAN_WATCHDOG_CTR,
                                     me->ff_watchdog_count, 0);
                }
            }
            break;

        case CANKOZ_DESC_GETDEVSTAT:
            me->lvmt->q_erase_and_send_next_v(me->handle, -1, desc);
            if (dlc < 6) return;

            bits = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
            me->acc_stat |= (bits & PERSISTENT_STAT_MASK);
            bits |= me->acc_stat;

            for (x = 0;  x < PANOV_UBS_CHAN_STAT_n_count;  x++)
            {
                bits_vals  [x] = (bits >> x) & 1;
                bits_addrs [x] = PANOV_UBS_CHAN_STAT_n_base + x;
                bits_dtypes[x] = CXDTYPE_INT32;
                bits_nelems[x] = 1;
                bits_rflags[x] = 0;
                bits_ptrs  [x] = bits_vals + x;
            }

            ReturnDataSet(devid, PANOV_UBS_CHAN_STAT_n_count,
                          bits_addrs, bits_dtypes, bits_nelems,
                          bits_ptrs,  bits_rflags, NULL);
            ReturnInt32Datum(devid, PANOV_UBS_CHAN_SET_MODE, data[5], 0);
            break;

        case DESC_ALL_MES_ILKS:
            me->lvmt->q_erase_and_send_next_v(me->handle, -1, desc);
            if (dlc < 5) return;

            bits = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);

            for (x = 0;  x < PANOV_UBS_CHAN_ILK_n_count;  x++)
            {
                bits_vals  [x] = (bits >> x) & 1;
                bits_addrs [x] = PANOV_UBS_CHAN_ILK_n_base + x;
                bits_dtypes[x] = CXDTYPE_INT32;
                bits_nelems[x] = 1;
                bits_rflags[x] = 0;
                bits_ptrs  [x] = bits_vals + x;
            }
            ReturnDataSet(devid, PANOV_UBS_CHAN_ILK_n_count,
                          bits_addrs, bits_dtypes, bits_nelems,
                          bits_ptrs,  bits_rflags, NULL);
            break;

        case DESC_ALL_GET_ILKS_MASK:
            me->lvmt->q_erase_and_send_next_v(me->handle, -1, desc);
            if (dlc < 5) return;

            /* Move data to storage... */
            me->msks.cur_vals[0] = data[1];
            me->msks.cur_vals[1] = data[2];
            me->msks.cur_vals[2] = data[3];
            me->msks.cur_vals[3] = data[4];
            me->msks.cur_vals[4] = data[5];
            me->msks.cur_vals[5] = data[6];
            me->msks.cur_vals[6] = data[7];
            me->msks.cur_vals[7] = 0;
            me->msks.rcvd = 1;

            /* Do we have a pending write request? */
            if (me->msks.pend != 0)
            {
                /*!!! should use if(enq(REPLACE_NOTFIRST)==NOTFOUND) enq(ALWAYS */
                send_wrmsks_cmd(me);
                
                me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                                  DESC_ALL_GET_ILKS_MASK);
            }

            /* And return requested data */
            for (x = 0;  x < PANOV_UBS_CHAN_LKM_n_count;  x++)
            {
                msks_vals[x] = (me->msks.cur_vals[x / 8] & (1 << (x & 7))) != 0;
                msks_addrs [x] = PANOV_UBS_CHAN_LKM_n_base + x;
                msks_dtypes[x] = CXDTYPE_INT32;
                msks_nelems[x] = 1;
                msks_rflags[x] = 0;
                msks_ptrs  [x] = msks_vals + x;
            }
            ReturnDataSet(devid, PANOV_UBS_CHAN_LKM_n_count,
                          msks_addrs, msks_dtypes, msks_nelems,
                          msks_ptrs,  msks_rflags, NULL);
            break;

        case DESC_ERROR:
            if (dlc < 3)
            {
                DoDriverLog(devid, 0,
                            "DESC_ERROR: dlc=%d, <3 -- terminating", 
                            (int)dlc);
                SetDevState(devid, DEVSTATE_OFFLINE, CXRF_REM_C_PROBL, "DESC_ERROR dlc<3");
                return;
            }
            if (data[2] == ERR_LENGTH)
            {
                DoDriverLog(devid, 0,
                            "DESC_ERROR: ERR_LENGTH of cmd=0x%02x", 
                            data[1]);
                if (data[1] != 0) SetDevState(devid, DEVSTATE_OFFLINE, CXRF_REM_C_PROBL, "ERR_LENGTH");
                return;
            }
            if (data[2] == ERR_WRCMD)
            {
                me->lvmt->q_erase_and_send_next_v(me->handle, -1, data[1]);
                return;
            }
            if (data[2] == ERR_UNKCMD)
            {
                err_desc = data[1];
                if (err_desc > 0  &&  err_desc < countof(desc2chan_map)  &&
                    (chan = desc2chan_map[err_desc]) >= 0)
                {
                    ReturnInt32Datum(devid, chan, 0, CXRF_UNSUPPORTED);
                }
                else
                {
                    DoDriverLog(devid, 0,
                                "DESC_ERROR: ERR_UNKCMD cmd=0x%02x", 
                                data[1]);
                    SetDevState(devid, DEVSTATE_OFFLINE, CXRF_REM_C_PROBL, "ERR_UNKCMD");
                }
                return;
            }
            break;
    }
}

static void panov_ubs_rw_p(int devid, void *devptr,
                           int action,
                           int count, int *addrs,
                           cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = (privrec_t *)devptr;

  int          n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int          chn;   // channel
  int32        val;   // Value

  sq_status_t  sr;

  chan2desc_t *cdp;

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

        cdp = chan2desc_map + chn;
        if      (chn == PANOV_UBS_CHAN_HWADDR)
        {
            if (me->hwaddr >= 0)
                ReturnInt32Datum(devid, chn, me->hwaddr, 0);
            else
                me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                                  DESC_UBS_GET_HWADDR);
        }
        else if (chn == PANOV_UBS_CHAN_MES_ST1_IHEAT  ||
                 chn == PANOV_UBS_CHAN_MES_ST1_IARC   ||
                 chn == PANOV_UBS_CHAN_MES_ST2_IHEAT  ||
                 chn == PANOV_UBS_CHAN_MES_ST2_IARC   ||
                 chn == PANOV_UBS_CHAN_MES_DGS_UDGS)
        {
            me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                              DESC_ALL_MES_PARAMS);
        }
        else if (chn == PANOV_UBS_CHAN_REBOOT_UBS  &&
                 action == DRVA_WRITE              &&
                 val == CX_VALUE_COMMAND)
        {
            me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                                  SQ_TRIES_DIR, 0,
                                  NULL, NULL,
                                  0, 1, DESC_UBS_REBOOT);
            ReturnInt32Datum(devid, chn, 0, 0);            
        }
        else if ((chn >= 0  &&  chn < countof(chan2desc_map))  &&
                 (cdp->rd_desc != 0  ||  cdp->wr_desc != 0))
        {
            /* A command channel? */
            if (cdp->nb == 0)
            {
                if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                {
                    sr = me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                                           cdp->wr_desc);
                    
                    if (sr == SQ_NOTFOUND  &&  cdp->after_w != 0)
                        me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 1,
                                          cdp->after_w);
                }
                ReturnInt32Datum(devid, chn, 0, 0);
            }
            /* No, a regular data one */
            else
            {
                if (action == DRVA_WRITE  &&  cdp->wr_desc != 0)
                {
                    if (val < 0)   val = 0;
                    if (val > 255) val = 255;
                    
                    /* Note: 'nb' is always 1 now, thus we just use "2"
                             instead of v2's "nb+1".
                             If someday a channel with nb>1 appears,
                             this will be changed to q_enq() with
                             variable-byte-length filling of data[]. */
                    me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 2,
                                      cdp->wr_desc, val);
                    /*!!! should use if(enq(REPLACE_NOTFIRST)==NOTFOUND) enq(ALWAYS */
                }

                me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                                  cdp->rd_desc);
            }
        }
        else if (chn >= PANOV_UBS_CHAN_STAT_n_base  &&
                 chn <  PANOV_UBS_CHAN_STAT_n_base + PANOV_UBS_CHAN_STAT_n_count)
        {
            me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                              CANKOZ_DESC_GETDEVSTAT);
        }
        else if (chn >= PANOV_UBS_CHAN_ILK_n_base  &&
                 chn <  PANOV_UBS_CHAN_ILK_n_base + PANOV_UBS_CHAN_ILK_n_count)
        {
            me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                              DESC_ALL_MES_ILKS);
        }
        /* Note: LKMs include CRMs (2nd 32 of 64) */
        else if (chn >= PANOV_UBS_CHAN_LKM_n_base  &&
                 chn <  PANOV_UBS_CHAN_LKM_n_base + PANOV_UBS_CHAN_LKM_n_count)
        {
          int  offs = (chn - PANOV_UBS_CHAN_LKM_n_base) / 8;
          int  mask = 1 << ((chn - PANOV_UBS_CHAN_LKM_n_base) & 7);

            if (action == DRVA_READ)
            {
                me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                                  DESC_ALL_GET_ILKS_MASK);
            }
            /* No, write */
            else
            {
                /* Decide, what to write... */
                if (val != 0) me->msks.req_vals[offs] |=  mask;
                else          me->msks.req_vals[offs] &=~ mask;
                me->msks.req_msks[offs] |= mask;
                
                me->msks.pend = 1;
                /* May we perform write right now? */
                if (me->msks.rcvd)
                {
                    /*!!! should use if(enq(REPLACE_NOTFIRST)==NOTFOUND) enq(ALWAYS */
                    send_wrmsks_cmd(me);
                    me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                                      DESC_ALL_GET_ILKS_MASK);
                }
                /* No, we should request read first... */
                else
                {
                    me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 1,
                                      DESC_ALL_GET_ILKS_MASK);
                }
            }
        }
        else if (chn == PANOV_UBS_CHAN_RST_ACCSTAT)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                me->acc_stat = 0;
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == PANOV_UBS_CHAN_POWERON_CTR)
        {
            if (action == DRVA_WRITE) me->ff_poweron_count = val;
            ReturnInt32Datum(devid, chn, me->ff_poweron_count, 0);
        }
        else if (chn == PANOV_UBS_CHAN_WATCHDOG_CTR)
        {
            if (action == DRVA_WRITE) me->ff_watchdog_count = val;
            ReturnInt32Datum(devid, chn, me->ff_watchdog_count, 0);
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(panov_ubs, "Panov's UBS driver",
                   panov_ubs_init_mod, NULL,
                   sizeof(privrec_t), NULL,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   panov_ubs_init_d, NULL, panov_ubs_rw_p);
