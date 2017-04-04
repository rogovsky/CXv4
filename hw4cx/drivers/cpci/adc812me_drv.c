#include "cxsd_driver.h"
#include "pzframe_drv.h"
#include "pci4624_lyr.h"

#include "drv_i/adc812me_drv_i.h"


//////////////////////////////////////////////////////////////////////

enum
{
    ADC812ME_REG_RESERVE_00 = 0x00,
    ADC812ME_REG_PAGE_LEN   = 0x04,
    ADC812ME_REG_CSR        = 0x08,
    ADC812ME_REG_CUR_ADDR   = 0x0C,
    ADC812ME_REG_RESERVE_10 = 0x10,
    ADC812ME_REG_IRQS       = 0x14,
    ADC812ME_REG_RANGES     = 0x18,
    ADC812ME_REG_SERIAL     = 0x1C,
};

enum
{
    ADC812ME_CSR_RUN_shift     = 0,  ADC812ME_CSR_RUN       = 1 << ADC812ME_CSR_RUN_shift,
    ADC812ME_CSR_HLT_shift     = 1,  ADC812ME_CSR_HLT       = 1 << ADC812ME_CSR_HLT_shift,
    ADC812ME_CSR_STM_shift     = 2,  ADC812ME_CSR_STM       = 1 << ADC812ME_CSR_STM_shift,
    ADC812ME_CSR_Fn_shift      = 3,  ADC812ME_CSR_Fn        = 1 << ADC812ME_CSR_Fn_shift,
    ADC812ME_CSR_BUSY_shift    = 4,  ADC812ME_CSR_BUSY      = 1 << ADC812ME_CSR_BUSY_shift,
    ADC812ME_CSR_R5_SHIFT      = 5,
    ADC812ME_CSR_PC_shift      = 6,  ADC812ME_CSR_PC        = 1 << ADC812ME_CSR_PC_shift,
    ADC812ME_CSR_REC_shift     = 7,  ADC812ME_CSR_REC       = 1 << ADC812ME_CSR_REC_shift,
    ADC812ME_CSR_Fdiv_shift    = 8,  ADC812ME_CSR_Fdiv_MASK = 3 << ADC812ME_CSR_Fdiv_shift,
    ADC812ME_CSR_INT_ENA_shift = 16, ADC812ME_CSR_INT_ENA   = 1 << ADC812ME_CSR_INT_ENA_shift,
    ADC812ME_CSR_ZC_shift      = 17, ADC812ME_CSR_ZC        = 1 << ADC812ME_CSR_ZC_shift,
};

enum
{
    FASTADC_VENDOR_ID      = 0x4624,
    FASTADC_DEVICE_ID      = 0xADC2,
    FASTADC_SERIAL_OFFSET  = ADC812ME_REG_SERIAL,
    
    FASTADC_IRQ_CHK_OP     = PCI4624_OP_RD,
    FASTADC_IRQ_CHK_OFS    = ADC812ME_REG_IRQS,
    FASTADC_IRQ_CHK_MSK    = -1,
    FASTADC_IRQ_CHK_VAL    = 0,
    FASTADC_IRQ_CHK_COND   = PCI4624_COND_NONZERO,
    
    FASTADC_IRQ_RST_OP     = PCI4624_OP_WR,
    FASTADC_IRQ_RST_OFS    = ADC812ME_REG_IRQS,
    FASTADC_IRQ_RST_VAL    = 0,
    
    FASTADC_ON_CLS1_OP     = PCI4624_OP_WR,
    FASTADC_ON_CLS1_OFS    = ADC812ME_REG_CSR,
    FASTADC_ON_CLS1_VAL    = ADC812ME_CSR_HLT | ADC812ME_CSR_INT_ENA,
    
    FASTADC_ON_CLS2_OP     = PCI4624_OP_WR,
    FASTADC_ON_CLS2_OFS    = ADC812ME_REG_IRQS,
    FASTADC_ON_CLS2_VAL    = 0
};

static int32  quants       [4] = {4000, 2000, 1000, 500};
static int    timing_scales[2] = {250, 0}; // That's in fact the "exttim" factor
static int    frqdiv_scales[4] = {1, 2, 4, 8};



typedef int32 ADC812ME_DATUM_T;
enum { ADC812ME_DTYPE      = CXDTYPE_INT32 };

/* We allow only as much measurements, not ADC812ME_MAX_NUMPTS  */
enum { ADC812ME_MAX_ALLOWED_NUMPTS = 50000*0 + ADC812ME_MAX_NUMPTS };

//--------------------------------------------------------------------

