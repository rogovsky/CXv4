#include <stdio.h>

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/kozdev_common_drv_i.h"

#include "drv_i/weldproc_drv_i.h"


enum
{
    SODC_PLACEHOLDER,
    SODC_OUT_MODE,
    SODC_DO_TAB_DROP,
    SODC_DO_TAB_ACTIVATE,
    SODC_DO_TAB_START,
    SODC_DO_TAB_STOP,

    SODC_TAB_TIMES,

    SODC_CUR_n_base,
    SODC_CUR_n_last = SODC_CUR_n_base + WELDPROC_max_lines_count - 1,

    SODC_TAB_n_base,
    SODC_TAB_n_last = SODC_TAB_n_base + WELDPROC_max_lines_count - 1,

    SUBORD_NUMCHANS
};

static vdev_sodc_dsc_t sodc2ourc_mapping[SUBORD_NUMCHANS] =
{
    [SODC_PLACEHOLDER] = {"cur0._devstate", 0, -1},

    [SODC_OUT_MODE]        = {"dac.out_mode",         VDEV_IMPR, -1},
    [SODC_DO_TAB_DROP]     = {"dac.do_tab_drop",      0,         -1},
    [SODC_DO_TAB_ACTIVATE] = {"dac.do_tab_activate",  0,         -1},
    [SODC_DO_TAB_START]    = {"dac.do_tab_start",     VDEV_IMPR, -1},
    [SODC_DO_TAB_STOP]     = {"dac.do_tab_stop",      0,         -1},

    [SODC_TAB_TIMES]       = {"dac.out_tab_times",    0,         -1, 0, CXDTYPE_INT32, 2},

#define DEF_ONE_LINE(n) \
    [SODC_CUR_n_base+n]    = {"cur"__CX_STRINGIZE(n), VDEV_IMPR|VDEV_TUBE, WELDPROC_CHAN_CUR_n_base + n}, \
    [SODC_TAB_n_base+n]    = {"tab"__CX_STRINGIZE(n), VDEV_IMPR, -1, 0, CXDTYPE_INT32, 2}
    /* Unfortunately, there's no loop facility in CPP,
       so lines 0-9 are written directly instead of loop_for(0...WELDPROC_max_lines_count) */
    DEF_ONE_LINE(0),
    DEF_ONE_LINE(1),
    DEF_ONE_LINE(2),
    DEF_ONE_LINE(3),
    DEF_ONE_LINE(4),
    DEF_ONE_LINE(5),
    DEF_ONE_LINE(6),
    DEF_ONE_LINE(7),
    DEF_ONE_LINE(8),
    DEF_ONE_LINE(9),
};

static int ourc2sodc[WELDPROC_NUMCHANS];

static const char *devstate_names[] = {"dac._devstate"};

enum
{
    WORK_HEARTBEAT_PERIOD =    100000, // 100ms/10Hz
};

//--------------------------------------------------------------------

typedef struct
{
    int                 devid;

    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];

    int                 numlines;
    vdev_sodc_dsc_t     this_mapping[countof(sodc2ourc_mapping)];    

    char               *at_beg_src;
    char               *at_end_src;
    cda_dataref_t       at_beg_ref;
    cda_dataref_t       at_end_ref;

    int32               time_us;

    int32               beg_val    [WELDPROC_max_lines_count];
    int32               end_val    [WELDPROC_max_lines_count];
    int                 beg_val_set[WELDPROC_max_lines_count];
    int                 end_val_set[WELDPROC_max_lines_count];
} privrec_t;

static psp_paramdescr_t text2cfg[] =
{
    PSP_P_MSTRING("at_beg", privrec_t, at_beg_src, NULL, 10000),
    PSP_P_MSTRING("at_end", privrec_t, at_end_src, NULL, 10000),
    PSP_P_END()
};

static inline void SndCVal(privrec_t *me, int sodc, int32 val)
{
    cda_snd_ref_data(me->cur[sodc].ref, CXDTYPE_INT32, 1, &val);
}

static inline void SndDupl(privrec_t *me, int sodc, int32 val1, int32 val2)
{
  int32 dupl[2] = {val1, val2};

    cda_snd_ref_data(me->cur[sodc].ref, CXDTYPE_INT32, 2, dupl);
}

//--------------------------------------------------------------------

enum
{
    EBW_STATE_UNKNOWN   = 0,
    EBW_STATE_DETERMINE,         // 1

    EBW_STATE_READY,             // 2

    EBW_STATE_START,             // 3
    EBW_STATE_SET_TABLE,         // 4
    EBW_STATE_ACTIVATE,          // 5
    EBW_STATE_GO,                // 6

    EBW_STATE_WAIT_END,          // 7
    EBW_STATE_FINISH,            // 8

