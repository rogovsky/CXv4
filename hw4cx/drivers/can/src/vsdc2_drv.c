#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/vsdc2_drv_i.h"


/*=== VSDC2 specifics ==============================================*/

enum
{
    DEVTYPE = 0x80
};

/* Map of registers */
enum
{
    /* General-purpose registers */
    VSDC2_R_ID               = 0x00,
    VSDC2_R_GCSR             = 0x01,
    VSDC2_R_CALIB_OFFSET_NUM = 0x02,
    VSDC2_R_UART0_BAUDRATE   = 0x03,
    VSDC2_R_UART0_DATA       = 0x04,
    VSDC2_R_UART0_NUM        = 0x05,
    VSDC2_R_UART1_BAUDRATE   = 0x06,
    VSDC2_R_UART1_DATA       = 0x07,
    VSDC2_R_UART1_NUM        = 0x08,
    VSDC2_R_CAN_CSR          = 0x09,
    VSDC2_R_GPIO             = 0x0A,
    VSDC2_R_IRQ_MASK         = 0x0B,
    VSDC2_R_IRQ_STATUS       = 0x0C,
    VSDC2_R_ADC0_CSR         = 0x0D,
    VSDC2_R_ADC0_TIMER       = 0x0E,
    VSDC2_R_ADC0_AVG_NUM     = 0x0F,
    VSDC2_R_ADC0_WRITE_ADDR  = 0x10,
    VSDC2_R_ADC0_READ_ADDR   = 0x11,
    VSDC2_R_ADC0_DATA        = 0x12,
    VSDC2_R_ADC0_INTEGRAL    = 0x13,
    VSDC2_R_ADC0_INTEGRAL_C  = 0x14,
    VSDC2_R_ADC1_CSR         = 0x15,
    VSDC2_R_ADC1_TIMER       = 0x16,
    VSDC2_R_ADC1_AVG_NUM     = 0x17,
    VSDC2_R_ADC1_WRITE_ADDR  = 0x18,
    VSDC2_R_ADC1_READ_ADDR   = 0x19,
    VSDC2_R_ADC1_DATA        = 0x1A,
    VSDC2_R_ADC1_INTEGRAL    = 0x1B,
    VSDC2_R_ADC1_INTEGRAL_C  = 0x1C,
    VSDC2_R_ADC0_BP_MUX      = 0x20,
    VSDC2_R_ADC1_BP_MUX      = 0x28,

    /* Non-general-purpose registers */
    VSDC2_R_REF_VOLTAGE_LOW  = 0x30,
    VSDC2_R_REF_VOLTAGE_HIGH = 0x31,
    VSDC2_R_BF_TIMER0        = 0x32,
    VSDC2_R_BF_TIMER1        = 0x33,
    VSDC2_R_ADC0_OFFSET_AMP  = 0x34,
    VSDC2_R_ADC1_OFFSET_AMP  = 0x35,
    VSDC2_R_ADC0_OFFSET_ADC  = 0x36,
    VSDC2_R_ADC1_OFFSET_ADC  = 0x37,
};

/* Register-specific bits */
enum
{
    VSDC2_GCSR_GPIO           = 1 << 0,
    VSDC2_GCSR_UART0          = 1 << 1,
    VSDC2_GCSR_UART1          = 1 << 2,
    VSDC2_GCSR_CAN            = 1 << 3,
    VSDC2_GCSR_MUX_MASK       = 3 << 4,
        VSDC2_GCSR_MUX_U_INP  = 0 << 4,
        VSDC2_GCSR_MUX_U_GND  = 1 << 4,
        VSDC2_GCSR_MUX_U_1851 = 2 << 4,
        VSDC2_GCSR_MUX_U_168  = 3 << 4,
    VSDC2_GCSR_CALIB          = 1 << 6,
    VSDC2_GCSR_AUTOCAL        = 1 << 7,
    
