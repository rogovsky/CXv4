#include "cxsd_driver.h"
#include "pzframe_drv.h"
#include "pci4624_lyr.h"

#include "drv_i/adc200me_drv_i.h"


//////////////////////////////////////////////////////////////////////

enum
{
    ADC200ME_REG_START_ADDR = 0x00,
    ADC200ME_REG_PAGE_LEN   = 0x04,
    ADC200ME_REG_CSR        = 0x08,
    ADC200ME_REG_CUR_ADDR   = 0x0C,
    ADC200ME_REG_PAGE_NUM   = 0x10,
    ADC200ME_REG_IRQS       = 0x14,
    ADC200ME_REG_RESERVE_18 = 0x18,
    ADC200ME_REG_SERIAL     = 0x1C,
};

enum
{
    ADC200ME_CSR_RUN_shift     = 0,  ADC200ME_CSR_RUN     = 1 << ADC200ME_CSR_RUN_shift,
    ADC200ME_CSR_HLT_shift     = 1,  ADC200ME_CSR_HLT     = 1 << ADC200ME_CSR_HLT_shift,
    ADC200ME_CSR_STM_shift     = 2,  ADC200ME_CSR_STM     = 1 << ADC200ME_CSR_STM_shift,
    ADC200ME_CSR_Fn_shift      = 3,  ADC200ME_CSR_Fn      = 1 << ADC200ME_CSR_Fn_shift,
    ADC200ME_CSR_BUSY_shift    = 4,  ADC200ME_CSR_BUSY    = 1 << ADC200ME_CSR_BUSY_shift,
    ADC200ME_CSR_PM_shift      = 5,  ADC200ME_CSR_PM      = 1 << ADC200ME_CSR_PM_shift,
    ADC200ME_CSR_PC_shift      = 6,  ADC200ME_CSR_PC      = 1 << ADC200ME_CSR_PC_shift,
    ADC200ME_CSR_REC_shift     = 7,  ADC200ME_CSR_REC     = 1 << ADC200ME_CSR_REC_shift,
    ADC200ME_CSR_A_shift       = 8,  ADC200ME_CSR_A_MASK  = 3 << ADC200ME_CSR_A_shift,
    ADC200ME_CSR_B_shift       = 10, ADC200ME_CSR_B_MASK  = 3 << ADC200ME_CSR_B_shift,
    ADC200ME_CSR_ERR_LN_shift  = 12, ADC200ME_CSR_ERR_LN  = 1 << ADC200ME_CSR_ERR_LN_shift,
    ADC200ME_CSR_ERR_PN_shift  = 13, ADC200ME_CSR_ERR_PN  = 1 << ADC200ME_CSR_ERR_PN_shift,
    ADC200ME_CSR_ERR_PC_shift  = 14, ADC200ME_CSR_ERR_PC  = 1 << ADC200ME_CSR_ERR_PC_shift,
    ADC200ME_CSR_ERR_OF_shift  = 15, ADC200ME_CSR_ERR_OF  = 1 << ADC200ME_CSR_ERR_OF_shift,
    ADC200ME_CSR_INT_ENA_shift = 16, ADC200ME_CSR_INT_ENA = 1 << ADC200ME_CSR_INT_ENA_shift,
};

enum
{
    FASTADC_VENDOR_ID      = 0x4624,
    FASTADC_DEVICE_ID      = 0xADC1,
    FASTADC_SERIAL_OFFSET  = ADC200ME_REG_SERIAL,
    
    FASTADC_IRQ_CHK_OP     = PCI4624_OP_RD,
    FASTADC_IRQ_CHK_OFS    = ADC200ME_REG_IRQS,
    FASTADC_IRQ_CHK_MSK    = -1,
    FASTADC_IRQ_CHK_VAL    = 0,
    FASTADC_IRQ_CHK_COND   = PCI4624_COND_NONZERO,
    
    FASTADC_IRQ_RST_OP     = PCI4624_OP_WR,
    FASTADC_IRQ_RST_OFS    = ADC200ME_REG_IRQS,
    FASTADC_IRQ_RST_VAL    = 0,
    
    FASTADC_ON_CLS1_OP     = PCI4624_OP_WR,
    FASTADC_ON_CLS1_OFS    = ADC200ME_REG_CSR,
    FASTADC_ON_CLS1_VAL    = ADC200ME_CSR_HLT | ADC200ME_CSR_INT_ENA,
    
    FASTADC_ON_CLS2_OP     = PCI4624_OP_WR,
    FASTADC_ON_CLS2_OFS    = ADC200ME_REG_IRQS,
    FASTADC_ON_CLS2_VAL    = 0
};

static int32        quants[4] = {2000, 1000, 500, 250};


typedef int32 ADC200ME_DATUM_T;
enum { ADC200ME_DTYPE      = CXDTYPE_INT32 };

/* We allow only as much measurements, not ADC200ME_MAX_NUMPTS  */
enum { ADC200ME_MAX_ALLOWED_NUMPTS = 200000*0 + ADC200ME_MAX_NUMPTS };