static pzframe_chinfo_t chinfo[] =
{
    /* data */
    [ADC812ME_CHAN_DATA]          = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC812ME_CHAN_LINE0]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC812ME_CHAN_LINE1]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC812ME_CHAN_LINE2]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC812ME_CHAN_LINE3]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC812ME_CHAN_LINE4]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC812ME_CHAN_LINE5]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC812ME_CHAN_LINE6]         = {PZFRAME_CHTYPE_BIGC,        0},
    [ADC812ME_CHAN_LINE7]         = {PZFRAME_CHTYPE_BIGC,        0},

    [ADC812ME_CHAN_MARKER]        = {PZFRAME_CHTYPE_MARKER,      0},

    /* controls */
    [ADC812ME_CHAN_SHOT]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC812ME_CHAN_STOP]          = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC812ME_CHAN_ISTART]        = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC812ME_CHAN_WAITTIME]      = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC812ME_CHAN_CALIBRATE]     = {PZFRAME_CHTYPE_INDIVIDUAL,  0},
    [ADC812ME_CHAN_FGT_CLB]       = {PZFRAME_CHTYPE_INDIVIDUAL,  0},
    [ADC812ME_CHAN_VISIBLE_CLB]   = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC812ME_CHAN_CALC_STATS]    = {PZFRAME_CHTYPE_VALIDATE,    0},

    [ADC812ME_CHAN_PTSOFS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC812ME_CHAN_NUMPTS]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC812ME_CHAN_TIMING]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC812ME_CHAN_FRQDIV]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC812ME_CHAN_RANGE0]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC812ME_CHAN_RANGE1]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC812ME_CHAN_RANGE2]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC812ME_CHAN_RANGE3]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC812ME_CHAN_RANGE4]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC812ME_CHAN_RANGE5]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC812ME_CHAN_RANGE6]        = {PZFRAME_CHTYPE_VALIDATE,    0},
    [ADC812ME_CHAN_RANGE7]        = {PZFRAME_CHTYPE_VALIDATE,    0},

    /* status */
    [ADC812ME_CHAN_ELAPSED]       = {PZFRAME_CHTYPE_PZFRAME_STD, 0},
    [ADC812ME_CHAN_SERIAL]        = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_STATE]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_XS_PER_POINT]  = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_XS_DIVISOR]    = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_XS_FACTOR]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CUR_PTSOFS]    = {PZFRAME_CHTYPE_STATUS,      ADC812ME_CHAN_PTSOFS},
    [ADC812ME_CHAN_CUR_NUMPTS]    = {PZFRAME_CHTYPE_STATUS,      ADC812ME_CHAN_NUMPTS},
    [ADC812ME_CHAN_CUR_TIMING]    = {PZFRAME_CHTYPE_STATUS,      ADC812ME_CHAN_TIMING},
    [ADC812ME_CHAN_CUR_FRQDIV]    = {PZFRAME_CHTYPE_STATUS,      ADC812ME_CHAN_FRQDIV},
    [ADC812ME_CHAN_CUR_RANGE0]    = {PZFRAME_CHTYPE_STATUS,      ADC812ME_CHAN_RANGE0},
    [ADC812ME_CHAN_CUR_RANGE1]    = {PZFRAME_CHTYPE_STATUS,      ADC812ME_CHAN_RANGE1},
    [ADC812ME_CHAN_CUR_RANGE2]    = {PZFRAME_CHTYPE_STATUS,      ADC812ME_CHAN_RANGE2},
    [ADC812ME_CHAN_CUR_RANGE3]    = {PZFRAME_CHTYPE_STATUS,      ADC812ME_CHAN_RANGE3},
    [ADC812ME_CHAN_CUR_RANGE4]    = {PZFRAME_CHTYPE_STATUS,      ADC812ME_CHAN_RANGE4},
    [ADC812ME_CHAN_CUR_RANGE5]    = {PZFRAME_CHTYPE_STATUS,      ADC812ME_CHAN_RANGE5},
    [ADC812ME_CHAN_CUR_RANGE6]    = {PZFRAME_CHTYPE_STATUS,      ADC812ME_CHAN_RANGE6},
    [ADC812ME_CHAN_CUR_RANGE7]    = {PZFRAME_CHTYPE_STATUS,      ADC812ME_CHAN_RANGE7},

    [ADC812ME_CHAN_LINE0ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE1ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE2ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE3ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE4ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE5ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE6ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE7ON]       = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE0RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE1RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE2RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE3RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE4RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE5RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE6RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE7RANGEMIN] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE0RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE1RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE2RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE3RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE4RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE5RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE6RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},
    [ADC812ME_CHAN_LINE7RANGEMAX] = {PZFRAME_CHTYPE_STATUS,      -1},

    [ADC812ME_CHAN_CLB_OFS0]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_OFS1]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_OFS2]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_OFS3]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_OFS4]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_OFS5]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_OFS6]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_OFS7]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_COR0]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_COR1]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_COR2]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_COR3]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_COR4]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_COR5]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_COR6]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_CLB_COR7]      = {PZFRAME_CHTYPE_AUTOUPDATED, 0},

    [ADC812ME_CHAN_MIN0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MIN1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MIN2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MIN3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MIN4]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MIN5]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MIN6]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MIN7]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MAX0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MAX1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MAX2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MAX3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MAX4]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MAX5]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MAX6]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_MAX7]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_AVG0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_AVG1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_AVG2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_AVG3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_AVG4]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_AVG5]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_AVG6]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_AVG7]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_INT0]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_INT1]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_INT2]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_INT3]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_INT4]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_INT5]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_INT6]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_INT7]          = {PZFRAME_CHTYPE_AUTOUPDATED, 0},

    [ADC812ME_CHAN_LINE0TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE1TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE2TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE3TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE4TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE5TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE6TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE7TOTALMIN] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE0TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE1TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE2TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE3TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE4TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE5TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE6TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_LINE7TOTALMAX] = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
    [ADC812ME_CHAN_NUM_LINES]     = {PZFRAME_CHTYPE_AUTOUPDATED, 0},
};

//--------------------------------------------------------------------