    VSDC2_GCSR_PSTART_ALL     = 1 << 14,
    VSDC2_GCSR_PSTOP_ALL      = 1 << 15,
};

enum
{
    VSDC2_ADCn_CSR_PSTART              = 1 << 0,
    VSDC2_ADCn_CSR_PSTOP               = 1 << 1,
    VSDC2_ADCn_CSR_START_MASK          = 7 << 2,
        VSDC2_ADCn_CSR_START_PSTART    = 0 << 2,
        VSDC2_ADCn_CSR_START_SYNC_A    = 5 << 2,
        VSDC2_ADCn_CSR_START_SYNC_B    = 6 << 2,
        VSDC2_ADCn_CSR_START_BACKPLANE = 7 << 2,
    VSDC2_ADCn_CSR_STOP_MASK           = 7 << 5,
        VSDC2_ADCn_CSR_STOP_PSTOP      = 0 << 5,
        VSDC2_ADCn_CSR_STOP_TIMER      = 1 << 5,
        VSDC2_ADCn_CSR_STOP_SYNC_A     = 5 << 5,
        VSDC2_ADCn_CSR_STOP_SYNC_B     = 6 << 5,
        VSDC2_ADCn_CSR_STOP_BACKPLANE  = 7 << 5,
    VSDC2_ADCn_CSR_GAIN_MASK           = 7 << 8,
        VSDC2_ADCn_CSR_GAIN_ERROR      = 0 << 8,
        VSDC2_ADCn_CSR_GAIN_02V        = 1 << 8,
        VSDC2_ADCn_CSR_GAIN_05V        = 2 << 8,
        VSDC2_ADCn_CSR_GAIN_1V         = 3 << 8,
        VSDC2_ADCn_CSR_GAIN_2V         = 4 << 8,
        VSDC2_ADCn_CSR_GAIN_5V         = 5 << 8,
        VSDC2_ADCn_CSR_GAIN_10V        = 6 << 8,
        VSDC2_ADCn_CSR_GAIN_MISSING7   = 7 << 8,
    VSDC2_ADCn_CSR_OVRNG               = 1 << 11,
    VSDC2_ADCn_CSR_STATUS_MASK         = 3 << 12,
        VSDC2_ADCn_CSR_STATUS_STOP     = 0 << 12,
        VSDC2_ADCn_CSR_STATUS_MISSING1 = 1 << 12,
        VSDC2_ADCn_CSR_STATUS_INTE     = 2 << 12,
        VSDC2_ADCn_CSR_STATUS_POSTINT  = 3 << 12,
    VSDC2_ADCn_CSR_MOVF                = 1 << 14,
    VSDC2_ADCn_CSR_SINGLE              = 1 << 15,
    
    VSDC2_ADCn_CSR_IRQ_M_START         = 1 << 16,
    VSDC2_ADCn_CSR_IRQ_M_STOP          = 1 << 17,
    VSDC2_ADCn_CSR_IRQ_M_INTSTOP       = 1 << 18,
    VSDC2_ADCn_CSR_IRQ_M_OVRNG         = 1 << 19,
    VSDC2_ADCn_CSR_IRQ_M_MOVF          = 1 << 20,

    VSDC2_ADCn_CSR_IRQ_C_START         = 1 << 24,
    VSDC2_ADCn_CSR_IRQ_C_STOP          = 1 << 25,
    VSDC2_ADCn_CSR_IRQ_C_INTSTOP       = 1 << 26,
    VSDC2_ADCn_CSR_IRQ_C_OVRNG         = 1 << 27,
    VSDC2_ADCn_CSR_IRQ_C_MOVF          = 1 << 28,
};

enum
{
    VSDC2_ADCn_BP_MUX_START0 = 0,
    VSDC2_ADCn_BP_MUX_START1 = 1,
    VSDC2_ADCn_BP_MUX_STOP0  = 2,
    VSDC2_ADCn_BP_MUX_STOP1  = 3,
};