//--------------------------------------------------------------------

static pzframe_chinfo_t chinfo[] =
{
    /* data */
    [ADC200ME_CHAN_DATA]          = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC200ME_CHAN_LINE1]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC200ME_CHAN_LINE2]         = {PZFRAME_CHTYPE_BIGC,        0},

    [ADC200ME_CHAN_MARKER]        = {PZFRAME_CHTYPE_MARKER,      0},

    /* controls */
    [ADC200ME_CHAN_SHOT]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC200ME_CHAN_STOP]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC200ME_CHAN_ISTART]        = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC200ME_CHAN_WAITTIME]      = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC200ME_CHAN_CALIBRATE]     = {PZFRAME_CHTYPE_INDIVIDUAL,  0},
    [ADC200ME_CHAN_FGT_CLB]       = {PZFRAME_CHTYPE_INDIVIDUAL,  0},
    [ADC200ME_CHAN_VISIBLE_CLB]   = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC200ME_CHAN_CALC_STATS]    = {PZFRAME_CHTYPE_VALIDATE,    0},

    [ADC200ME_CHAN_PTSOFS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC200ME_CHAN_NUMPTS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC200ME_CHAN_TIMING]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC200ME_CHAN_RANGE1]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC200ME_CHAN_RANGE2]        = {PZFRAME_CHTYPE_VALIDATE,    0},

    /* status */
    [ADC200ME_CHAN_ELAPSED]       = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC200ME_CHAN_SERIAL]        = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_CLB_STATE]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_XS_PER_POINT]  = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC200ME_CHAN_XS_DIVISOR]    = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_XS_FACTOR]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_CUR_PTSOFS]    = {PZFRAME_CHTYPE_STATUS,      ADC200ME_CHAN_PTSOFS},
    [ADC200ME_CHAN_CUR_NUMPTS]    = {PZFRAME_CHTYPE_STATUS,      ADC200ME_CHAN_NUMPTS},
    [ADC200ME_CHAN_CUR_TIMING]    = {PZFRAME_CHTYPE_STATUS,      ADC200ME_CHAN_TIMING},
    [ADC200ME_CHAN_CUR_RANGE1]    = {PZFRAME_CHTYPE_STATUS,      ADC200ME_CHAN_RANGE1},
    [ADC200ME_CHAN_CUR_RANGE2]    = {PZFRAME_CHTYPE_STATUS,      ADC200ME_CHAN_RANGE2},

    [ADC200ME_CHAN_LINE1ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC200ME_CHAN_LINE2ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC200ME_CHAN_LINE1RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC200ME_CHAN_LINE2RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC200ME_CHAN_LINE1RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC200ME_CHAN_LINE2RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},

    [ADC200ME_CHAN_CLB_OFS1]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_CLB_OFS2]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_CLB_COR1]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_CLB_COR2]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},

    [ADC200ME_CHAN_MIN1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_MIN2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_MAX1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_MAX2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_AVG1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_AVG2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_INT1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_INT2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},

    [ADC200ME_CHAN_LINE1TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_LINE2TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_LINE1TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_LINE2TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC200ME_CHAN_NUM_LINES]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
};

//--------------------------------------------------------------------

typedef struct
{
    pzframe_drv_t      pz;
    int                devid;
    int                handle;
    pci4624_vmt_t     *lvmt;

    int                timing_alwd;

    int                serial;

    int32              cur_args[ADC200ME_NUMCHANS];
    int32              nxt_args[ADC200ME_NUMCHANS];
    int32              prv_args[ADC200ME_NUMCHANS];
    ADC200ME_DATUM_T   retdata [ADC200ME_MAX_ALLOWED_NUMPTS*ADC200ME_NUM_LINES];
    int                do_return;
    int                force_read;
    pzframe_retbufs_t  retbufs;

    // This holds CSR value WITHOUT RUN/PC bits, i.e. -- effective SETTINGS only
    int32              cword;
    // This holds either ADC200ME_CSR_RUN or ADC200ME_CSR_PC
    int32              start_mask;

    int                clb_valid;
    int32              clb_Offs[2];
    int32              clb_DatS[2];

    int                data_rqd;
    int                line_rqd[ADC200ME_NUM_LINES];
    struct
    {
        int            addrs [ADC200ME_NUMCHANS];
        cxdtype_t      dtypes[ADC200ME_NUMCHANS];
        int            nelems[ADC200ME_NUMCHANS];
        void          *val_ps[ADC200ME_NUMCHANS];
        rflags_t       rflags[ADC200ME_NUMCHANS];
    }                  r;
} adc200me_privrec_t;

#define PDR2ME(pdr) ((adc200me_privrec_t*)pdr) //!!! Should better subtract offsetof(pz)

//--------------------------------------------------------------------

