#include "cxsd_driver.h"

#include "drv_i/dds500_drv_i.h"


enum
{
    A_BUF01    = 0,
    A_BUF23    = 1,
    A_BUFIN    = 2,
    A_DELAYS   = 3,

    A_STATUS   = 5,
    A_INFOWORD = 15,
};

#if 0
enum
{
    RN_CSR    = 0x00,
    RN_FR1    = 0x01,
    RN_FR2    = 0x02,
    RN_CFRx   = 0x03,
    RN_CTW0x  = 0x04,
    RN_CPW0x  = 0x05,
    RN_ACRx   = 0x06,
    RN_LSRx   = 0x07,
    RN_RDWx   = 0x08,
    RN_FDWx   = 0x09,
    RN_CTW1x  = 0x0A,
    RN_CTW2x  = 0x0B,
    RN_CTW3x  = 0x0C,
    RN_CTW4x  = 0x0D,
    RN_CTW5x  = 0x0E,
    RN_CTW6x  = 0x0F,
    RN_CTW7x  = 0x10,
    RN_CTW8x  = 0x11,
    RN_CTW9x  = 0x12,
    RN_CTW10x = 0x13,
    RN_CTW11x = 0x14,
    RN_CTW12x = 0x15,
    RN_CTW13x = 0x16,
    RN_CTW14x = 0x17,
    RN_CTW15x = 0x18,
};
#endif

enum
{
    DDS500_STATUS_INOUT1_shift =  0, DDS500_STATUS_INOUT1 = 1 << DDS500_STATUS_INOUT1_shift,
    DDS500_STATUS_INOUT2_shift =  1, DDS500_STATUS_INOUT2 = 1 << DDS500_STATUS_INOUT2_shift,
    DDS500_STATUS_FUNIN1_shift =  2, DDS500_STATUS_FUNIN1 = 1 << DDS500_STATUS_FUNIN1_shift,
    DDS500_STATUS_FUNIN2_shift =  3, DDS500_STATUS_FUNIN2 = 1 << DDS500_STATUS_FUNIN2_shift,
    DDS500_STATUS_RSRVD4_shift =  4, DDS500_STATUS_RSRVD4 = 1 << DDS500_STATUS_RSRVD4_shift,
    DDS500_STATUS_RSRVD5_shift =  5, DDS500_STATUS_RSRVD5 = 1 << DDS500_STATUS_RSRVD5_shift,
    DDS500_STATUS_DLYENA_shift =  6, DDS500_STATUS_DLYENA = 1 << DDS500_STATUS_DLYENA_shift,
    DDS500_STATUS_GENINTEXT_shift = 7, DDS500_STATUS_GENINTEXT = 1 << DDS500_STATUS_GENINTEXT_shift,
    DDS500_STATUS_SW0_shift    =  8, DDS500_STATUS_SW0    = 1 << DDS500_STATUS_SW0_shift,
    DDS500_STATUS_SW1_shift    =  9, DDS500_STATUS_SW1    = 1 << DDS500_STATUS_SW1_shift,
    DDS500_STATUS_P2_shift     = 10, DDS500_STATUS_P2     = 1 << DDS500_STATUS_P2_shift,
    DDS500_STATUS_P3_shift     = 11, DDS500_STATUS_P3     = 1 << DDS500_STATUS_P3_shift,
};

enum
{
    DDS500_REG_CSR    = 0x00,  // Channel Select Register
    DDS500_REG_FR1    = 0x01,
    DDS500_REG_FR2    = 0x02,
    DDS500_REG_CFRx   = 0x03,
    DDS500_REG_CFTW0x = 0x04,  // FTW (Frequency Tuning Word)
    DDS500_REG_CPOW0x = 0x05,
    DDS500_REG_ACRx   = 0x06,
    DDS500_REG_LSRx   = 0x07,
    DDS500_REG_RDWx   = 0x08,
    DDS500_REG_FDWx   = 0x09,
    DDS500_REG_CTW1x  = 0x0A,
    DDS500_REG_CTW2x  = 0x0B,
    DDS500_REG_CTW3x  = 0x0C,
    DDS500_REG_CTW4x  = 0x0D,
    DDS500_REG_CTW5x  = 0x0E,
    DDS500_REG_CTW6x  = 0x0F,
    DDS500_REG_CTW7x  = 0x10,
    DDS500_REG_CTW8x  = 0x11,
    DDS500_REG_CTW9x  = 0x12,
    DDS500_REG_CTW10x = 0x13,
    DDS500_REG_CTW11x = 0x14,
    DDS500_REG_CTW12x = 0x15,
    DDS500_REG_CTW13x = 0x16,
    DDS500_REG_CTW14x = 0x17,
    DDS500_REG_CTW15x = 0x18,
};