#define DESC_RD_REG(n) (n)
#define DESC_WR_REG(n) ((n) | 0x80)

enum
{
    /* Global */
    DESC_RD_GCSR            = DESC_RD_REG(VSDC2_R_GCSR),
    DESC_WR_GCSR            = DESC_WR_REG(VSDC2_R_GCSR),

    /* ADC0 */
    DESC_RD_ADC0_INTEGRAL   = DESC_RD_REG(VSDC2_R_ADC0_INTEGRAL),
    
    DESC_RD_ADC0_CSR        = DESC_RD_REG(VSDC2_R_ADC0_CSR),
    DESC_WR_ADC0_CSR        = DESC_WR_REG(VSDC2_R_ADC0_CSR),

    DESC_RD_ADC0_AVG_NUM    = DESC_RD_REG(VSDC2_R_ADC0_AVG_NUM),
    DESC_WR_ADC0_AVG_NUM    = DESC_WR_REG(VSDC2_R_ADC0_AVG_NUM),

    DESC_RD_ADC0_BP_MUX     = DESC_RD_REG(VSDC2_R_ADC0_BP_MUX),
    DESC_WR_ADC0_BP_MUX     = DESC_WR_REG(VSDC2_R_ADC0_BP_MUX),

    DESC_RD_ADC0_WRITE_ADDR = DESC_RD_REG(VSDC2_R_ADC0_WRITE_ADDR),
    DESC_WR_ADC0_WRITE_ADDR = DESC_WR_REG(VSDC2_R_ADC0_WRITE_ADDR),
    DESC_RD_ADC0_READ_ADDR  = DESC_RD_REG(VSDC2_R_ADC0_READ_ADDR),
    DESC_WR_ADC0_READ_ADDR  = DESC_WR_REG(VSDC2_R_ADC0_READ_ADDR),
    DESC_RD_ADC0_DATA       = DESC_RD_REG(VSDC2_R_ADC0_DATA),

    /* ADC1 */
    DESC_RD_ADC1_INTEGRAL   = DESC_RD_REG(VSDC2_R_ADC1_INTEGRAL),

    DESC_RD_ADC1_CSR        = DESC_RD_REG(VSDC2_R_ADC1_CSR),
    DESC_WR_ADC1_CSR        = DESC_WR_REG(VSDC2_R_ADC1_CSR),

    DESC_RD_ADC1_AVG_NUM    = DESC_RD_REG(VSDC2_R_ADC1_AVG_NUM),
    DESC_WR_ADC1_AVG_NUM    = DESC_WR_REG(VSDC2_R_ADC1_AVG_NUM),

    DESC_RD_ADC1_BP_MUX     = DESC_RD_REG(VSDC2_R_ADC1_BP_MUX),
    DESC_WR_ADC1_BP_MUX     = DESC_WR_REG(VSDC2_R_ADC1_BP_MUX),

    DESC_RD_ADC1_WRITE_ADDR = DESC_RD_REG(VSDC2_R_ADC1_WRITE_ADDR),
    DESC_WR_ADC1_WRITE_ADDR = DESC_WR_REG(VSDC2_R_ADC1_WRITE_ADDR),
    DESC_RD_ADC1_READ_ADDR  = DESC_RD_REG(VSDC2_R_ADC1_READ_ADDR),
    DESC_WR_ADC1_READ_ADDR  = DESC_WR_REG(VSDC2_R_ADC1_READ_ADDR),
    DESC_RD_ADC1_DATA       = DESC_RD_REG(VSDC2_R_ADC1_DATA),

};


//////////////////////////////////////////////////////////////////////

// Values are the same as in pzframe_drv.h::PZFRAME_CHTYPE_NNN,
// but that's not important
enum
{
    CHTYPE_UNSUPPORTED = 0,
    CHTYPE_BIGC        = 1,
    CHTYPE_INDIVIDUAL  = 3,
    CHTYPE_VALIDATE    = 4,
    CHTYPE_STATUS      = 5,
    CHTYPE_AUTOUPDATED = 6,
};