typedef struct
{
    pzframe_drv_t      pz;
    int                devid;
    int                handle;
    pci4624_vmt_t     *lvmt;

    int                serial;

    int32              cur_args[ADC812ME_NUMCHANS];
    int32              nxt_args[ADC812ME_NUMCHANS];
    int32              prv_args[ADC812ME_NUMCHANS];
    ADC812ME_DATUM_T   retdata [ADC812ME_MAX_ALLOWED_NUMPTS*ADC812ME_NUM_LINES];
    int                do_return;
    int                force_read;
    pzframe_retbufs_t  retbufs;

    // This holds CSR value WITHOUT RUN/PC bits, i.e. -- effective SETTINGS only
    int32              cword;
    // This holds either ADC812ME_CSR_RUN or ADC812ME_CSR_PC
    int32              start_mask;

    int                clb_valid;
    int32              clb_Offs[ADC812ME_NUM_LINES];
    int32              clb_DatS[ADC812ME_NUM_LINES];

    int                data_rqd;
    int                line_rqd[ADC812ME_NUM_LINES];
    struct
    {
        int            addrs [ADC812ME_NUMCHANS];
        cxdtype_t      dtypes[ADC812ME_NUMCHANS];
        int            nelems[ADC812ME_NUMCHANS];
        void          *val_ps[ADC812ME_NUMCHANS];
        rflags_t       rflags[ADC812ME_NUMCHANS];
    }                  r;
} adc812me_privrec_t;

#define PDR2ME(pdr) ((adc812me_privrec_t*)pdr) //!!! Should better subtract offsetof(pz)

//--------------------------------------------------------------------

static psp_lkp_t adc812me_timing_lkp[] =
{
    {"4",   ADC812ME_T_4MHZ},
    {"int", ADC812ME_T_4MHZ},
    {"ext", ADC812ME_T_EXT},
    {NULL, 0}
};

static psp_lkp_t adc812me_frqdiv_lkp[] =
{
    {"1",    ADC812ME_FRQD_250NS},
    {"250",  ADC812ME_FRQD_250NS},
    {"2",    ADC812ME_FRQD_500NS},
    {"500",  ADC812ME_FRQD_500NS},
    {"4",    ADC812ME_FRQD_1000NS},
    {"1000", ADC812ME_FRQD_1000NS},
    {"8",    ADC812ME_FRQD_2000NS},
    {"2000", ADC812ME_FRQD_2000NS},
    {NULL, 0}
};

static psp_lkp_t adc812me_range_lkp [] =
{
    {"8192", ADC812ME_R_8V},
    {"4096", ADC812ME_R_4V},
    {"2048", ADC812ME_R_2V},
    {"1024", ADC812ME_R_1V},
    {NULL, 0}
};

static psp_paramdescr_t adc812me_params[] =
{
    PSP_P_INT    ("ptsofs",   adc812me_privrec_t, nxt_args[ADC812ME_CHAN_PTSOFS],   -1, 0, ADC812ME_MAX_ALLOWED_NUMPTS-1),
    PSP_P_INT    ("numpts",   adc812me_privrec_t, nxt_args[ADC812ME_CHAN_NUMPTS],   -1, 1, ADC812ME_MAX_ALLOWED_NUMPTS),
    PSP_P_LOOKUP ("timing",   adc812me_privrec_t, nxt_args[ADC812ME_CHAN_TIMING],   -1, adc812me_timing_lkp),
    PSP_P_LOOKUP ("frqdiv",   adc812me_privrec_t, nxt_args[ADC812ME_CHAN_FRQDIV],   -1, adc812me_frqdiv_lkp),
    PSP_P_LOOKUP ("range0A",  adc812me_privrec_t, nxt_args[ADC812ME_CHAN_RANGE0],   -1, adc812me_range_lkp),
    PSP_P_LOOKUP ("range0B",  adc812me_privrec_t, nxt_args[ADC812ME_CHAN_RANGE1],   -1, adc812me_range_lkp),
    PSP_P_LOOKUP ("range1A",  adc812me_privrec_t, nxt_args[ADC812ME_CHAN_RANGE2],   -1, adc812me_range_lkp),
    PSP_P_LOOKUP ("range1B",  adc812me_privrec_t, nxt_args[ADC812ME_CHAN_RANGE3],   -1, adc812me_range_lkp),
    PSP_P_LOOKUP ("range2A",  adc812me_privrec_t, nxt_args[ADC812ME_CHAN_RANGE4],   -1, adc812me_range_lkp),
    PSP_P_LOOKUP ("range2B",  adc812me_privrec_t, nxt_args[ADC812ME_CHAN_RANGE5],   -1, adc812me_range_lkp),
    PSP_P_LOOKUP ("range3A",  adc812me_privrec_t, nxt_args[ADC812ME_CHAN_RANGE6],   -1, adc812me_range_lkp),
    PSP_P_LOOKUP ("range3B",  adc812me_privrec_t, nxt_args[ADC812ME_CHAN_RANGE7],   -1, adc812me_range_lkp),
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
    SAVED_DATA_COUNT  = countof(saved_data_signature) + ADC812ME_NUMCHANS,
    SAVED_DATA_OFFSET = (1 << 22) - SAVED_DATA_COUNT * sizeof(int32),
};