enum
{
    DDS500_INSTR_ACTION_WR = 0 << 7,
    DDS500_INSTR_ACTION_RD = 1 << 7,
};

enum
{
    DDS500_CSR_CSEL0_shift = 6, DDS500_CSR_CSEL0 = 1 << DDS500_CSR_CSEL0_shift,
    DDS500_CSR_CSEL1_shift = 7, DDS500_CSR_CSEL1 = 1 << DDS500_CSR_CSEL1_shift,
};

enum
{
    DDS500_FR2_CLR_PHASE_shift     = 12, DDS500_FR2_CLR_PHASE     = 1 << DDS500_FR2_CLR_PHASE_shift,
    DDS500_FR2_AUTOCLR_PHASE_shift = 13, DDS500_FR2_AUTOCLR_PHASE = 1 << DDS500_FR2_AUTOCLR_PHASE_shift,
};

enum
{
    DDS500_CFRx_SINE_shift = 0, DDS500_CFRx_SINE = 1 << DDS500_CFRx_SINE_shift,
};

enum
{
    DDS500_ACRx_A_MUL_ENA_shift = 12, DDS500_ACRx_A_MUL_ENA = 1 << DDS500_ACRx_A_MUL_ENA_shift,
};

static rflags_t rd_stat(int N, int *v_p)
{
    return x2rflags(DO_NAF(CAMAC_REF, N, A_STATUS, 0, v_p));
}

static rflags_t wr_stat(int N, int v)
{
    return x2rflags(DO_NAF(CAMAC_REF, N, A_STATUS, 16, &v));
}

#define UPDATE_DDS() rflags |= x2rflags(DO_NAF(CAMAC_REF, N, 0, 26, &c));

static rflags_t rd_reg(int N, int r_n, int l, int *v_p)
{
  rflags_t  rflags = 0;
  int       c;

    // Select line
    if (r_n >= 3)
    {
        c = DDS500_REG_CSR | DDS500_INSTR_ACTION_WR;
        rflags |= x2rflags(DO_NAF(CAMAC_REF, N, A_BUFIN, 16, &c));
        c = (l + 1) << DDS500_CSR_CSEL0_shift;
        rflags |= x2rflags(DO_NAF(CAMAC_REF, N, A_BUF01, 16, &c));
        UPDATE_DDS();
    }

    c = (r_n & 0x1F) | DDS500_INSTR_ACTION_RD;
    rflags |= x2rflags(DO_NAF(CAMAC_REF, N, A_BUFIN, 16, &c));
    UPDATE_DDS();

    rflags |= x2rflags(DO_NAF(CAMAC_REF, N, A_BUF01,  0, &c));
    *v_p    =  c & 0xFFFF;
    rflags |= x2rflags(DO_NAF(CAMAC_REF, N, A_BUF23,  0, &c));
    *v_p   |= (c & 0xFFFF) << 16;

    return rflags;
}

static rflags_t wr_reg(int N, int r_n, int l, int v)
{
  rflags_t  rflags = 0;
  int       c;

    // Select line
    if (r_n >= 3)
    {
        c = DDS500_REG_CSR | DDS500_INSTR_ACTION_WR;
        rflags |= x2rflags(DO_NAF(CAMAC_REF, N, A_BUFIN, 16, &c));
        c = (l + 1) << DDS500_CSR_CSEL0_shift;
        rflags |= x2rflags(DO_NAF(CAMAC_REF, N, A_BUF01, 16, &c));
        UPDATE_DDS();
    }

    c = (r_n & 0x1F) | DDS500_INSTR_ACTION_WR;
    rflags |= x2rflags(DO_NAF(CAMAC_REF, N, A_BUFIN, 16, &c));
    c = (v >> 16) & 0xFFFF;
    rflags |= x2rflags(DO_NAF(CAMAC_REF, N, A_BUF23, 16, &c));
    c =  v        & 0xFFFF;
    rflags |= x2rflags(DO_NAF(CAMAC_REF, N, A_BUF01, 16, &c));
    UPDATE_DDS();

    return rflags;
}


//////////////////////////////////////////////////////////////////////

typedef struct
{
    int  N;
    
    int32     infoword;
    rflags_t  infoword_rflags;

    int       debug_reg;

    int32     defvals[DDS500_NUMCHANS];
} dds500_privrec_t;