static uint8 chinfo_chtype[] =
{
    [VSDC2_CHAN_HW_VER]                = CHTYPE_AUTOUPDATED,
    [VSDC2_CHAN_SW_VER]                = CHTYPE_AUTOUPDATED,

    [VSDC2_CHAN_GAIN_n_base       + 0] = CHTYPE_AUTOUPDATED,
    [VSDC2_CHAN_GAIN_n_base       + 1] = CHTYPE_AUTOUPDATED,

    [VSDC2_CHAN_INT_n_base        + 0] = CHTYPE_INDIVIDUAL, // Not AUTOUPDATED but INDIVIDUAL
    [VSDC2_CHAN_INT_n_base        + 1] = CHTYPE_INDIVIDUAL, // to not set to AUTOUPDATED_TRUSTED

    [VSDC2_CHAN_LINE_n_base       + 0] = CHTYPE_BIGC,
    [VSDC2_CHAN_LINE_n_base       + 1] = CHTYPE_BIGC,

    [VSDC2_CHAN_GET_OSC_n_base    + 0] = CHTYPE_INDIVIDUAL,
    [VSDC2_CHAN_GET_OSC_n_base    + 1] = CHTYPE_INDIVIDUAL,

    [VSDC2_CHAN_PTSOFS_n_base     + 0] = CHTYPE_VALIDATE,
    [VSDC2_CHAN_PTSOFS_n_base     + 1] = CHTYPE_VALIDATE,

    [VSDC2_CHAN_CUR_PTSOFS_n_base + 0] = CHTYPE_STATUS,
    [VSDC2_CHAN_CUR_PTSOFS_n_base + 1] = CHTYPE_STATUS,
    [VSDC2_CHAN_CUR_NUMPTS_n_base + 0] = CHTYPE_STATUS,
    [VSDC2_CHAN_CUR_NUMPTS_n_base + 1] = CHTYPE_STATUS,

    [VSDC2_CHAN_WRITE_ADDR_n_base + 0] = CHTYPE_STATUS,
    [VSDC2_CHAN_WRITE_ADDR_n_base + 1] = CHTYPE_STATUS,
};

//--------------------------------------------------------------------

typedef struct
{
    cankoz_vmt_t    *lvmt;
    int              devid;
    int              handle;

    int                  hw_ver;
    int                  sw_ver;

    int                  ch0start;
    int                  ch0stop;
    int                  ch1start;
    int                  ch1stop;

    /* Oscillogramme support */
    int                  osc_rqd        [VSDC2_CHAN_INT_n_count];
    int                  osc_reading    [VSDC2_CHAN_INT_n_count];
    int32                osc_cur_numpts [VSDC2_CHAN_INT_n_count];
    int                  osc_index      [VSDC2_CHAN_INT_n_count];
    float32              osc_data       [VSDC2_CHAN_INT_n_count][VSDC2_MAX_NUMPTS];
} privrec_t;

static psp_lkp_t start_stop_lkp[] =
{
    {"start0", VSDC2_ADCn_BP_MUX_START0},
    {"start1", VSDC2_ADCn_BP_MUX_START1},
    {"stop0",  VSDC2_ADCn_BP_MUX_STOP0},
    {"stop1",  VSDC2_ADCn_BP_MUX_STOP1},
    {NULL, 0},
};

static psp_paramdescr_t vsdc2_params[] =
{
    PSP_P_LOOKUP("ch0start", privrec_t, ch0start, VSDC2_ADCn_BP_MUX_START0, start_stop_lkp),
    PSP_P_LOOKUP("ch0stop",  privrec_t, ch0stop,  VSDC2_ADCn_BP_MUX_STOP0,  start_stop_lkp),
    PSP_P_LOOKUP("ch1start", privrec_t, ch1start, VSDC2_ADCn_BP_MUX_START1, start_stop_lkp),
    PSP_P_LOOKUP("ch1stop",  privrec_t, ch1stop,  VSDC2_ADCn_BP_MUX_STOP1,  start_stop_lkp),

    PSP_P_END()
};