static psp_lkp_t adc200me_timing_lkp[] =
{
    {"200", ADC200ME_T_200MHZ},
    {"int", ADC200ME_T_200MHZ},
    {"ext", ADC200ME_T_EXT},
    {NULL, 0}
};

static psp_lkp_t adc200me_range_lkp [] =
{
    {"4096", ADC200ME_R_4V},
    {"2048", ADC200ME_R_2V},
    {"1024", ADC200ME_R_1V},
    {"512",  ADC200ME_R_05V},
    {NULL, 0}
};

static psp_paramdescr_t adc200me_params[] =
{
    PSP_P_INT   ("ptsofs",   adc200me_privrec_t, nxt_args[ADC200ME_CHAN_PTSOFS], -1, 0, ADC200ME_MAX_ALLOWED_NUMPTS-1),
    PSP_P_INT   ("numpts",   adc200me_privrec_t, nxt_args[ADC200ME_CHAN_NUMPTS], -1, 1, ADC200ME_MAX_ALLOWED_NUMPTS),
    PSP_P_LOOKUP("timing",   adc200me_privrec_t, nxt_args[ADC200ME_CHAN_TIMING], -1, adc200me_timing_lkp),
    PSP_P_LOOKUP("rangeA",   adc200me_privrec_t, nxt_args[ADC200ME_CHAN_RANGE1], -1, adc200me_range_lkp),
    PSP_P_LOOKUP("rangeB",   adc200me_privrec_t, nxt_args[ADC200ME_CHAN_RANGE2], -1, adc200me_range_lkp),
    PSP_P_FLAG  ("calcs",    adc200me_privrec_t, nxt_args[ADC200ME_CHAN_CALC_STATS], 1, 0),
    PSP_P_FLAG  ("nocalcs",  adc200me_privrec_t, nxt_args[ADC200ME_CHAN_CALC_STATS], 0, 1),

    PSP_P_FLAG("notiming", adc200me_privrec_t, timing_alwd, 0, 1),
    PSP_P_FLAG("timing",   adc200me_privrec_t, timing_alwd, 1, 0),
    PSP_P_END()
};

//--------------------------------------------------------------------

static int32 saved_data_signature[] =
{
    0x00FFFFFF,
    0x00000000,
    0x00123456,
    0x00654321,
};

enum
{
    SAVED_DATA_COUNT  = countof(saved_data_signature) + ADC200ME_NUMCHANS,
    SAVED_DATA_OFFSET = (1 << 22) - SAVED_DATA_COUNT * sizeof(int32),
};

static int32 MakeCWord(int TIMING, int RANGE1, int RANGE2)
{
    return
        0 /* RUN=0 */
        |
        0 /* HLT=0 -- sure! */
        |
        ADC200ME_CSR_STM
        |
        ((TIMING == ADC200ME_T_EXT)? ADC200ME_CSR_Fn : 0)
        |
        0 /* PM=0 */
        |
        0 /* PC=0 */
        |
        0 /* REC=0 */
        |
        (RANGE1 << ADC200ME_CSR_A_shift)
        |
        (RANGE2 << ADC200ME_CSR_B_shift)
        |
        0 /* INT_ENA=0 */
        ;
}

static void ReturnCalibrations(adc200me_privrec_t *me)
{
  int  nl;

    ReturnInt32Datum(me->devid,   ADC200ME_CHAN_CLB_STATE,
                     me->cur_args[ADC200ME_CHAN_CLB_STATE], 0);
    for (nl = 0;  nl < ADC200ME_NUM_LINES;  nl++)
    {
        ReturnInt32Datum(me->devid,   ADC200ME_CHAN_CLB_OFS1+nl,
                         me->cur_args[ADC200ME_CHAN_CLB_OFS1+nl], 0);
        ReturnInt32Datum(me->devid,   ADC200ME_CHAN_CLB_COR1+nl,
                         me->cur_args[ADC200ME_CHAN_CLB_COR1+nl], 0);
    }
}

static void InvalidateCalibrations(adc200me_privrec_t *me)
{
  int  nl;

    me->cur_args[ADC200ME_CHAN_CLB_STATE] =
    me->nxt_args[ADC200ME_CHAN_CLB_STATE] =
        me->clb_valid   = 0;
    for (nl = 0;  nl < ADC200ME_NUM_LINES;  nl++)
    {
        me->cur_args[ADC200ME_CHAN_CLB_OFS1+nl] =
        me->nxt_args[ADC200ME_CHAN_CLB_OFS1+nl] =
        me->clb_Offs[                       nl] = 0;

        me->cur_args[ADC200ME_CHAN_CLB_COR1+nl] =
        me->nxt_args[ADC200ME_CHAN_CLB_COR1+nl] =
        me->clb_DatS[                       nl] = ADC200ME_CLB_COR_R;
    }

    ReturnCalibrations(me);
}