    EBW_STATE_DO_STOP,           // 9
    EBW_STATE_CHECK_STOP,        // 10

    EBW_STATE_count
};

//--------------------------------------------------------------------
static void SwchToUNKNOWN(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

}

static void SwchToDETERMINE(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;
  int        nl;

    for (nl = 0;  nl < me->numlines;  nl++)
    {
        if (!(me->beg_val_set[nl]))
        {
            me->beg_val    [nl] = me->cur[SODC_CUR_n_base + nl].v.i32;
            me->beg_val_set[nl] = 1;
            ReturnInt32Datum(me->devid, WELDPROC_CHAN_BEG_n_base + nl, me->beg_val[nl], 0);
        }
        if (!(me->end_val_set[nl]))
        {
            me->end_val    [nl] = me->cur[SODC_CUR_n_base + nl].v.i32;
            me->end_val_set[nl] = 1;
            ReturnInt32Datum(me->devid, WELDPROC_CHAN_END_n_base + nl, me->end_val[nl], 0);
        }
    }

    if (me->cur[SODC_OUT_MODE].v.i32 == KOZDEV_OUT_MODE_NORM)
        vdev_set_state(&(me->ctx), EBW_STATE_READY);
    else
        vdev_set_state(&(me->ctx), EBW_STATE_WAIT_END);
}

static int  IsAlwdSTART(void *devptr, int prev_state __attribute__((unused)))
{
    return prev_state == EBW_STATE_READY;
}

static void SwchToSET_TABLE(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;
  int        nl;

    SndDupl    (me, SODC_TAB_TIMES,       0,               me->time_us);
    for (nl = 0;  nl < me->numlines;  nl++)
        SndDupl(me, SODC_TAB_n_base + nl, me->beg_val[nl], me->end_val[nl]);
}

static void SwchToACTIVATE(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    SndCVal(me, SODC_DO_TAB_ACTIVATE, CX_VALUE_COMMAND);
}

static int  IsAlwdGO(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return me->cur[SODC_DO_TAB_START].v.i32 == 0;
}

static void SwchToGO(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    if (cda_ref_is_sensible(me->at_beg_ref)) 
        cda_set_dcval(me->at_beg_ref, CX_VALUE_COMMAND);
    SndCVal(me, SODC_DO_TAB_START,    CX_VALUE_COMMAND);
}

static int  IsAlwdFINISH(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return me->cur[SODC_OUT_MODE].v.i32 == KOZDEV_OUT_MODE_NORM;
}

static void SwchToFINISH(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    if (cda_ref_is_sensible(me->at_end_ref)) 
        cda_set_dcval(me->at_end_ref, CX_VALUE_COMMAND);
    SndCVal(me, SODC_DO_TAB_DROP,     CX_VALUE_COMMAND);
}

static int  IsAlwdDO_STOP(void *devptr, int prev_state __attribute__((unused)))
{
    return prev_state != EBW_STATE_UNKNOWN;
}

static void SwchToDO_STOP(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    if (cda_ref_is_sensible(me->at_end_ref)) 
        cda_set_dcval(me->at_end_ref, CX_VALUE_COMMAND);
    SndCVal(me, SODC_DO_TAB_STOP,     CX_VALUE_COMMAND);
    SndCVal(me, SODC_DO_TAB_DROP,     CX_VALUE_COMMAND);
}

static int  IsAlwdCheckSTOP(void *devptr, int prev_state __attribute__((unused)))
{
  privrec_t *me = devptr;

    return me->cur[SODC_OUT_MODE].v.i32 == KOZDEV_OUT_MODE_NORM;
}

static vdev_state_dsc_t state_descr[] =
{
    [EBW_STATE_UNKNOWN]      = {0,       -1,                     NULL,               SwchToUNKNOWN,      NULL},
    [EBW_STATE_DETERMINE]    = {0,       -1,                     NULL,               SwchToDETERMINE,    NULL},

    [EBW_STATE_READY]        = {0,       -1,                     NULL,               NULL,               NULL},

    [EBW_STATE_START]        = {1*0,       EBW_STATE_SET_TABLE,    IsAlwdSTART,        NULL,               NULL},
    [EBW_STATE_SET_TABLE]    = {1*0,       EBW_STATE_ACTIVATE,     NULL,               SwchToSET_TABLE,    NULL},
    [EBW_STATE_ACTIVATE]     = {1000000, EBW_STATE_GO,           NULL,               SwchToACTIVATE,     NULL},
    [EBW_STATE_GO]           = {1*0,       EBW_STATE_WAIT_END,     IsAlwdGO,           SwchToGO,           (int[]){SODC_DO_TAB_START, -1}},
    [EBW_STATE_WAIT_END]     = {1*0,       EBW_STATE_FINISH,       NULL,               NULL,               NULL},
    [EBW_STATE_FINISH]       = {1*0,       EBW_STATE_READY,        IsAlwdFINISH,       SwchToFINISH,       (int[]){SODC_OUT_MODE, -1}},

    [EBW_STATE_DO_STOP]      = {0,       EBW_STATE_CHECK_STOP,   IsAlwdDO_STOP,      SwchToDO_STOP,      NULL},
    [EBW_STATE_CHECK_STOP]   = {1*0,       EBW_STATE_READY,        IsAlwdCheckSTOP,    NULL,               (int[]){SODC_OUT_MODE, -1}},
};

