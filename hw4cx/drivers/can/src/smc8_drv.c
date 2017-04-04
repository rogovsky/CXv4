#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/smc8_drv_i.h"


/*=== SMC8    specifics ============================================*/

enum
{
    DEVTYPE = 31
};


enum
{
    DESC_ERROR      = 0x00,
    DESC_START_STOP = 0x70,
    DESC_WR_PARAM   = 0x71,
    DESC_RD_PARAM   = 0xF1,
};

enum
{
    PARAM_CSTATUS_REG = 0,
    PARAM_CONTROL_REG = 1,
    PARAM_START_FREQ  = 2,
    PARAM_FINAL_FREQ  = 3,
    PARAM_ACCEL       = 4,
    PARAM_NUM_STEPS   = 5,
    PARAM_POS_CNT     = 6,
};

enum
{
    ERR_NO        = 0,
    ERR_UNKCMD    = 1,
    ERR_LENGTH    = 2,
    ERR_INV_VAL   = 3,
    ERR_INV_STATE = 4,
    ERR_INV_PARAM = 9,
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
    }                ctl[SMC8_NUMLINES];
} privrec_t;

//////////////////////////////////////////////////////////////////////

static void smc8_ff (int devid, void *devptr, int is_a_reset);
static void smc8_in (int devid, void *devptr,
                     int desc, size_t dlc, uint8 *data);

static int smc8_init_d(int devid, void *devptr, 
                       int businfocount, int businfo[],
                       const char *auxinfo)
{
  privrec_t *me      = (privrec_t *) devptr;
    
    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "ENTRY %s(%s)", __FUNCTION__, auxinfo);

    /* Initialize interface */
    me->lvmt   = GetLayerVMT(devid);
    me->devid  = devid;
    me->handle = me->lvmt->add(devid, devptr,
                               businfocount, businfo,
                               DEVTYPE,
                               smc8_ff, smc8_in,
                               SMC8_NUMCHANS*2/*!!!*/);
    if (me->handle < 0) return me->handle;

    SetChanReturnType(devid, SMC8_CHAN_HW_VER, 1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, SMC8_CHAN_SW_VER, 1, IS_AUTOUPDATED_TRUSTED);

    return DEVSTATE_OPERATING;
}

static void smc8_ff (int   devid    __attribute__((unused)),
                     void *devptr,
                     int   is_a_reset)
{
  privrec_t  *me       = (privrec_t *) devptr;
  int         hw_ver;
  int         sw_ver;

    me->lvmt->get_dev_ver(me->handle, &hw_ver, &sw_ver, NULL);
    ReturnInt32Datum(me->devid, SMC8_CHAN_HW_VER, hw_ver, 0);
    ReturnInt32Datum(me->devid, SMC8_CHAN_SW_VER, sw_ver, 0);

    if (is_a_reset)
    {
        bzero(&(me->ctl), sizeof(me->ctl));
    }
}

static void send_wrctl_cmd(privrec_t *me, int l)
{
    me->lvmt->q_enqueue_v(me->handle, SQ_REPLACE_NOTFIRST,
                         SQ_TRIES_ONS, 0,
                         NULL, NULL,
                         3, 4,
                         DESC_WR_PARAM, l, PARAM_CONTROL_REG,
                         (me->ctl[l].cur_val &~ me->ctl[l].req_msk) |
                         (me->ctl[l].req_val &  me->ctl[l].req_msk));
    me->ctl[l].pend = 0;
}

static void ReturnCStatus(int devid, privrec_t *me, int l, uint8 b)
{
    ReturnInt32Datum(devid, SMC8_CHAN_KM_base    + l, (b >> 1) & 1,  0);
    ReturnInt32Datum(devid, SMC8_CHAN_KP_base    + l, (b >> 2) & 1,  0);
    ReturnInt32Datum(devid, SMC8_CHAN_STATE_base + l, (b >> 4) & 15, 0);
    ReturnInt32Datum(devid, SMC8_CHAN_GOING_base + l, (b >> 7) & 1,  0);
}

static void EnqRD_PARAM(privrec_t *me, int l, int pn)
{
    me->lvmt->q_enq_v(me->handle, SQ_IF_ABSENT, 3,
                      DESC_RD_PARAM, l, pn);
}