static int32 ValidateParam(pzframe_drv_t *pdr, int n, int32 v)
{
  adc200me_privrec_t *me = PDR2ME(pdr);

    if      (n == ADC200ME_CHAN_TIMING)
    {
        if (v < ADC200ME_T_200MHZ) v = ADC200ME_T_200MHZ;
        if (v > ADC200ME_T_EXT)    v = ADC200ME_T_EXT;
        if (!(me->timing_alwd))    v = ADC200ME_T_200MHZ;
    }
    else if (n == ADC200ME_CHAN_RANGE1  ||  n == ADC200ME_CHAN_RANGE2)
    {
        if (v < ADC200ME_R_4V)  v = ADC200ME_R_4V;
        if (v > ADC200ME_R_05V) v = ADC200ME_R_05V;
    }
    else if (n == ADC200ME_CHAN_PTSOFS)
    {
        if (v < 0)                             v = 0;
        if (v > ADC200ME_MAX_ALLOWED_NUMPTS-1) v = ADC200ME_MAX_ALLOWED_NUMPTS-1;
    }
    else if (n == ADC200ME_CHAN_NUMPTS)
    {
        if (v < 1)                             v = 1;
        if (v > ADC200ME_MAX_ALLOWED_NUMPTS)   v = ADC200ME_MAX_ALLOWED_NUMPTS;
    }

    return v;
}

static void Return1Param(adc200me_privrec_t *me, int n, int32 v)
{
    ReturnInt32Datum(me->devid, n, me->cur_args[n] = me->nxt_args[n] = v, 0);
}

static void Init1Param(adc200me_privrec_t *me, int n, int32 v)
{
    if (me->nxt_args[n] < 0) me->nxt_args[n] = v;
}
static int   InitParams(pzframe_drv_t *pdr)
{
  adc200me_privrec_t *me = PDR2ME(pdr);

  int                 n;
  int32               saved_data[SAVED_DATA_COUNT];

    for (n = 0;  n < countof(chinfo);  n++)
        if (chinfo[n].chtype == PZFRAME_CHTYPE_AUTOUPDATED  ||
            chinfo[n].chtype == PZFRAME_CHTYPE_STATUS)
            SetChanReturnType(me->devid, n, 1, IS_AUTOUPDATED_TRUSTED);

    /* HLT device first */
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_CSR,  ADC200ME_CSR_HLT);
    /* Drop IRQ */
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_IRQS, 0);
    /* Read previous-settings-storage area */
    me->lvmt->ReadBar1 (me->handle, SAVED_DATA_OFFSET, saved_data, SAVED_DATA_COUNT);

    /* Try to use previous settings, otherwise use defaults */
    if (memcmp(saved_data, saved_data_signature, sizeof(saved_data_signature)) == 0)
    {
        memcpy(me->nxt_args,
               saved_data + countof(saved_data_signature),
               sizeof(me->nxt_args[0]) * ADC200ME_NUMCHANS);
        for (n = 0;  n < ADC200ME_NUMCHANS;  n++)
            if (n < countof(chinfo)  &&
                chinfo[n].chtype == PZFRAME_CHTYPE_VALIDATE)
                me->nxt_args[n] = ValidateParam(pdr, n, me->nxt_args[n]);
            else
                me->nxt_args[n] = 0;
    }
    else
    {
        Init1Param(me, ADC200ME_CHAN_PTSOFS, 0);
        Init1Param(me, ADC200ME_CHAN_NUMPTS, 1024);
        Init1Param(me, ADC200ME_CHAN_TIMING, ADC200ME_T_200MHZ);
        Init1Param(me, ADC200ME_CHAN_RANGE1, ADC200ME_R_4V);
        Init1Param(me, ADC200ME_CHAN_RANGE2, ADC200ME_R_4V);
        me->nxt_args[ADC200ME_CHAN_CALIBRATE] = 0;
    }

    InvalidateCalibrations(me);

    me->cword = MakeCWord(me->nxt_args[ADC200ME_CHAN_TIMING],
                          me->nxt_args[ADC200ME_CHAN_RANGE1],
                          me->nxt_args[ADC200ME_CHAN_RANGE2]);
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_CSR, me->cword);

    Return1Param(me, ADC200ME_CHAN_SERIAL,
                 me->serial =
                 me->lvmt->ReadBar0(me->handle, ADC200ME_REG_SERIAL));
    DoDriverLog(me->devid, 0, "serial=%d", me->serial);
    Return1Param(me, ADC200ME_CHAN_XS_DIVISOR, 0);
    Return1Param(me, ADC200ME_CHAN_XS_FACTOR,  -9);

    for (n = 0;  n < ADC200ME_NUM_LINES;  n++)
    {
        Return1Param(me, ADC200ME_CHAN_LINE1TOTALMIN + n, ADC200ME_MIN_VALUE);
        Return1Param(me, ADC200ME_CHAN_LINE1TOTALMAX + n, ADC200ME_MAX_VALUE);
    }
    Return1Param    (me, ADC200ME_CHAN_NUM_LINES,         ADC200ME_NUM_LINES);

    SetChanRDs(me->devid, ADC200ME_CHAN_DATA, 1+ADC200ME_NUM_LINES, 1000000.0, 0.0);
    SetChanRDs(me->devid, ADC200ME_CHAN_STATS_first,
                          ADC200ME_CHAN_STATS_last-ADC200ME_CHAN_STATS_first+1,
                                                                    1000000.0, 0.0);
    SetChanRDs(me->devid, ADC200ME_CHAN_ELAPSED, 1, 1000.0, 0.0);

    return DEVSTATE_OPERATING;
}