static int32 MakeCWord(int TIMING, int FRQDIV)
{
    return
        0 /* RUN=0 */
        |
        0 /* HLT=0 -- sure! */
        |
        ADC812ME_CSR_STM
        |
        ((TIMING == ADC812ME_T_EXT)? ADC812ME_CSR_Fn : 0)
        |
        0 /* PC=0 */
        |
        0 /* REC=0 */
        |
        (FRQDIV << ADC812ME_CSR_Fdiv_shift)
        |
        0 /* INT_ENA=0 */
        ;
}

static void ReturnCalibrations(adc812me_privrec_t *me)
{
  int  nl;

    ReturnInt32Datum(me->devid,   ADC812ME_CHAN_CLB_STATE,
                     me->cur_args[ADC812ME_CHAN_CLB_STATE], 0);
    for (nl = 0;  nl < ADC812ME_NUM_LINES;  nl++)
    {
        ReturnInt32Datum(me->devid,   ADC812ME_CHAN_CLB_OFS0+nl,
                         me->cur_args[ADC812ME_CHAN_CLB_OFS0+nl], 0);
        ReturnInt32Datum(me->devid,   ADC812ME_CHAN_CLB_COR0+nl,
                         me->cur_args[ADC812ME_CHAN_CLB_COR0+nl], 0);
    }
}

static void InvalidateCalibrations(adc812me_privrec_t *me)
{
  int  nl;

    me->cur_args[ADC812ME_CHAN_CLB_STATE] =
    me->nxt_args[ADC812ME_CHAN_CLB_STATE] =
        me->clb_valid   = 0;
    for (nl = 0;  nl < ADC812ME_NUM_LINES;  nl++)
    {
        me->cur_args[ADC812ME_CHAN_CLB_OFS0+nl] =
        me->nxt_args[ADC812ME_CHAN_CLB_OFS0+nl] =
        me->clb_Offs[                       nl] = 0;

        me->cur_args[ADC812ME_CHAN_CLB_COR0+nl] =
        me->nxt_args[ADC812ME_CHAN_CLB_COR0+nl] =
        me->clb_DatS[                       nl] = ADC812ME_CLB_COR_R;
    }

    ReturnCalibrations(me);
}

static int32 ValidateParam(pzframe_drv_t *pdr, int n, int32 v)
{
  adc812me_privrec_t *me = PDR2ME(pdr);

    if      (n == ADC812ME_CHAN_TIMING)
    {
        if (v < ADC812ME_T_4MHZ) v = ADC812ME_T_4MHZ;
        if (v > ADC812ME_T_EXT)  v = ADC812ME_T_EXT;
    }
    else if (n == ADC812ME_CHAN_FRQDIV)
    {
        if (v < ADC812ME_FRQD_250NS)  v = ADC812ME_FRQD_250NS;
        if (v > ADC812ME_FRQD_2000NS) v = ADC812ME_FRQD_2000NS;
    }
    else if (n >= ADC812ME_CHAN_RANGE0  &&  n <= ADC812ME_CHAN_RANGE7)
    {
        if (v < ADC812ME_R_8V) v = ADC812ME_R_8V;
        if (v > ADC812ME_R_1V) v = ADC812ME_R_1V;
    }
    else if (n == ADC812ME_CHAN_PTSOFS)
    {
        if (v < 0)                             v = 0;
        if (v > ADC812ME_MAX_ALLOWED_NUMPTS-1) v = ADC812ME_MAX_ALLOWED_NUMPTS-1;
    }
    else if (n == ADC812ME_CHAN_NUMPTS)
    {
        if (v < 1)                             v = 1;
        if (v > ADC812ME_MAX_ALLOWED_NUMPTS)   v = ADC812ME_MAX_ALLOWED_NUMPTS;
    }

    return v;
}

static void Return1Param(adc812me_privrec_t *me, int n, int32 v)
{
    ReturnInt32Datum(me->devid, n, me->cur_args[n] = me->nxt_args[n] = v, 0);
}