static psp_paramdescr_t dds500_params[] =
{
    PSP_P_FLAG("=def_delays",    dds500_privrec_t, defvals[DDS500_CHAN_ENABLE_DELAYS], -1, 1),
    PSP_P_FLAG("enable_delays",  dds500_privrec_t, defvals[DDS500_CHAN_ENABLE_DELAYS],  1, 0),
    PSP_P_FLAG("disable_delays", dds500_privrec_t, defvals[DDS500_CHAN_ENABLE_DELAYS],  0, 0),

    PSP_P_FLAG("=def_shape0",    dds500_privrec_t, defvals[DDS500_CHAN_USE_SINE_n_base + 0], -1, 1),
    PSP_P_FLAG("ch0_cosine",     dds500_privrec_t, defvals[DDS500_CHAN_USE_SINE_n_base + 0],  1, 0),
    PSP_P_FLAG("ch0_sine",       dds500_privrec_t, defvals[DDS500_CHAN_USE_SINE_n_base + 0],  0, 0),

    PSP_P_FLAG("=def_shape1",    dds500_privrec_t, defvals[DDS500_CHAN_USE_SINE_n_base + 1], -1, 1),
    PSP_P_FLAG("ch1_cosine",     dds500_privrec_t, defvals[DDS500_CHAN_USE_SINE_n_base + 1],  1, 0),
    PSP_P_FLAG("ch1_sine",       dds500_privrec_t, defvals[DDS500_CHAN_USE_SINE_n_base + 1],  0, 0),

    PSP_P_END()
};

static void dds500_rw_p(int devid, void *devptr,
                        int action,
                        int count, int *addrs,
                        cxdtype_t *dtypes, int *nelems, void **values);

static void DoReset    (int devid, dds500_privrec_t *me)
{
  int        ndc;

  static cxdtype_t dt_int32 = CXDTYPE_INT32;
  static int       nels_1   = 1;

  void            *vp;

  static int resettables[] =
  {
      DDS500_CHAN_ENABLE_DELAYS,
      DDS500_CHAN_USE_SINE_n_base + 0, DDS500_CHAN_USE_SINE_n_base + 1,
  };

    for (ndc = 0;  ndc < countof(resettables);  ndc++)
        if (me->defvals[resettables[ndc]] >= 0)
        {
            vp = me->defvals + resettables[ndc];
            dds500_rw_p(devid, me,
                        DRVA_WRITE,
                        1, resettables + ndc,
                        &dt_int32, &nels_1, &vp);
        }
}

static int dds500_init_d(int devid, void *devptr,
                         int businfocount, int *businfo,
                         const char *auxinfo)
{
  dds500_privrec_t *me = (dds500_privrec_t*)devptr;
  int               c;
  
    me->N = businfo[0];

    me->infoword_rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_INFOWORD, 1, &(me->infoword)));

    DoReset(devid, me);
    
    return DEVSTATE_OPERATING;
}