static void smc8_in (int devid, void *devptr,
                     int desc, size_t dlc, uint8 *data)
{
  privrec_t   *me       = (privrec_t *) devptr;
  int32        val;     // Value
  int          l;
  int          pn;
  int          bit;

    if      (desc == DESC_RD_PARAM  ||
             desc == DESC_WR_PARAM)
    {
        if (dlc < 3)
        {
            DoDriverLog(devid, 0,
                        "DESC_%s_PARAM: dlc=%d, <3",
                        desc == DESC_RD_PARAM? "RD":"WR", (int)dlc);
            SetDevState(devid, DEVSTATE_OFFLINE, CXRF_REM_C_PROBL,
                        desc == DESC_RD_PARAM? "DESC_RD_PARAM packet too short":"DESC_WR_PARAM packet too short");
            return;
        }

        l  = data[1];
        pn = data[2];
        if (l >= SMC8_NUMLINES)
        {
            DoDriverLog(devid, 0,
                        "DESC_%s_PARAM: line=%d, >%d",
                        desc == DESC_RD_PARAM? "RD":"WR", l, SMC8_NUMLINES-1);
            SetDevState(devid, DEVSTATE_OFFLINE, CXRF_REM_C_PROBL,
                        desc == DESC_RD_PARAM? "DESC_RD_PARAM line too big":"DESC_WR_PARAM line too big");
            return;
        }

        me->lvmt->q_erase_and_send_next_v(me->handle, -3, desc, l, pn);

        if      (pn == PARAM_CSTATUS_REG)
        {
            ReturnCStatus(devid, me, l, data[3]);
        }
        else if (pn == PARAM_CONTROL_REG)
        {
            me->ctl[l].cur_val = data[3];
            ////DoDriverLog(devid, 0 | DRIVERLOG_C_PKTDUMP, "{71,02,01      cur_val=%02x", me->ctl[l].cur_val);
            me->ctl[l].rcvd    = 1;
            /* Do we have a pending write request? */
            if (me->ctl[l].pend)
                send_wrctl_cmd(me, l);

            for (bit = 0;
                 bit <= (SMC8_g_CONTROL_LAST - SMC8_g_CONTROL_FIRST);
                 bit++)
                ReturnInt32Datum(devid, 
                                 (SMC8_g_CONTROL_FIRST + bit) * SMC8_NUMLINES + l,
                                 (data[3] >> bit) & 1, 0);
        }
        else if (pn == PARAM_START_FREQ  ||
                 pn == PARAM_FINAL_FREQ  ||
                 pn == PARAM_ACCEL)
        {
            ReturnInt32Datum(devid,
                             /*!!! Here we use the fact that "groups" and "params" are in synced order */
                             (SMC8_CHAN_START_FREQ_base +
                              (pn - PARAM_START_FREQ) * SMC8_NUMLINES
                             ) + l,
                             (data[3] << 8) | data[4], 0);
        }
        else if (pn == PARAM_NUM_STEPS)
        {
            ReturnInt32Datum(devid,
                             SMC8_CHAN_NUM_STEPS_base + l,
                             (data[3] << 24) | (data[4] << 16) |
                             (data[5] << 8)  |  data[6],
                             0);
        }
        else if (pn == PARAM_POS_CNT)
        {
            ReturnInt32Datum(devid,
                             SMC8_CHAN_COUNTER_base + l,
                             (data[3] << 24) | (data[4] << 16) |
                             (data[5] << 8)  |  data[6],
                             0);
        }
    }
    else if (desc == DESC_START_STOP)
    {
        l  = data[1];
        me->lvmt->q_erase_and_send_next_v(me->handle, -3, desc, l, data[2]);
        ReturnCStatus(devid, me, l, data[3]);
    }
    else if (desc == DESC_ERROR)
    {
        fprintf(stderr, "DESC_ERROR(dlc=%d): %08x,%08x,%08x,%08x\n", (int)dlc, data[1], data[2], data[3], data[4]);
        if (dlc < 3)
        {
            DoDriverLog(devid, 0,
                        "DESC_ERROR: dlc=%d, <3 -- terminating",
                        (int)dlc);
            SetDevState(devid, DEVSTATE_OFFLINE, CXRF_REM_C_PROBL, "DESC_ERROR dlc<3");
            return;
        }
        if (data[1] == ERR_UNKCMD)
        {
            DoDriverLog(devid, 0,
                        "DESC_ERROR: ERR_UNKCMD cmd=0x%02x", 
                        data[1]);
            SetDevState(devid, DEVSTATE_OFFLINE, CXRF_REM_C_PROBL, "ERR_UNKCMD");
            return;
        }
        if (data[1] == ERR_LENGTH)
        {
            DoDriverLog(devid, 0,
                        "DESC_ERROR: ERR_LENGTH of cmd=0x%02x", 
                        data[2]);
            SetDevState(devid, DEVSTATE_OFFLINE, CXRF_REM_C_PROBL, "ERR_LENGTH");
            return;
        }
        if (data[1] == ERR_INV_VAL    ||
            data[1] == ERR_INV_STATE)
        {
            if (data[2] == DESC_WR_PARAM)
                EnqRD_PARAM(me, data[3], data[4]);
            me->lvmt->q_erase_and_send_next_v(me->handle,
                                              -3, data[2], data[3], data[4]);
            return;
        }
        if (data[1] == ERR_INV_PARAM)
        {
            me->lvmt->q_erase_and_send_next_v(me->handle,
                                              -3, data[2], data[3], data[4]);
            return;
        }
    }
}