static void Init1Param(adc812me_privrec_t *me, int n, int32 v)
{
    if (me->nxt_args[n] < 0) me->nxt_args[n] = v;
}
static int   InitParams(pzframe_drv_t *pdr)
{
  adc812me_privrec_t *me = PDR2ME(pdr);

  int                 n;
  int32               saved_data[SAVED_DATA_COUNT];

    for (n = 0;  n < countof(chinfo);  n++)
        if (chinfo[n].chtype == PZFRAME_CHTYPE_AUTOUPDATED  ||
            chinfo[n].chtype == PZFRAME_CHTYPE_STATUS)
            SetChanReturnType(me->devid, n, 1, IS_AUTOUPDATED_TRUSTED);

    /* HLT device first */
    me->lvmt->WriteBar0(me->handle, ADC812ME_REG_CSR,  ADC812ME_CSR_HLT);
    /* Drop IRQ */
    me->lvmt->WriteBar0(me->handle, ADC812ME_REG_IRQS, 0);
    /* Read previous-settings-storage area */
    me->lvmt->ReadBar1 (me->handle, SAVED_DATA_OFFSET, saved_data, SAVED_DATA_COUNT);

    /* Try to use previous settings, otherwise use defaults */
    if (memcmp(saved_data, saved_data_signature, sizeof(saved_data_signature)) == 0)
    {
        memcpy(me->nxt_args,
               saved_data + countof(saved_data_signature),
               sizeof(me->nxt_args[0]) * ADC812ME_NUMCHANS);
        for (n = 0;  n < ADC812ME_NUMCHANS;  n++)
            if (n < countof(chinfo)  &&
                chinfo[n].chtype == PZFRAME_CHTYPE_VALIDATE)
                me->nxt_args[n] = ValidateParam(pdr, n, me->nxt_args[n]);
            else
                me->nxt_args[n] = 0;
    }
    else
    {
        Init1Param(me, ADC812ME_CHAN_PTSOFS, 0);
        Init1Param(me, ADC812ME_CHAN_NUMPTS, 1024);
        Init1Param(me, ADC812ME_CHAN_TIMING, ADC812ME_T_4MHZ);
        Init1Param(me, ADC812ME_CHAN_FRQDIV, ADC812ME_FRQD_250NS);
        for (n = 0;  n < ADC812ME_NUM_LINES; n++)
            Init1Param(me, ADC812ME_CHAN_RANGE0 + n, ADC812ME_R_8V);
        me->nxt_args[ADC812ME_CHAN_CALIBRATE] = 0;
    }

    InvalidateCalibrations(me);

    me->cword = MakeCWord(me->nxt_args[ADC812ME_CHAN_TIMING],
                          me->nxt_args[ADC812ME_CHAN_FRQDIV]);
    me->lvmt->WriteBar0(me->handle, ADC812ME_REG_CSR, me->cword);

    Return1Param(me, ADC812ME_CHAN_SERIAL,
                 me->serial =
                 me->lvmt->ReadBar0(me->handle, ADC812ME_REG_SERIAL));
    DoDriverLog(me->devid, 0, "serial=%d", me->serial);
    Return1Param(me, ADC812ME_CHAN_XS_FACTOR,  -9);

    for (n = 0;  n < ADC812ME_NUM_LINES;  n++)
    {
        Return1Param(me, ADC812ME_CHAN_LINE0TOTALMIN + n, ADC812ME_MIN_VALUE);
        Return1Param(me, ADC812ME_CHAN_LINE0TOTALMAX + n, ADC812ME_MAX_VALUE);
    }
    Return1Param    (me, ADC812ME_CHAN_NUM_LINES,         ADC812ME_NUM_LINES);

    SetChanRDs(me->devid, ADC812ME_CHAN_DATA, 1+ADC812ME_NUM_LINES, 1000000.0, 0.0);
    SetChanRDs(me->devid, ADC812ME_CHAN_STATS_first,
                          ADC812ME_CHAN_STATS_last-ADC812ME_CHAN_STATS_first+1,
                                                                    1000000.0, 0.0);
    SetChanRDs(me->devid, ADC812ME_CHAN_ELAPSED, 1, 1000.0, 0.0);

    return DEVSTATE_OPERATING;
}

static int  StartMeasurements(pzframe_drv_t *pdr)
{
  adc812me_privrec_t *me = PDR2ME(pdr);

  int                 nl;
  int32               ranges;

    /* "Actualize" requested parameters */
    memcpy(me->cur_args, me->nxt_args, sizeof(me->cur_args));

    /* Zeroth step: check PTSOFS against NUMPTS */
    if (me->cur_args    [ADC812ME_CHAN_PTSOFS] > ADC812ME_MAX_ALLOWED_NUMPTS - me->cur_args[ADC812ME_CHAN_NUMPTS])
        Return1Param(me, ADC812ME_CHAN_PTSOFS,   ADC812ME_MAX_ALLOWED_NUMPTS - me->cur_args[ADC812ME_CHAN_NUMPTS]);

    /* Check if calibration-affecting parameters had changed */
    if (me->cur_args[ADC812ME_CHAN_TIMING] != me->prv_args[ADC812ME_CHAN_TIMING])
        InvalidateCalibrations(me);

    /* Store parameters for future comparison */
    memcpy(me->prv_args, me->cur_args, sizeof(me->prv_args));

    /* Stop/init the device first */
    me->lvmt->WriteBar0(me->handle, ADC812ME_REG_CSR, ADC812ME_CSR_HLT);
    me->lvmt->WriteBar0(me->handle, ADC812ME_REG_CSR, 0);

    /* Now program various parameters */
    me->lvmt->WriteBar0(me->handle, ADC812ME_REG_PAGE_LEN,
                        me->cur_args[ADC812ME_CHAN_NUMPTS] + me->cur_args[ADC812ME_CHAN_PTSOFS]);
    for (nl = 0, ranges = 0;
         nl < ADC812ME_NUM_LINES;
         nl++)
        ranges |= ((me->cur_args[ADC812ME_CHAN_RANGE0 + nl] & 3) << (nl * 2));
    me->lvmt->WriteBar0(me->handle, ADC812ME_REG_RANGES, ranges);

    /* And, finally, the CSR */
    me->cword = MakeCWord(me->cur_args[ADC812ME_CHAN_TIMING],
                          me->cur_args[ADC812ME_CHAN_FRQDIV]);
    me->lvmt->WriteBar0(me->handle, ADC812ME_REG_CSR, me->cword);

    if ((me->cur_args[ADC812ME_CHAN_CALIBRATE] & CX_VALUE_LIT_MASK) != 0)
    {
        me->start_mask = ADC812ME_CSR_PC;
        Return1Param(me, ADC812ME_CHAN_CALIBRATE, 0);
    }
    else
        me->start_mask = ADC812ME_CSR_RUN;

    /* Save settings in a previous-settings-storage area */
    me->lvmt->WriteBar1(me->handle,
                        SAVED_DATA_OFFSET + sizeof(saved_data_signature),
                        me->cur_args,
                        ADC812ME_NUMCHANS);
    me->lvmt->WriteBar1(me->handle,
                        SAVED_DATA_OFFSET,
                        saved_data_signature,
                        countof(saved_data_signature));
    
    me->do_return =
        (me->start_mask != ADC812ME_CSR_PC  ||
         (me->cur_args[ADC812ME_CHAN_VISIBLE_CLB] & CX_VALUE_LIT_MASK) != 0);
    me->force_read = me->start_mask == ADC812ME_CSR_PC;

    /* Let's go! */
    me->lvmt->WriteBar0(me->handle, ADC812ME_REG_CSR, me->cword | me->start_mask);

    return PZFRAME_R_DOING;
}