//////////////////////////////////////////////////////////////////////

static void vsdc2_ff (int devid, void *devptr, int is_a_reset);
static void vsdc2_in (int devid, void *devptr,
                      int desc,  size_t dlc, uint8 *data);

static int  vsdc2_init_d(int devid, void *devptr,
                         int businfocount, int businfo[],
                         const char *auxinfo)
{
  privrec_t *me = devptr;
  int        n;

    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "%s([%d]:\"%s\")",
                __FUNCTION__, businfocount, auxinfo);

    /* Initialize interface */
    me->lvmt   = GetLayerVMT(devid);
    me->devid  = devid;
    me->handle = me->lvmt->add(devid, devptr,
                               businfocount, businfo,
                               DEVTYPE,
                               vsdc2_ff, vsdc2_in,
                               VSDC2_NUMCHANS*2/*!!!*/,
                               CANKOZ_LYR_OPTION_NONE);
    if (me->handle < 0) return me->handle;

    for (n = 0;  n < countof(chinfo_chtype);  n++)
        if (chinfo_chtype[n] == CHTYPE_AUTOUPDATED  ||
            chinfo_chtype[n] == CHTYPE_STATUS)
            SetChanReturnType(devid, n, 1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, VSDC2_CHAN_INT_n_base, VSDC2_CHAN_INT_n_count, IS_AUTOUPDATED_YES);

    return DEVSTATE_OPERATING;
}

static void vsdc2_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;

    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "%s()", __FUNCTION__);
#if 0 /*!!! Send "STOP"? */
    me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                          SQ_TRIES_DIR, 0,
                          NULL, NULL,
                          0, 1, DESC_STOP);
#endif
}

static void WriteReg(privrec_t *me, int desc, int32 rv)
{
    me->lvmt->q_enq_ons_v(me->handle, SQ_ALWAYS,
                          5, desc,
                          (rv)       & 0xFF,
                          (rv >>  8) & 0xFF,
                          (rv >> 16) & 0xFF,
                          (rv >> 24) & 0xFF);
}
static void vsdc2_ff (int devid, void *devptr, int is_a_reset)
{
  privrec_t *me = devptr;

    me->lvmt->get_dev_ver(me->handle, &(me->hw_ver), &(me->sw_ver), NULL);

    WriteReg(me, DESC_WR_GCSR,         VSDC2_GCSR_AUTOCAL);
    // VSDC2_ADCn_CSR_IRQ_M_INTSTOP | VSDC2_ADCn_CSR_START_BACKPLANE | VSDC2_ADCn_CSR_STOP_BACKPLANE
    WriteReg(me, DESC_WR_ADC0_CSR,     0x000400FC);
    WriteReg(me, DESC_WR_ADC1_CSR,     0x000400FC);
    WriteReg(me, DESC_WR_ADC0_AVG_NUM, 0);
    WriteReg(me, DESC_WR_ADC1_AVG_NUM, 0);
    if (me->sw_ver >= 2)
    {
        WriteReg(me, DESC_WR_ADC0_BP_MUX,
                 (me->ch0start & 3) | ((me->ch0stop  & 3) << 4));
        WriteReg(me, DESC_WR_ADC1_BP_MUX,
                 (me->ch1start & 3) | ((me->ch1stop  & 3) << 4));
    }

    ReturnInt32Datum(me->devid, VSDC2_CHAN_HW_VER, me->hw_ver, 0);
    ReturnInt32Datum(me->devid, VSDC2_CHAN_SW_VER, me->sw_ver, 0);

    if (is_a_reset)
    {
        bzero(me->osc_rqd,     sizeof(me->osc_rqd));
        bzero(me->osc_reading, sizeof(me->osc_reading));

        ReturnInt32Datum(devid, VSDC2_CHAN_GET_OSC0, me->osc_rqd[0], 0);
        ReturnInt32Datum(devid, VSDC2_CHAN_GET_OSC1, me->osc_rqd[1], 0);
    }
}