static int  StartMeasurements(pzframe_drv_t *pdr)
{
  adc200me_privrec_t *me = PDR2ME(pdr);

////fprintf(stderr, "%s\n", __FUNCTION__);
    /* "Actualize" requested parameters */
    memcpy(me->cur_args, me->nxt_args, sizeof(me->cur_args));

    /* Zeroth step: check PTSOFS against NUMPTS */
    if (me->cur_args    [ADC200ME_CHAN_PTSOFS] > ADC200ME_MAX_ALLOWED_NUMPTS - me->cur_args[ADC200ME_CHAN_NUMPTS])
        Return1Param(me, ADC200ME_CHAN_PTSOFS,   ADC200ME_MAX_ALLOWED_NUMPTS - me->cur_args[ADC200ME_CHAN_NUMPTS]);

    /* Check if calibration-affecting parameters had changed */
    if (me->clb_valid  &&
        (me->cur_args[ADC200ME_CHAN_TIMING] != me->prv_args[ADC200ME_CHAN_TIMING]  ||
         me->cur_args[ADC200ME_CHAN_RANGE1] != me->prv_args[ADC200ME_CHAN_RANGE1]  ||
         me->cur_args[ADC200ME_CHAN_RANGE2] != me->prv_args[ADC200ME_CHAN_RANGE2]))
        InvalidateCalibrations(me);

    /* Store parameters for future comparison */
    memcpy(me->prv_args, me->cur_args, sizeof(me->prv_args));

    /* Stop/init the device first */
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_CSR, ADC200ME_CSR_HLT);
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_CSR, 0);

    /* Now program various parameters */
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_START_ADDR, 0);
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_PAGE_LEN,
                        me->cur_args[ADC200ME_CHAN_NUMPTS] + me->cur_args[ADC200ME_CHAN_PTSOFS]);
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_PAGE_NUM,   1);

    /* And, finally, the CSR */
    me->cword = MakeCWord(me->cur_args[ADC200ME_CHAN_TIMING],
                          me->cur_args[ADC200ME_CHAN_RANGE1],
                          me->cur_args[ADC200ME_CHAN_RANGE2]);
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_CSR, me->cword);

    if ((me->cur_args[ADC200ME_CHAN_CALIBRATE] & CX_VALUE_LIT_MASK) != 0)
    {
        me->start_mask = ADC200ME_CSR_PC;
        Return1Param(me, ADC200ME_CHAN_CALIBRATE, 0);
    }
    else
        me->start_mask = ADC200ME_CSR_RUN;

    /* Save settings in a previous-settings-storage area */
    me->lvmt->WriteBar1(me->handle,
                        SAVED_DATA_OFFSET + sizeof(saved_data_signature),
                        me->cur_args,
                        ADC200ME_NUMCHANS);
    me->lvmt->WriteBar1(me->handle,
                        SAVED_DATA_OFFSET,
                        saved_data_signature,
                        countof(saved_data_signature));

    me->do_return =
        (me->start_mask != ADC200ME_CSR_PC  ||
         (me->cur_args[ADC200ME_CHAN_VISIBLE_CLB] & CX_VALUE_LIT_MASK) != 0);
    me->force_read = me->start_mask == ADC200ME_CSR_PC;
    
    /* Let's go! */
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_CSR, me->cword | me->start_mask);

    return PZFRAME_R_DOING;
}

static int  TrggrMeasurements(pzframe_drv_t *pdr)
{
  adc200me_privrec_t *me = PDR2ME(pdr);

////fprintf(stderr, "%s\n", __FUNCTION__);
    /* Don't even try anything in CALIBRATE mode */
    if (me->start_mask == ADC200ME_CSR_PC) return PZFRAME_R_DOING;

    /* 0. HLT device first (that is safe and don't touch any other registers) */
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_CSR, ADC200ME_CSR_HLT);

    /* 1. Allow prog-start */
    me->cword &=~ ADC200ME_CSR_STM;
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_CSR, me->cword);
    /* 2. Do prog-start */
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_CSR, me->cword | ADC200ME_CSR_RUN);

    return PZFRAME_R_DOING;
}