static int  TrggrMeasurements(pzframe_drv_t *pdr)
{
  adc812me_privrec_t *me = PDR2ME(pdr);

    /* Don't even try anything in CALIBRATE mode */
    if (me->start_mask == ADC812ME_CSR_PC) return PZFRAME_R_DOING;

    /* 0. HLT device first (that is safe and don't touch any other registers) */
    me->lvmt->WriteBar0(me->handle, ADC812ME_REG_CSR, ADC812ME_CSR_HLT);

    /* 1. Allow prog-start */
    me->cword &=~ ADC812ME_CSR_STM;
    me->lvmt->WriteBar0(me->handle, ADC812ME_REG_CSR, me->cword);
    /* 2. Do prog-start */
    me->lvmt->WriteBar0(me->handle, ADC812ME_REG_CSR, me->cword | ADC812ME_CSR_RUN);

    return PZFRAME_R_DOING;
}

static int  AbortMeasurements(pzframe_drv_t *pdr)
{
  adc812me_privrec_t *me = PDR2ME(pdr);
  int                 n;

    me->cword &=~ ADC812ME_CSR_STM;
    me->lvmt->WriteBar0(me->handle, ADC812ME_REG_CSR, me->cword |  ADC812ME_CSR_HLT);
    me->lvmt->WriteBar0(me->handle, ADC812ME_REG_CSR, me->cword);

    if (me->cur_args[ADC812ME_CHAN_NUMPTS] != 0)
        bzero(me->retdata,
              sizeof(me->retdata[0])
              *
              me->cur_args[ADC812ME_CHAN_NUMPTS] * ADC812ME_NUM_LINES);

    for (n = ADC812ME_CHAN_STATS_first;  n <= ADC812ME_CHAN_STATS_last;  n++)
        me->cur_args[n] = me->nxt_args[n] = 0;

    return PZFRAME_R_READY;
}

enum
{
    INT32S_PER_POINT = 4,
    BYTES_PER_POINT  = sizeof(int32) * INT32S_PER_POINT
};

static int32 CalcOneSum(int32 *buf, int block, int nl)
{
  int    cofs = nl / 2;
  int    ab   = nl % 2;
  int32 *rp   = buf + 
                (100 + block * 500) * INT32S_PER_POINT
                + cofs;
  int    x;
  int32  v;

    for (x = 0, v = 0; 
         x < 300;
         x++)
    {
        if (ab == 0)
            v +=  rp[x * INT32S_PER_POINT]        & 0x0FFF;
        else
            v += (rp[x * INT32S_PER_POINT] >> 12) & 0x0FFF;
    }

    return v - 2048*300;
}

static void ReadCalibrations (adc812me_privrec_t *me)
{
  int       nl;

  rflags_t  rflags = 0;

  int32     buf[INT32S_PER_POINT * 1000];

    me->lvmt->ReadBar1(me->handle, 0, buf, countof(buf));

    for (nl = 0;  nl < ADC812ME_NUM_LINES;  nl++)
    {
        me->clb_Offs[nl] = CalcOneSum(buf, 0, nl) / 300;
        me->clb_DatS[nl] = CalcOneSum(buf, 1, nl) / 300;
        //fprintf(stderr, "%d: %d,%d\n", nl, me->clb_Offs[nl], me->clb_DatS[nl]);
        if (me->clb_DatS[nl] == 0)
            DoDriverLog(me->devid, DRIVERLOG_ERR,
                        "BAD CALIBRATION sum[%d]=%d",
                        nl, me->clb_DatS[nl]);
    }

    me->cur_args[ADC812ME_CHAN_CLB_STATE] = me->nxt_args[ADC812ME_CHAN_CLB_STATE] =
        me->clb_valid = 1;
    for (nl = 0;  nl < ADC812ME_NUM_LINES;  nl++)
    {
        me->cur_args[ADC812ME_CHAN_CLB_OFS0+nl] =
        me->nxt_args[ADC812ME_CHAN_CLB_OFS0+nl] =
        me->clb_Offs[                       nl];

        me->cur_args[ADC812ME_CHAN_CLB_COR0+nl] =
        me->nxt_args[ADC812ME_CHAN_CLB_COR0+nl] =
        me->clb_DatS[                       nl];
    }

    ReturnCalibrations(me);
}