static void vsdc2_in (int devid, void *devptr,
                      int desc,  size_t dlc, uint8 *data)
{
  privrec_t  *me       = (privrec_t *) devptr;
  int         nl;       // Line #

  int         chn;
  void       *vp;
  uint32      u32;
  float32     f32;

  union
  {
      int32   i32;
      float32 f32;
  } intflt;

  static cxdtype_t dtype_f32   = CXDTYPE_SINGLE;
  static int       n_1         = 1;
  static rflags_t  zero_rflags = 0;
  static rflags_t  ovfl_rflags = CXRF_OVERLOAD;

#if __GNUC__ < 3
  int              r_addrs [2];
  cxdtype_t        r_dtypes[2];
  int              r_nelems[2];
  void            *r_values[2];
  static rflags_t  r_rflags[2] = {0, 0};
#endif

    switch (desc)
    {
        case DESC_RD_ADC0_CSR:
        case DESC_RD_ADC1_CSR:
            nl = (desc == DESC_RD_ADC0_CSR)? 0 : 1;
            u32 = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);

            ReturnInt32Datum(devid, VSDC2_CHAN_GAIN_n_base + nl,
                             (u32 >> 8) & 7, 0);

            if (u32 & ((1 << 11) | (1 << 14)))
            {
                chn = VSDC2_CHAN_INT_n_base + nl;
                f32 = 0.0;
                vp  = &f32;
                ReturnDataSet(devid, 1,
                              &chn, &dtype_f32, &n_1, &vp, &ovfl_rflags, NULL);
                return;
            }
            me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                              (nl == 0)? DESC_RD_ADC0_INTEGRAL : DESC_RD_ADC1_INTEGRAL);
            break;

        case DESC_RD_ADC0_INTEGRAL:
        case DESC_RD_ADC1_INTEGRAL:
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);

            nl = (desc == DESC_RD_ADC0_INTEGRAL)? 0 : 1;
            intflt.i32 = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);

            chn = VSDC2_CHAN_INT_n_base + nl;
            vp  = &f32;

            if (intflt.i32 == 0x7f800000) // +NaN => integration isn't finished yet
            {
                f32 = 0.0;
                ReturnDataSet(devid, 1,
                              &chn, &dtype_f32, &n_1, &vp, &ovfl_rflags, NULL);
            }
            else
            {
                f32 = intflt.f32;
                ReturnDataSet(devid, 1,
                              &chn, &dtype_f32, &n_1, &vp, &zero_rflags, NULL);
            }

            me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                              (nl == 0)? DESC_RD_ADC0_WRITE_ADDR : DESC_RD_ADC1_WRITE_ADDR);
            break;

        case DESC_RD_ADC0_WRITE_ADDR:
        case DESC_RD_ADC1_WRITE_ADDR:
            if (dlc < 5) return;
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);

            nl = (desc == DESC_RD_ADC0_WRITE_ADDR)? 0 : 1;
            u32 = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
            ReturnInt32Datum(devid, VSDC2_CHAN_WRITE_ADDR_n_base + nl, u32, 0);

            if (me->osc_rqd[nl] == 0) return;

            if (u32 > VSDC2_MAX_NUMPTS) u32 = VSDC2_MAX_NUMPTS;

            me->osc_rqd       [nl] = 0;
            ReturnInt32Datum(devid, VSDC2_CHAN_GET_OSC_n_base + nl, 0, 0);
            me->osc_reading   [nl] = 1;
            me->osc_cur_numpts[nl] = u32;
            me->osc_index     [nl] = 0;
            WriteReg(me,
                     (nl == 0)? DESC_WR_ADC0_READ_ADDR : DESC_WR_ADC1_READ_ADDR,
                     0);
            me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1,
                              (nl == 0)? DESC_RD_ADC0_DATA : DESC_RD_ADC1_DATA);
            break;

        case DESC_RD_ADC0_DATA:
        case DESC_RD_ADC1_DATA:
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);

            nl = (desc == DESC_RD_ADC0_DATA)? 0 : 1;
            if (me->osc_reading[nl] == 0) return;

            intflt.i32 = data[1] | (data[2] << 8) | (data[3] << 16) | (data[4] << 24);
            me->osc_data[nl][(me->osc_index[nl])++] = intflt.f32;
            if (me->osc_index[nl] < me->osc_cur_numpts[nl])
            {
                me->lvmt->q_enq_v(me->handle, SQ_IF_NONEORFIRST, 1, desc);
            }
            else
            {
                me->osc_reading[nl] = 0;
#if __GNUC__ < 3
                r_addrs [0] = VSDC2_CHAN_CUR_NUMPTS_n_base + nl; r_addrs [1] = VSDC2_CHAN_LINE_n_base + nl;
                r_dtypes[0] = CXDTYPE_INT32;                     r_dtypes[1] = CXDTYPE_SINGLE;
                r_nelems[0] = 1;                                 r_nelems[1] = me->osc_cur_numpts[nl];
                r_values[0] = me->osc_cur_numpts           + nl; r_values[1] = me->osc_data           + nl;
                ReturnDataSet(me->devid,
                              2,
                              r_addrs,
                              r_dtypes,
                              r_nelems,
                              r_values,
                              r_rflags,
                              NULL);
#else
                ReturnDataSet(me->devid,
                              2,
                              (int      []){VSDC2_CHAN_CUR_NUMPTS_n_base + nl, VSDC2_CHAN_LINE_n_base + nl},
                              (cxdtype_t[]){CXDTYPE_INT32,                     CXDTYPE_SINGLE},
                              (int      []){1,                                 me->osc_cur_numpts[nl]},
                              (void*    []){me->osc_cur_numpts           + nl, me->osc_data           + nl},
                              (rflags_t []){0,                                 0},
                              NULL);
#endif
            }

            break;
    }
}