static void dds500_rw_p(int devid, void *devptr,
                        int action,
                        int count, int *addrs,
                        cxdtype_t *dtypes, int *nelems, void **values)
{
  dds500_privrec_t *me = (dds500_privrec_t*)devptr;

  int               n;     // channel N in values[] (loop ctl var)
  int               chn;   // channel
  
  int               l;
  int               c;
  int32             value;
  rflags_t          rflags;
  int               junk;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
        if (action == DRVA_WRITE)
        {
            if (nelems[n] != 1  ||
                (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            value = *((int32*)(values[n]));
            ////fprintf(stderr, " write #%d:=%d\n", chn, val);
        }
        else
            value = 0xFFFFFFFF; // That's just to make GCC happy
        rflags = 0;
        switch (chn)
        {
            case DDS500_CHAN_AMP_n_base + 0:
            case DDS500_CHAN_AMP_n_base + 1:
                l = chn - DDS500_CHAN_AMP_n_base;
                if (action == DRVA_WRITE)
                {
                    if (value < 0)    value = 0;
                    if (value > 1023) value = 1023;

                    rflags |= wr_reg(me->N, DDS500_REG_ACRx, l, value | DDS500_ACRx_A_MUL_ENA);
                }
                rflags |= rd_reg(me->N, DDS500_REG_ACRx, l, &c);
                value = c & 1023;
                break;

            case DDS500_CHAN_PHS_n_base + 0:
            case DDS500_CHAN_PHS_n_base + 1:
                l = chn - DDS500_CHAN_PHS_n_base;
                if (action == DRVA_WRITE)
                {
                    if (value < 0)      value = 0;
                    if (value > 0x3FFF) value = 0x3FFF;

                    rflags |= wr_reg(me->N, DDS500_REG_CPOW0x, l, value);
                }
                rflags |= rd_reg(me->N, DDS500_REG_CPOW0x, l, &c);
                value = c & 0x3FFF;
                break;

            case DDS500_CHAN_FTW_n_base + 0:
            case DDS500_CHAN_FTW_n_base + 1:
                l = chn - DDS500_CHAN_FTW_n_base;
                if (action == DRVA_WRITE)
                {
                    rflags |= wr_reg(me->N, DDS500_REG_CFTW0x, l, value);
                }
                rflags |= rd_reg(me->N, DDS500_REG_CFTW0x, l, &c);
                value = c;
                break;

            case DDS500_CHAN_USE_SINE_n_base + 0:
            case DDS500_CHAN_USE_SINE_n_base + 1:
                l = chn - DDS500_CHAN_USE_SINE_n_base;
                if (action == DRVA_WRITE)
                {
                    rflags |= rd_reg(me->N, DDS500_REG_CFRx, l, &c);
                    if (value == 0) c &=~ DDS500_CFRx_SINE;
                    else            c |=  DDS500_CFRx_SINE;
                    rflags |= wr_reg(me->N, DDS500_REG_CFRx, l,  c);
                }
                rflags |= rd_reg(me->N, DDS500_REG_CFRx, l, &c);
                value   = (c & DDS500_CFRx_SINE) != 0;
                break;

            case DDS500_CHAN_DLY_n_base + 0:
            case DDS500_CHAN_DLY_n_base + 1:
                l = chn - DDS500_CHAN_DLY_n_base;
                if (action == DRVA_WRITE)
                {
                    if (value < 0)   value = 0;
                    if (value > 255) value = 255;

                    /* Read current value, ... */
                    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_DELAYS,  26,  &junk));
                    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_DELAYS,   0,  &c));
                    /* ...modify, ...*/
                    if (l == 0) c = (c & 0xFF00) |  value;
                    else        c = (c & 0x00FF) | (value << 8);
                    /* ...write */
                    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_DELAYS,  16, &c));
                }
                rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_DELAYS,  26,  &junk));
                rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_DELAYS,   0,  &c));
                if (l == 0) value =  c       & 0xFF;
                else        value = (c >> 8) & 0xFF;
                break;

            case DDS500_CHAN_ENABLE_DELAYS:
                if (action == DRVA_WRITE)
                {
                    rflags |= rd_stat(me->N, &c);
                    if (value == 0) c &=~ DDS500_STATUS_DLYENA;
                    else            c |=  DDS500_STATUS_DLYENA;
                    rflags |= wr_stat(me->N,  c);
                }
                rflags |= rd_stat(me->N, &c);
                value   = (c & DDS500_STATUS_DLYENA) != 0;
                break;

            case DDS500_CHAN_CLR_PHASE:
                if (action == DRVA_WRITE  &&
                    value  == CX_VALUE_COMMAND)
                {
                    rflags |= rd_reg(me->N, DDS500_REG_FR2, 0, &c);
                    c |=  DDS500_FR2_CLR_PHASE;
                    rflags |= wr_reg(me->N, DDS500_REG_FR2, 0,  c);
                    c &=~ DDS500_FR2_CLR_PHASE;
                    rflags |= wr_reg(me->N, DDS500_REG_FR2, 0,  c);
                }
                value = 0;
                break;

            case DDS500_CHAN_RESET_TO_DEFAULTS:
                if (action == DRVA_WRITE  &&
                    value  == DDS500_V_RESET_TO_DEFAULTS_KEYVAL)
                    DoReset(devid, me);
                value = 0;
                break;
                
            case DDS500_CHAN_SERIAL:
                value  = (me->infoword >> 8) & 0xFF;
                rflags = me->infoword_rflags;
                break;
            
            case DDS500_CHAN_FIRMWARE:
                value  = me->infoword & 0xFF;
                rflags = me->infoword_rflags;
                break;
            
            case DDS500_CHAN_DEBUG_REG_W:
                if (action == DRVA_WRITE)
                    rflags = wr_reg(me->N, me->debug_reg % 100, me->debug_reg / 100, value);
                else
                    rflags = 0;
                value = 0;
                break;

            case DDS500_CHAN_DEBUG_REG_R:
                rflags = rd_reg(me->N, me->debug_reg % 100, me->debug_reg / 100, &c);
                value  = c;
                break;

            default:
                value  = 0;
                rflags = CXRF_UNSUPPORTED;
        }

        ReturnInt32Datum(devid, chn, value, rflags);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(dds500, "Shubin DDS500",
                   NULL, NULL,
                   sizeof(dds500_privrec_t), dds500_params,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   dds500_init_d, NULL, dds500_rw_p);