static rflags_t ReadMeasurements(pzframe_drv_t *pdr)
{
  adc812me_privrec_t *me = PDR2ME(pdr);

  int                 nl;
  int                 clb_nz;
  ADC812ME_DATUM_T   *wp[ADC812ME_NUM_LINES];
  int32               q [ADC812ME_NUM_LINES];
  int                 ofs;
  int                 n;
  int                 count;
  int                 x;
  int32              *rp;
  int32               rd;
  
  int32               csr;

  int32               buf[INT32S_PER_POINT * 1024]; // Array count must be multiple of 4; 1024%4==0

  //int                 nl;
  ADC812ME_DATUM_T   *dp;
  ADC812ME_DATUM_T    v;
  ADC812ME_DATUM_T    min_v, max_v;
  int64               sum_v;

    ////me->lvmt->WriteBar0(me->handle, ADC812ME_REG_CSR, 0);
    csr = me->lvmt->ReadBar0(me->handle, ADC812ME_REG_CSR);

    /* Was it a calibration? */
    if (me->start_mask == ADC812ME_CSR_PC)
        ReadCalibrations(me);

    for (nl = 0;  nl < ADC812ME_NUM_LINES;  nl++)
    {
        wp[nl] = me->retdata + me->cur_args[ADC812ME_CHAN_NUMPTS] * nl;
        q [nl] = quants[me->cur_args[ADC812ME_CHAN_RANGE0 + nl] & 3];
    }
    ofs = me->cur_args[ADC812ME_CHAN_PTSOFS] * BYTES_PER_POINT;

    /* Ensure that none of sums ==0 */
    for (nl = 0, clb_nz = 1;  nl < ADC812ME_NUM_LINES;  nl++)
        if (me->clb_DatS[nl] == 0) clb_nz = 0;

    for (n = me->cur_args[ADC812ME_CHAN_NUMPTS];
         n > 0;
         n -= count, ofs += count * BYTES_PER_POINT)
    {
        count = n;
        if (count > countof(buf) / INT32S_PER_POINT)
            count = countof(buf) / INT32S_PER_POINT;

        me->lvmt->ReadBar1(me->handle, ofs, buf, count * INT32S_PER_POINT);
        /* Do NOT try to apply calibration-correction to calibration data */
        if (me->clb_valid  &&  me->start_mask != ADC812ME_CSR_PC  &&
            clb_nz)
            for (x = 0, rp = buf;  x < count;  x++)
            {
                rd = *rp++;
                *(wp[0]++) = (( rd        & 0x0FFF) - 2048 - me->clb_Offs[0])
                             * ADC812ME_CLB_COR_R / me->clb_DatS[0]
                             * q[0];
                *(wp[1]++) = (((rd >> 12) & 0x0FFF) - 2048 - me->clb_Offs[1])
                             * ADC812ME_CLB_COR_R / me->clb_DatS[1]
                             * q[1];
                rd = *rp++;
                *(wp[2]++) = (( rd        & 0x0FFF) - 2048 - me->clb_Offs[2])
                             * ADC812ME_CLB_COR_R / me->clb_DatS[2]
                             * q[2];
                *(wp[3]++) = (((rd >> 12) & 0x0FFF) - 2048 - me->clb_Offs[3])
                             * ADC812ME_CLB_COR_R / me->clb_DatS[3]
                             * q[3];
                rd = *rp++;
                *(wp[4]++) = (( rd        & 0x0FFF) - 2048 - me->clb_Offs[4])
                             * ADC812ME_CLB_COR_R / me->clb_DatS[4]
                             * q[4];
                *(wp[5]++) = (((rd >> 12) & 0x0FFF) - 2048 - me->clb_Offs[5])
                             * ADC812ME_CLB_COR_R / me->clb_DatS[5]
                             * q[5];
                rd = *rp++;
                *(wp[6]++) = (( rd        & 0x0FFF) - 2048 - me->clb_Offs[6])
                             * ADC812ME_CLB_COR_R / me->clb_DatS[6]
                             * q[6];
                *(wp[7]++) = (((rd >> 12) & 0x0FFF) - 2048 - me->clb_Offs[7])
                             * ADC812ME_CLB_COR_R / me->clb_DatS[7]
                             * q[7];
            }
        else
            for (x = 0, rp = buf;  x < count;  x++)
            {
                rd = *rp++;
                *(wp[0]++) = (( rd        & 0x0FFF) - 2048) * q[0];
                *(wp[1]++) = (((rd >> 12) & 0x0FFF) - 2048) * q[1];
                rd = *rp++;
                *(wp[2]++) = (( rd        & 0x0FFF) - 2048) * q[2];
                *(wp[3]++) = (((rd >> 12) & 0x0FFF) - 2048) * q[3];
                rd = *rp++;
                *(wp[4]++) = (( rd        & 0x0FFF) - 2048) * q[4];
                *(wp[5]++) = (((rd >> 12) & 0x0FFF) - 2048) * q[5];
                rd = *rp++;
                *(wp[6]++) = (( rd        & 0x0FFF) - 2048) * q[6];
                *(wp[7]++) = (((rd >> 12) & 0x0FFF) - 2048) * q[7];
            }
    }
    
    if (me->cur_args[ADC812ME_CHAN_CALC_STATS])
    {
        for (nl = 0, dp = me->retdata;  nl < ADC812ME_NUM_LINES;  nl++)
        {
            min_v = ADC812ME_MAX_VALUE; max_v = ADC812ME_MIN_VALUE; sum_v = 0;
            for (count = me->cur_args[ADC812ME_CHAN_NUMPTS];
                 count > 0;
                 count--, dp++)
            {
                v = *dp;
                if (min_v > v) min_v = v;
                if (max_v < v) max_v = v;
                sum_v += v;
            }
            me->cur_args[ADC812ME_CHAN_MIN0 + nl] =
            me->nxt_args[ADC812ME_CHAN_MIN0 + nl] = min_v;
            me->cur_args[ADC812ME_CHAN_MAX0 + nl] =
            me->nxt_args[ADC812ME_CHAN_MAX0 + nl] = max_v;
            me->cur_args[ADC812ME_CHAN_AVG0 + nl] =
            me->nxt_args[ADC812ME_CHAN_AVG0 + nl] = sum_v / me->cur_args[ADC812ME_CHAN_NUMPTS];
            me->cur_args[ADC812ME_CHAN_INT0 + nl] =
            me->nxt_args[ADC812ME_CHAN_INT0 + nl] = sum_v;
        }
    }

    ////fprintf(stderr, "CSR=%d\n", csr);

    return 0;
}