static int  AbortMeasurements(pzframe_drv_t *pdr)
{
  adc200me_privrec_t *me = PDR2ME(pdr);
  int                 n;

////fprintf(stderr, "%s\n", __FUNCTION__);
    me->cword &=~ ADC200ME_CSR_STM;
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_CSR, me->cword |  ADC200ME_CSR_HLT);
    me->lvmt->WriteBar0(me->handle, ADC200ME_REG_CSR, me->cword);

    if (me->cur_args[ADC200ME_CHAN_NUMPTS] != 0)
        bzero(me->retdata,
              sizeof(me->retdata[0])
              *
              me->cur_args[ADC200ME_CHAN_NUMPTS] * ADC200ME_NUM_LINES);

    for (n = ADC200ME_CHAN_STATS_first;  n <= ADC200ME_CHAN_STATS_last;  n++)
        me->cur_args[n] = me->nxt_args[n] = 0;

    return PZFRAME_R_READY;
}

static int32 ReadOneCalibr(adc200me_privrec_t *me, int block, int ab)
{
  int32             buf[1024];
  int               x;
  int32             v;
  
    me->lvmt->ReadBar1(me->handle, sizeof(int32)*1024*block, buf, 1024);

    for (x = 0, v = 0;  x < 1024;  x++)
    {
        if (ab == 0)
            v +=  buf[x]        & 0x0FFF;
        else
            v += (buf[x] >> 12) & 0x0FFF;
    }

    return v - 2048*1024;
}

static void ReadCalibrations (adc200me_privrec_t *me)
{
  int       nl;

  int32     DatM[2];
  int32     DatP[2];

    me->clb_Offs[0] = ReadOneCalibr(me, 0, 0) / 1024;
    me->clb_Offs[1] = ReadOneCalibr(me, 0, 1) / 1024;

    DatM[0]         = ReadOneCalibr(me, 1, 0);
    DatP[0]         = ReadOneCalibr(me, 2, 0);
    DatM[1]         = ReadOneCalibr(me, 3, 1);
    DatP[1]         = ReadOneCalibr(me, 4, 1);
    me->clb_DatS[0] = (DatP[0] - DatM[0]) / 1024;
    me->clb_DatS[1] = (DatP[1] - DatM[1]) / 1024;

    if (me->clb_DatS[0] == 0  ||  me->clb_DatS[1] == 0)
        DoDriverLog(me->devid, DRIVERLOG_ERR,
                    "BAD CALIBRATION sums: %d,%d",
                    me->clb_DatS[0], me->clb_DatS[1]);

    me->cur_args[ADC200ME_CHAN_CLB_STATE] = me->nxt_args[ADC200ME_CHAN_CLB_STATE] =
        me->clb_valid = 1;
    for (nl = 0;  nl < ADC200ME_NUM_LINES;  nl++)
    {
        me->cur_args[ADC200ME_CHAN_CLB_OFS1+nl] =
        me->nxt_args[ADC200ME_CHAN_CLB_OFS1+nl] =
        me->clb_Offs[                       nl];

        me->cur_args[ADC200ME_CHAN_CLB_COR1+nl] =
        me->nxt_args[ADC200ME_CHAN_CLB_COR1+nl] =
        me->clb_DatS[                       nl];
    }

    ReturnCalibrations(me);

#if 0
    DoDriverLog(DEVID_NOT_IN_DRIVER, 0,
                "NEW CALB: oA=%-6d oB=%-6d sA=%-6d sB=%-6d",
                me->clb_Offs[0], me->clb_Offs[1],
                me->clb_DatS[0], me->clb_DatS[1]);
#endif
}