static int state_important_channels[countof(state_descr)][SUBORD_NUMCHANS];

//////////////////////////////////////////////////////////////////////

static int weldproc_init_mod(void)
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
    for (x = 0;  x < countof(sodc2ourc_mapping);  x++)
        if (sodc2ourc_mapping[x].mode != 0  &&  sodc2ourc_mapping[x].ourc >= 0)
            ourc2sodc[sodc2ourc_mapping[x].ourc] = x;

    return 0;
}

//////////////////////////////////////////////////////////////////////

static void weldproc_rw_p(int devid, void *devptr,
                          int action,
                          int count, int *addrs,
                          cxdtype_t *dtypes, int *nelems, void **values);
static void weldproc_sodc_cb(int devid, void *devptr,
                             int sodc, int32 val);

static vdev_sr_chan_dsc_t state_related_channels[] =
{
    {WELDPROC_CHAN_DO_START,     EBW_STATE_START,        0,                 CX_VALUE_DISABLED_MASK},
    {WELDPROC_CHAN_DO_STOP,      EBW_STATE_DO_STOP,      0,                 CX_VALUE_DISABLED_MASK},
    {WELDPROC_CHAN_IS_WELDING,   -1,                     0,                 0},
    {-1,                         -1,                     0,                 0},
};

static int weldproc_init_d(int devid, void *devptr,
                           int businfocount, int businfo[],
                           const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;
  const char     *p  = auxinfo;
  const char     *endp;
  char            hiername[100];
  size_t          len;
  int             idx;
  int             vdev_init_r;

    me->devid = devid;
    if (p == NULL  ||  *p == '\0')
    {
        DoDriverLog(devid, 0,
                    "%s(): base-cpoint-hierarchy name is required in auxinfo",
                    __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }
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
    me->numlines = strtol(p, &endp, 10);
    if (endp == p  ||
        me->numlines < 1  ||
        me->numlines >= WELDPROC_max_lines_count)
    {
        DoDriverLog(devid, 0,
                    "%s(): '/N' (N in [1...%d]) expected in auxinfo",
                    __FUNCTION__, WELDPROC_max_lines_count);
        return -CXRF_CFG_PROBL;
    }

    if (psp_parse(endp, &p,
                  me,
                  '=', " \t", "",
                  text2cfg) != PSP_R_OK)
    {
        DoDriverLog(devid, 0,
                    "psp_parse()@init: %s",
                    psp_error());
    }

    for (idx = 0;  idx < countof(me->this_mapping);  idx++)
        me->this_mapping[idx] =
            sodc2ourc_mapping[(
                               (idx >= SODC_CUR_n_base + me->numlines  &&
                                idx <  SODC_CUR_n_base + WELDPROC_max_lines_count)
                               ||
                               (idx >= SODC_TAB_n_base + me->numlines  &&
                                idx <  SODC_TAB_n_base + WELDPROC_max_lines_count)
                              )?
                              SODC_PLACEHOLDER : idx
                             ];

#if 1
    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = me->this_mapping;
#else // vvv this variant could work only if SODC_CUR_n_ and SODC_TAB_n_ would go in pairs, interleaving
    me->ctx.num_sodcs      = SUBORD_NUMCHANS - (WELDPROC_max_lines_count - me->numlines) * 2;
    me->ctx.map            = sodc2ourc_mapping;
#endif
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = weldproc_rw_p;
    me->ctx.sodc_cb        = weldproc_sodc_cb;

    me->ctx.our_numchans             = WELDPROC_NUMCHANS;
    me->ctx.chan_state_n             = WELDPROC_CHAN_VDEV_STATE;
    me->ctx.state_unknown_val        = EBW_STATE_UNKNOWN;
    me->ctx.state_determine_val      = EBW_STATE_DETERMINE;
    me->ctx.state_count              = countof(state_descr);
    me->ctx.state_descr              = state_descr;
    me->ctx.state_related_channels   = state_related_channels;
    me->ctx.state_important_channels = state_important_channels;

    /*!!!SetChanRDs, SetChanReturnType*/
    SetChanRDs       (devid, WELDPROC_CHAN_TIME,       1,            1000000.0, 0.0);
    SetChanRDs       (devid, WELDPROC_CHAN_BEG_n_base, me->numlines, 1000000.0, 0.0);
    SetChanRDs       (devid, WELDPROC_CHAN_END_n_base, me->numlines, 1000000.0, 0.0);
    SetChanRDs       (devid, WELDPROC_CHAN_CUR_n_base, me->numlines, 1000000.0, 0.0);
    SetChanReturnType(devid, WELDPROC_CHAN_CUR_n_base, me->numlines, IS_AUTOUPDATED_TRUSTED);

    me->time_us = 5*1000000; // Default time is 5s

    vdev_init_r = vdev_init(&(me->ctx), devid, devptr, WORK_HEARTBEAT_PERIOD, hiername);
    if (vdev_init_r < 0) return vdev_init_r;

    if (me->at_beg_src != NULL  &&  me->at_beg_src[0] != '\0')
    {
        me->at_beg_ref = cda_add_formula(me->ctx.cid, "",
                                         me->at_beg_src, CDA_OPT_WR_FLA,
                                         NULL, 0,
                                         0, NULL, NULL);
        if (me->at_beg_ref < 0)
        {
            DoDriverLog(devid, 0, "error registering at_beg-formula: %s", cda_last_err());
            return -CXRF_CFG_PROBL;
        }
    }

    if (me->at_end_src != NULL  &&  me->at_end_src[0] != '\0')
    {
        me->at_end_ref = cda_add_formula(me->ctx.cid, "",
                                         me->at_end_src, CDA_OPT_WR_FLA,
                                         NULL, 0,
                                         0, NULL, NULL);
        if (me->at_end_ref < 0)
        {
            DoDriverLog(devid, 0, "error registering at_end-formula: %s", cda_last_err());
            return -CXRF_CFG_PROBL;
        }
    }

    return vdev_init_r;
}

static void weldproc_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void weldproc_sodc_cb(int devid, void *devptr,
                             int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;

    /* If not all info is gathered yet, then there's nothing else to do yet */
    if (me->ctx.cur_state == EBW_STATE_UNKNOWN) return;
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

static void weldproc_rw_p(int devid, void *devptr,
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
  int                 nl;

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

        if      (sodc >= 0  &&  sodc2ourc_mapping[sodc].mode & VDEV_TUBE)
        {
            if (action == DRVA_WRITE)
                SndCVal(me, sodc, val);
            else
                if (me->cur[sodc].rcvd)
                    ReturnInt32DatumTimed(devid, chn, me->cur[sodc].v.i32,
                                          me->cur[sodc].flgs,
                                          me->cur[sodc].ts);
        }
        else if (chn >= WELDPROC_CHAN_BEG_n_base  &&
                 chn <  WELDPROC_CHAN_BEG_n_base + me->numlines)
        {
            nl = chn - WELDPROC_CHAN_BEG_n_base;
            if (action == DRVA_WRITE)
            {
                me->beg_val    [nl] = val;
                me->beg_val_set[nl] = 1;
            }
            if (me->beg_val_set[nl])
                ReturnInt32Datum(me->devid, WELDPROC_CHAN_BEG_n_base + nl, me->beg_val[nl], 0);
        }
        else if (chn >= WELDPROC_CHAN_END_n_base  &&
                 chn <  WELDPROC_CHAN_END_n_base + me->numlines)
        {
            nl = chn - WELDPROC_CHAN_END_n_base;
            if (action == DRVA_WRITE)
            {
                me->end_val    [nl] = val;
                me->end_val_set[nl] = 1;
            }
            if (me->end_val_set[nl])
                ReturnInt32Datum(me->devid, WELDPROC_CHAN_END_n_base + nl, me->end_val[nl], 0);
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
        else if (chn == WELDPROC_CHAN_RESET_STATE)
        {
            if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                vdev_set_state(&(me->ctx), EBW_STATE_DETERMINE);
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn == WELDPROC_CHAN_TIME)
        {
            if (action == DRVA_WRITE)
            {
                if (val < 100*1000)    val = 100*1000;    // Min 100ms -- arbitrary limit
                if (val > 655*1000000) val = 655*1000000; // Max 655s: CAC208/CEAC124 limit (100steps/s, 16bits => 65535
                me->time_us = val;
            }
            ReturnInt32Datum(devid, chn, me->time_us, 0);
        }
        else if (chn == WELDPROC_CHAN_IS_WELDING)
        {
            val = (me->ctx.cur_state >= EBW_STATE_START  &&
                   me->ctx.cur_state <= EBW_STATE_FINISH);
            ReturnInt32Datum(devid, chn, val, 0);
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(weldproc, "Electron beam weld process table-driver via CAC208/CEAC124 ",
                   weldproc_init_mod, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   weldproc_init_d, weldproc_term_d, weldproc_rw_p);