static void PrepareRetbufs     (pzframe_drv_t *pdr, rflags_t add_rflags)
{
  adc812me_privrec_t *me = PDR2ME(pdr);

  int                 n;
  int                 x;

    n = 0;

    /* 1. Device-specific: calc  */

    me->cur_args[ADC812ME_CHAN_XS_PER_POINT] =
        1000 * timing_scales[me->cur_args[ADC812ME_CHAN_TIMING] & 1];
    me->cur_args[ADC812ME_CHAN_XS_DIVISOR]   =
        1000 / frqdiv_scales[me->cur_args[ADC812ME_CHAN_FRQDIV] & 3];
    for (x = 0;  x < ADC812ME_NUM_LINES;  x++)
    {
        me->cur_args[ADC812ME_CHAN_LINE0ON       + x] = 1;
        me->cur_args[ADC812ME_CHAN_LINE0RANGEMIN + x] =
            (    0-2048) * quants[me->cur_args[ADC812ME_CHAN_RANGE0 + x] & 3];
        me->cur_args[ADC812ME_CHAN_LINE0RANGEMAX + x] =
            (0xFFF-2048) * quants[me->cur_args[ADC812ME_CHAN_RANGE0 + x] & 3];
    }

    /* 2. Device-specific: mark-for-returning */

    /* Optional calculated channels */
    if (me->cur_args[ADC812ME_CHAN_CALC_STATS])
        for (x = ADC812ME_CHAN_STATS_first;  x <= ADC812ME_CHAN_STATS_last; x++)
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
        me->r.addrs [n] = ADC812ME_CHAN_DATA;
        me->r.dtypes[n] = ADC812ME_DTYPE;
        me->r.nelems[n] = me->cur_args[ADC812ME_CHAN_NUMPTS] * ADC812ME_NUM_LINES;
        me->r.val_ps[n] = me->retdata;
        me->r.rflags[n] = add_rflags;
        n++;
    }
    for (x = 0;  x < ADC812ME_NUM_LINES;  x++)
        if (me->line_rqd[x])
        {
            me->r.addrs [n] = ADC812ME_CHAN_LINE0 + x;
            me->r.dtypes[n] = ADC812ME_DTYPE;
            me->r.nelems[n] = me->cur_args[ADC812ME_CHAN_NUMPTS];
            me->r.val_ps[n] = me->retdata + me->cur_args[ADC812ME_CHAN_NUMPTS] * x;
            me->r.rflags[n] = add_rflags;
            n++;
        }
    /* A marker */
    me->r.addrs [n] = ADC812ME_CHAN_MARKER;
    me->r.dtypes[n] = CXDTYPE_INT32;
    me->r.nelems[n] = 1;
    me->r.val_ps[n] = me->cur_args + ADC812ME_CHAN_MARKER;
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

static void adc812me_rw_p (int devid, void *devptr,
                           int action,
                           int count, int *addrs,
                           cxdtype_t *dtypes, int *nelems, void **values)
{
  adc812me_privrec_t *me = devptr;

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
            if (chn == ADC812ME_CHAN_DATA)
                me->data_rqd                          = 1;
            else
                me->line_rqd[chn-ADC812ME_CHAN_LINE0] = 1;

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
                ////if (chn == ADC812ME_CHAN_CUR_NUMPTS) fprintf(stderr, "ADC812ME_CHAN_CUR_NUMPTS. =%d\n", me->cur_args[chn]);
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
                if      (chn == ADC812ME_CHAN_CALIBRATE)
                {
                     if (action == DRVA_WRITE)
                          me->nxt_args[chn] = (val != 0);
                     ReturnInt32Datum(devid, chn, me->nxt_args[chn], 0);
                }
                else if (chn == ADC812ME_CHAN_FGT_CLB)
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
    PARAM_SHOT     = ADC812ME_CHAN_SHOT,
    PARAM_ISTART   = ADC812ME_CHAN_ISTART,
    PARAM_WAITTIME = ADC812ME_CHAN_WAITTIME,
    PARAM_STOP     = ADC812ME_CHAN_STOP,
    PARAM_ELAPSED  = ADC812ME_CHAN_ELAPSED,
};

#define FASTADC_NAME adc812me
#include "cpci_fastadc_common.h"