static void smc8_rw_p(int devid, void *devptr,
                      int action,
                      int count, int *addrs,
                      cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t    *me       = (privrec_t *) devptr;
  int           n;       // channel N in addrs[]/.../values[] (loop ctl var)
  int           chn;     // channel
  int32         val;     // Value
  int           g;       // Group #
  int           l;       // Line #
  int           pn;      // Param #

  uint8         mask;

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

        g = chn / SMC8_NUMLINES;
        l = chn % SMC8_NUMLINES;
        
        if (chn == SMC8_CHAN_HW_VER  ||  chn == SMC8_CHAN_SW_VER)
        {
            /* Do-nothing */
        }
        else if (g == SMC8_g_GOING  ||  g == SMC8_g_STATE  ||
                 g == SMC8_g_KM     ||  g == SMC8_g_KP)
        {
            EnqRD_PARAM(me, l, PARAM_CSTATUS_REG);
        }
        else if (g == SMC8_g_COUNTER)
        {
            EnqRD_PARAM(me, l, PARAM_POS_CNT);
        }
        else if (g == SMC8_g_STOP)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
            {
                /* Erase "START", if any */
                me->lvmt->q_foreach_v(me->handle, SQ_ERASE_ALL,
                                      3, DESC_START_STOP, l, 1);
                /* Send "STOP" -- both directly and enqueue (for case if direct-send fails) */
                me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                                      SQ_TRIES_DIR, 0,
                                      NULL, NULL,
                                      0, 3,
                                      DESC_START_STOP, l, 0);
                me->lvmt->q_enq_v    (me->handle, SQ_IF_NONEORFIRST, 3,
                                      DESC_START_STOP, l, 0);
            }
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (g == SMC8_g_GO)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
            {
                /*!!! That's weird!  Should check if there's any other "GO"
                      command in queue, and if so -- remove it.
                      Or, maybe, even more -- check last-known CStatus value
                      for "RUNNING" bit.
                 */
                me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 3,
                                  DESC_START_STOP, l, 1);
            }
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (g == SMC8_g_GO_N_STEPS)
        {
            if (action == DRVA_WRITE  &&  val != 0)
            {
            }
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (g == SMC8_g_RST_COUNTER)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
            {
                me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 7,
                                  DESC_WR_PARAM, l, PARAM_POS_CNT,
                                  0, 0, 0, 0);
            }
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (g == SMC8_g_START_FREQ    ||
                 g == SMC8_g_FINAL_FREQ    ||
                 g == SMC8_g_ACCELERATION  ||
                 g == SMC8_g_NUM_STEPS)
        {
            /*!!! Here we use the fact that "groups" and "params" are in synced order */
            pn = PARAM_START_FREQ + (g - SMC8_g_START_FREQ);
            if (action == DRVA_WRITE)
            {
                if (g == SMC8_g_NUM_STEPS)
                {
                    me->lvmt->q_enqueue_v(me->handle, SQ_REPLACE_NOTFIRST,
                                          SQ_TRIES_INF, 0,
                                          NULL, NULL,
                                          3, 7,
                                          DESC_WR_PARAM, l, pn,
                                          (val >> 24) & 0xFF,
                                          (val >> 16) & 0xFF,
                                          (val >>  8) & 0xFF,
                                           val        & 0xFF);
                }
                else
                {
                    if (val < 0)     val = 0;
                    if (val > 65535) val = 65535;
                    me->lvmt->q_enqueue_v(me->handle, SQ_REPLACE_NOTFIRST,
                                          SQ_TRIES_INF, 0,
                                          NULL, NULL,
                                          3, 5,
                                          DESC_WR_PARAM, l, pn,
                                          (val >>  8) & 0xFF,
                                           val        & 0xFF);
                }
            }
            else
                EnqRD_PARAM(me, l, pn);
        }
        else if (g >= SMC8_g_CONTROL_FIRST  &&
                 g <= SMC8_g_CONTROL_LAST)
        {
            if (action == DRVA_READ)
                EnqRD_PARAM(me, l, PARAM_CONTROL_REG);
            else
            {
                ////DoDriverLog(devid, 0 | DRIVERLOG_C_PKTDUMP, "{71,02,01   PRE   cur_val=%02x", me->ctl[l].cur_val);
                mask = 1 << (g - SMC8_g_CONTROL_FIRST);
                if (val != 0) me->ctl[l].req_val |=  mask;
                else          me->ctl[l].req_val &=~ mask;
                me->ctl[l].req_msk |= mask;
                ////DoDriverLog(devid, 0 | DRIVERLOG_C_PKTDUMP, "{71,02,01   PST   cur_val=%02x", me->ctl[l].cur_val);

                me->ctl[l].pend = 1;
                /* May we perform write right now? */
                if (me->ctl[l].rcvd)
                    send_wrctl_cmd(me, l);
                /* No, we should request read first... */
                else
                    EnqRD_PARAM(me, l, PARAM_CONTROL_REG);
            }
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(smc8, "Yaminov KShD8",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   smc8_init_d, NULL, smc8_rw_p);