static rflags_t ReadMeasurements(pzframe_drv_t *pdr)
{
  adc200me_privrec_t *me = PDR2ME(pdr);

  ADC200ME_DATUM_T   *w1;
  ADC200ME_DATUM_T   *w2;
  int32               q1;
  int32               q2;
  int                 ofs;
  int                 n;
  int                 count;
  int                 x;
  
  int32               csr;
  
  int32               buf[1024];

  int                 nl;
  ADC200ME_DATUM_T   *dp;
  ADC200ME_DATUM_T    v;
  ADC200ME_DATUM_T    min_v, max_v;
  int64               sum_v;

////fprintf(stderr, "%s\n", __FUNCTION__);
    ////me->lvmt->WriteBar0(me->handle, ADC200ME_REG_CSR, 0);
    csr = me->lvmt->ReadBar0(me->handle, ADC200ME_REG_CSR);

    /* Was it a calibration? */
    if (me->start_mask == ADC200ME_CSR_PC)
        ReadCalibrations(me);

    w1  = me->retdata;
    w2  = w1 + me->cur_args[ADC200ME_CHAN_NUMPTS];
    q1  = quants[me->cur_args[ADC200ME_CHAN_RANGE1]];
    q2  = quants[me->cur_args[ADC200ME_CHAN_RANGE2]];
    ofs = me->cur_args[ADC200ME_CHAN_PTSOFS] * sizeof(int32);

    for (n = me->cur_args[ADC200ME_CHAN_NUMPTS];
         n > 0;
         n -= count, ofs += count * sizeof(int32))
    {
        count = n;
        if (count > countof(buf))
            count = countof(buf);

        me->lvmt->ReadBar1(me->handle, ofs, buf, count);
        /* Do NOT try to apply calibration-correction to calibration data */
        if (me->clb_valid  &&  me->start_mask != ADC200ME_CSR_PC  &&
            me->clb_DatS[0] != 0  &&  me->clb_DatS[1] != 0)
            for (x = 0;  x < count;  x++)
            {
                *w1++ = (( buf[x]        & 0x0FFF) - 2048 - me->clb_Offs[0])
                        * ADC200ME_CLB_COR_R / me->clb_DatS[0]
                        * q1;
                *w2++ = (((buf[x] >> 12) & 0x0FFF) - 2048 - me->clb_Offs[1])
                        * ADC200ME_CLB_COR_R / me->clb_DatS[1]
                        * q2;
            }
        else
            for (x = 0;  x < count;  x++)
            {
                *w1++ = (( buf[x]        & 0x0FFF) - 2048) * q1;
                *w2++ = (((buf[x] >> 12) & 0x0FFF) - 2048) * q2;
            }

    }

    if (me->cur_args[ADC200ME_CHAN_CALC_STATS])
    {
        for (nl = 0, dp = me->retdata;  nl < ADC200ME_NUM_LINES;  nl++)
        {
            min_v = ADC200ME_MAX_VALUE; max_v = ADC200ME_MIN_VALUE; sum_v = 0;
            for (count = me->cur_args[ADC200ME_CHAN_NUMPTS];
                 count > 0;
                 count--, dp++)
            {
                v = *dp;
                if (min_v > v) min_v = v;
                if (max_v < v) max_v = v;
                sum_v += v;
            }
            me->cur_args[ADC200ME_CHAN_MIN1 + nl] =
            me->nxt_args[ADC200ME_CHAN_MIN1 + nl] = min_v;
            me->cur_args[ADC200ME_CHAN_MAX1 + nl] =
            me->nxt_args[ADC200ME_CHAN_MAX1 + nl] = max_v;
            me->cur_args[ADC200ME_CHAN_AVG1 + nl] =
            me->nxt_args[ADC200ME_CHAN_AVG1 + nl] = sum_v / me->cur_args[ADC200ME_CHAN_NUMPTS];
            me->cur_args[ADC200ME_CHAN_INT1 + nl] =
            me->nxt_args[ADC200ME_CHAN_INT1 + nl] = sum_v;
        }
    }

    ////fprintf(stderr, "CSR=%d\n", csr);

    return 0;
}

static void PrepareRetbufs     (pzframe_drv_t *pdr, rflags_t add_rflags)
{
  adc200me_privrec_t *me = PDR2ME(pdr);

  int                 n;
  int                 x;

    n = 0;

    /* 1. Device-specific: calc  */

    me->cur_args[ADC200ME_CHAN_XS_PER_POINT] =
        me->cur_args[ADC200ME_CHAN_TIMING] == ADC200ME_T_200MHZ ? 5 : 0;
    for (x = 0;  x < ADC200ME_NUM_LINES;  x++)
    {
        me->cur_args[ADC200ME_CHAN_LINE1ON       + x] = 1;
        me->cur_args[ADC200ME_CHAN_LINE1RANGEMIN + x] =
            (    0-2048) * quants[me->cur_args[ADC200ME_CHAN_RANGE1 + x] & 3];
        me->cur_args[ADC200ME_CHAN_LINE1RANGEMAX + x] =
            (0xFFF-2048) * quants[me->cur_args[ADC200ME_CHAN_RANGE1 + x] & 3];
    }

    /* 2. Device-specific: mark-for-returning */

    /* Optional calculated channels */
    if (me->cur_args[ADC200ME_CHAN_CALC_STATS])
        for (x = ADC200ME_CHAN_STATS_first;  x <= ADC200ME_CHAN_STATS_last; x++)
        {
            me->r.addrs [n] = x;
            me->r.dtypes[n] = CXDTYPE_INT32;
            me->r.nelems[n] = 1;
            me->r.val_ps[n] = me->cur_args + x;
            me->r.rflags[n] = 0;
            n++;
        }

    /* 3. General */

    /* Scalar STATUS channels */
    for (x = 0;  x < countof(chinfo);  x++)
        if (chinfo[x].chtype == PZFRAME_CHTYPE_STATUS)
        {
            if (chinfo[x].refchn >= 0) // Copy CUR value from respective control one
                me->cur_args[x] = me->cur_args[chinfo[x].refchn];
            me->nxt_args[x] = me->cur_args[x]; // For them to stay in place upon copy cur_args[]=nxt_args[]
            me->r.addrs [n] = x;
            me->r.dtypes[n] = CXDTYPE_INT32;
            me->r.nelems[n] = 1;
            me->r.val_ps[n] = me->cur_args + x;
            me->r.rflags[n] = 0;
            n++;
        }

    /* Vector channels, which were requested */
    if (me->data_rqd)
    {
        me->r.addrs [n] = ADC200ME_CHAN_DATA;
        me->r.dtypes[n] = ADC200ME_DTYPE;
        me->r.nelems[n] = me->cur_args[ADC200ME_CHAN_NUMPTS] * ADC200ME_NUM_LINES;
        me->r.val_ps[n] = me->retdata;
        me->r.rflags[n] = add_rflags;
        n++;
    }
    for (x = 0;  x < ADC200ME_NUM_LINES;  x++)
        if (me->line_rqd[x])
        {
            me->r.addrs [n] = ADC200ME_CHAN_LINE1 + x;
            me->r.dtypes[n] = ADC200ME_DTYPE;
            me->r.nelems[n] = me->cur_args[ADC200ME_CHAN_NUMPTS];
            me->r.val_ps[n] = me->retdata + me->cur_args[ADC200ME_CHAN_NUMPTS] * x;
            me->r.rflags[n] = add_rflags;
            n++;
        }
    /* A marker */
    me->r.addrs [n] = ADC200ME_CHAN_MARKER;
    me->r.dtypes[n] = CXDTYPE_INT32;
    me->r.nelems[n] = 1;
    me->r.val_ps[n] = me->cur_args + ADC200ME_CHAN_MARKER;
    me->r.rflags[n] = add_rflags;
    n++;
    /* ...and drop "requested" flags */
    me->data_rqd = 0;
    bzero(me->line_rqd, sizeof(me->line_rqd));

    /* Store retbufs data */
    me->pz.retbufs.count      = n;
    me->pz.retbufs.addrs      = me->r.addrs;
    me->pz.retbufs.dtypes     = me->r.dtypes;
    me->pz.retbufs.nelems     = me->r.nelems;
    me->pz.retbufs.values     = me->r.val_ps;
    me->pz.retbufs.rflags     = me->r.rflags;
    me->pz.retbufs.timestamps = NULL;
}