static void vsdc2_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = (privrec_t *)devptr;

  int        n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int        chn;   // channel
  int        ct;
  int32      val;   // Value
  int        nl;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];

        if (chn < 0  ||  chn >= countof(chinfo_chtype)  ||
            (ct = chinfo_chtype[chn]) == CHTYPE_UNSUPPORTED)
        {
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);
        }
        else
        {
            if (action == DRVA_WRITE)
            {
                if (nelems[n] != 1  ||
                    (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                    goto NEXT_CHANNEL;
                val = *((int32*)(values[n]));
            }
            else
                val = 0xFFFFFFFF; // That's just to make GCC happy

            if      (chn >= VSDC2_CHAN_GET_OSC_n_base  &&
                     chn <  VSDC2_CHAN_GET_OSC_n_base + VSDC2_CHAN_INT_n_count)
            {
                nl = chn - VSDC2_CHAN_GET_OSC_n_base;
                if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                    me->osc_rqd[nl] = 1;
                ReturnInt32Datum(devid, chn, me->osc_rqd[nl], 0);
            }
            else if (chn >= VSDC2_CHAN_PTSOFS_n_base  &&
                     chn <  VSDC2_CHAN_PTSOFS_n_base + VSDC2_CHAN_INT_n_count)
            {
                ReturnInt32Datum(devid, chn, 000/*!!!*/, 0);
            }
        }

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(vsdc2, "VSDC2 driver",
                   NULL, NULL,
                   sizeof(privrec_t), vsdc2_params,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   vsdc2_init_d, vsdc2_term_d, vsdc2_rw_p);