static void adc200me_rw_p (int devid, void *devptr,
                           int action,
                           int count, int *addrs,
                           cxdtype_t *dtypes, int *nelems, void **values)
{
  adc200me_privrec_t *me = devptr;

  int                 n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int                 chn;   // channel
  int                 ct;
  int32               val;   // Value

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];

        if (chn < 0  ||  chn >= countof(chinfo)  ||
            (ct = chinfo[chn].chtype) == PZFRAME_CHTYPE_UNSUPPORTED)
        {
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);
        }
        else if (ct == PZFRAME_CHTYPE_BIGC)
        {
            if (chn == ADC200ME_CHAN_DATA)
                me->data_rqd                          = 1;
            else
                me->line_rqd[chn-ADC200ME_CHAN_LINE1] = 1;

            pzframe_drv_req_mes(&(me->pz));
        }
        else if (ct == PZFRAME_CHTYPE_MARKER)
        {/* Ignore */}
        else
        {
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

            if      (ct == PZFRAME_CHTYPE_STATUS  ||  ct == PZFRAME_CHTYPE_AUTOUPDATED)
            {
                ////if (chn == ADC200ME_CHAN_CUR_NUMPTS) fprintf(stderr, "ADC200ME_CHAN_CUR_NUMPTS. =%d\n", me->cur_args[chn]);
                ReturnInt32Datum(devid, chn, me->cur_args[chn], 0);
            }
            else if (ct == PZFRAME_CHTYPE_VALIDATE)
            {
                if (action == DRVA_WRITE)
                    me->nxt_args[chn] = ValidateParam(&(me->pz), chn, val);
                ReturnInt32Datum(devid, chn, me->nxt_args[chn], 0);
            }
            else if (ct == PZFRAME_CHTYPE_INDIVIDUAL)
            {
                if      (chn == ADC200ME_CHAN_CALIBRATE)
                {
                     if (action == DRVA_WRITE)
                          me->nxt_args[chn] = (val != 0);
                     ReturnInt32Datum(devid, chn, me->nxt_args[chn], 0);
                }
                else if (chn == ADC200ME_CHAN_FGT_CLB)
                {
                    if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                        InvalidateCalibrations(me);
                    ReturnInt32Datum(devid, chn, 0, 0);
                }
            }
            else /*  ct == PZFRAME_CHTYPE_PZFRAME_STD, which also returns UPSUPPORTED for unknown */
                ////fprintf(stderr, "PZFRAME_STD(%d,%d)\n", chn, val),
                pzframe_drv_rw_p  (&(me->pz), chn,
                                   action == DRVA_WRITE? val : 0,
                                   action);
        }

 NEXT_CHANNEL:;
    }
}


enum
{
    PARAM_SHOT     = ADC200ME_CHAN_SHOT,
    PARAM_ISTART   = ADC200ME_CHAN_ISTART,
    PARAM_WAITTIME = ADC200ME_CHAN_WAITTIME,
    PARAM_STOP     = ADC200ME_CHAN_STOP,
    PARAM_ELAPSED  = ADC200ME_CHAN_ELAPSED,
};

#define FASTADC_NAME adc200me
#include "cpci_fastadc_common.h"
