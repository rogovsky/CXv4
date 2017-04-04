#include "cxsd_driver.h"

#include "drv_i/dds300_drv_i.h"


/* AD9854-specific definitions. Candidates for ad9854_chip.h? */

/* _R_* -- registers' END addresses (big-endian, so end address contains LSB). */
enum
{
    AD9854_R_PAR1   = 0x01,  AD9854_R_PAR_BYTES    = 2,
    AD9854_R_PAR2   = 0x03,
    AD9854_R_FTW1   = 0x09,  AD9854_R_FTW_BYTES    = 6,
    AD9854_R_FTW2   = 0x0F,
    AD9854_R_DFW    = 0x15,  AD9854_R_DFW_BYTES    = 6,
    AD9854_R_UPDCLK = 0x19,  AD9854_R_UPDCLK_BYTES = 4,
    AD9854_R_RRC    = 0x1C,  AD9854_R_RRC_BYTES    = 3,
    AD9854_R_CSR3   = 0x1D,  // PowerDown ctl
    AD9854_R_CSR2   = 0x1E,
    AD9854_R_CSR1   = 0x1F,
    AD9854_R_CSR0   = 0x20,
    AD9854_R_SHK_I  = 0x22,  AD9854_R_SHK_BYTES    = 2,
    AD9854_R_SHK_Q  = 0x24,  
    AD9854_R_SHK_RR = 0x25,
    AD9854_R_QDAC   = 0x27,  AD9854_R_QDAC_BYTES   = 2,
};

enum
{
    AD9854_CSR3_DIG_PD  = 1 << 0,
    AD9854_CSR3_DAC_PD  = 1 << 1,
    AD9854_CSR3_QDAC_PD = 1 << 2,
    AD9854_CSR3_COMP_PD = 1 << 4,

    AD9854_CSR2_ = 1 << 0,
    
    AD9854_CSR1_ = 1 << 0,
    
    AD9854_CSR0_SDO_ACTIVE   = 1 << 0,
    AD9854_CSR0_LSB_FIRST    = 1 << 1,
    AD9854_CSR0_OSK_INT      = 1 << 4,
    AD9854_CSR0_OSK_EN       = 1 << 5,
    AD9854_CSR0_BYP_INV_SINC = 1 << 6,
};


/* DDS300-specific definitions */

enum
{
    A_DATA   = 0,
    A_REGADR = 1,
    A_STATUS = 2,
    A_SERIAL = 3,
};

enum
{
    STAT_SH_REF = 0,
    STAT_SH_OUT = 1,
    STAT_SH_WRT = 2,
};


typedef struct
{
    int  N;
} dds300_privrec_t;



static int dds300_init_d(int devid, void *devptr,
                         int businfocount, int *businfo,
                         const char *auxinfo)
{
  dds300_privrec_t *me = (dds300_privrec_t*)devptr;
  int               c;
  
    me->N = businfo[0];

    /* Set  OSK_EN and BypassInvSinc */
    // Enable write to CSR
    DO_NAF(CAMAC_REF, me->N, A_STATUS,  0, &c);
    c |= (1 << STAT_SH_WRT);
    DO_NAF(CAMAC_REF, me->N, A_STATUS, 16, &c);
    // Write value
    c = (AD9854_R_CSR0 << 8) | (AD9854_CSR0_OSK_EN | AD9854_CSR0_BYP_INV_SINC);
    DO_NAF(CAMAC_REF, me->N, A_DATA,   16, &c);
    // Update
    DO_NAF(CAMAC_REF, me->N, 0, 26, &c);
    
    return DEVSTATE_OPERATING;
}

static rflags_t dds300_rd_data(int N, int ra, uint8 *data, int data_len)
{
  rflags_t  rflags = 0;
  int       c;

    for (; data_len > 0;  ra--, data++, data_len--)
    {
        c = ra << 8;
        rflags |= x2rflags(DO_NAF(CAMAC_REF, N, A_REGADR, 16, &c));
        rflags |= x2rflags(DO_NAF(CAMAC_REF, N, A_DATA,   0,  &c));
        *data = c;
    }
    return rflags;
}

static rflags_t dds300_wr_data(int N, int ra, uint8 *data, int data_len)
{
  rflags_t  rflags = 0;
  int       c;

    for (; data_len > 0;  ra--, data++, data_len--)
    {
        c = (ra << 8) | *data;
        rflags |= x2rflags(DO_NAF(CAMAC_REF, N, A_DATA, 16, &c));
    }
  
    return rflags;
}

static rflags_t dds300_rd_int(int N, int ra, int32 *v_p, int nbytes)
{
  rflags_t  rflags;
  uint8     data[4];

    data[0] = data[1] = data[2] = data[3] = 0;
    rflags = dds300_rd_data(N, ra, data, nbytes);
    *v_p =          data[0]         |
           ((int32)(data[1]) << 8)  |
           ((int32)(data[2]) << 16) |
           ((int32)(data[3]) << 24);
    return rflags;
}

static rflags_t dds300_wr_int(int N, int ra, int32  val, int nbytes)
{
  uint8     data[4];
    
    data[0] =  val        & 0xFF;
    data[1] = (val >>  8) & 0xFF;
    data[2] = (val >> 16) & 0xFF;
    data[3] = (val >> 24) & 0xFF;
    return dds300_wr_data(N, ra, data, nbytes);
}

static rflags_t dds300_rd_int64(int N, int ra, int64 *v_p, int nbytes)
{
  rflags_t  rflags;
  uint8     data[8];

    data[0] = data[1] = data[2] = data[3] =
    data[4] = data[5] = data[6] = data[7] = 0;
    rflags = dds300_rd_data(N, ra, data, nbytes);
    *v_p =          data[0]         |
           ((int64)(data[1]) << 8)  |
           ((int64)(data[2]) << 16) |
           ((int64)(data[3]) << 24) |
           ((int64)(data[4]) << 32) |
           ((int64)(data[5]) << 40) |
           ((int64)(data[6]) << 48) |
           ((int64)(data[7]) << 56);
    return rflags;
}

static rflags_t dds300_wr_int64(int N, int ra, int64  val, int nbytes)
{
  uint8     data[8];
    
    data[0] =  val        & 0xFF;
    data[1] = (val >>  8) & 0xFF;
    data[2] = (val >> 16) & 0xFF;
    data[3] = (val >> 24) & 0xFF;
    data[4] = (val >> 32) & 0xFF;
    data[5] = (val >> 40) & 0xFF;
    data[6] = (val >> 48) & 0xFF;
    data[7] = (val >> 56) & 0xFF;
    return dds300_wr_data(N, ra, data, nbytes);
}

static void dds300_rw_p(int devid, void *devptr,
                        int action,
                        int count, int *addrs,
                        cxdtype_t *dtypes, int *nelems, void **values)
{
  dds300_privrec_t *me = (dds300_privrec_t*)devptr;

  int               n;     // channel N in values[] (loop ctl var)
  int               chn;   // channel
  int               ra;    // Reg endAddress
  
  int               c;
  int32             value;
  rflags_t          rflags;
  int               junk;

#define POW248 0x1000000000000LL
  int32             Fgen = 30*1000*1000; // 30 MHz
  int32             REF_MULT = 6;
  double            fquant;
  int64             FTW;
  
  int               ndc;
  struct {int n; int32 v;} def_vals[] =
  {
      {DDS300_CHAN_AMP1,    0},
      {DDS300_CHAN_AMP2,    0},
      {DDS300_CHAN_PHS1,    0},
      {DDS300_CHAN_PHS2,    0},
      {DDS300_CHAN_FRQ1,    0},
      {DDS300_CHAN_FRQ2,    0},

      {DDS300_CHAN_REF_SEL, DDS300_V_REF_INT},
      {DDS300_CHAN_OUT_SEL, DDS300_V_OUT_REFGEN},
  };
      
#define DDS300_UPDATE() \
    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, 0, 26, &junk))
  
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
            case DDS300_CHAN_AMP1:
            case DDS300_CHAN_AMP2:
                ra = (chn == DDS300_CHAN_AMP1)? AD9854_R_SHK_I : AD9854_R_SHK_Q;
                if (action == DRVA_WRITE)
                {
                    if (value < 0)      value = 0;
                    if (value > 0x0FFF) value = 0x0FFF;
                    rflags |= dds300_wr_int(me->N, ra, value, AD9854_R_SHK_BYTES);
                    DDS300_UPDATE();
                }
                rflags |= dds300_rd_int(me->N, ra, &value, AD9854_R_SHK_BYTES);
                break;
                
            case DDS300_CHAN_PHS1:
            case DDS300_CHAN_PHS2:
                ra = (chn == DDS300_CHAN_PHS1)? AD9854_R_PAR1 : AD9854_R_PAR2;
                if (action == DRVA_WRITE)
                {
                    if (value < 0)      value = 0;
                    if (value > 0x3FFF) value = 0x3FFF;
                    rflags |= dds300_wr_int(me->N, ra, value, AD9854_R_PAR_BYTES);
                    DDS300_UPDATE();
                }
                rflags |= dds300_rd_int(me->N, ra, &value, AD9854_R_PAR_BYTES);
                break;
                
            case DDS300_CHAN_FRQ1:
            case DDS300_CHAN_FRQ2:
                ra = (chn == DDS300_CHAN_FRQ1)? AD9854_R_FTW1 : AD9854_R_FTW2;
                fquant = ((double)(POW248)) / (Fgen * REF_MULT);
                if (action == DRVA_WRITE)
                {
                    if (value < 0) value = 0;
                    FTW = value * fquant + 0.5;
                    rflags |= dds300_wr_int64(me->N, ra, FTW, AD9854_R_FTW_BYTES);
                    DDS300_UPDATE();
                    DoDriverLog(devid, 0 | DRIVERLOG_C_DATACONV,
                                "write(%d:%02x): value=%d FTW=%016llx",
                                chn, ra, value, FTW);
                }
                rflags |= dds300_rd_int64(me->N, ra, &FTW, AD9854_R_FTW_BYTES);
                value = FTW / fquant + 0.5;
                DoDriverLog(devid, 0 | DRIVERLOG_C_DATACONV,
                            " read(%d:%02x): value=%d FTW=%016llx fq=%e",
                            chn, ra, value, FTW, fquant);
                break;
                
            case DDS300_CHAN_REF_SEL:
                rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_STATUS, 0, &c));
                if (action == DRVA_WRITE)
                {
                    value = value != 0;
                    c = (c &~ (1 << STAT_SH_REF)) | (value << STAT_SH_REF);
                    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_STATUS, 16, &c));
                }
                value = (c >> STAT_SH_REF) & 1;
                break;
                
            case DDS300_CHAN_OUT_SEL:
                rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_STATUS, 0, &c));
                if (action == DRVA_WRITE)
                {
                    value = value != 0;
                    c = (c &~ (1 << STAT_SH_OUT)) | (value << STAT_SH_OUT);
                    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_STATUS, 16, &c));
                }
                value = (c >> STAT_SH_OUT) & 1;
                break;

            case DDS300_CHAN_RESET_TO_DEFAULTS:
                if (action == DRVA_WRITE  &&
                    value  == DDS300_V_RESET_TO_DEFAULTS_KEYVAL)
                    for (ndc = 0;  ndc < countof(def_vals);  ndc++)
                        dds300_rw_p(devid, devptr,
                                    DRVA_WRITE,
                                    1, &(def_vals[ndc].n),
                                    (cxdtype_t[]){CXDTYPE_INT32},
                                    (int      []){1},
                                    (void*    []){&(def_vals[ndc].v)});
                value = 0;
                break;
                
            case DDS300_CHAN_SERIAL:
                rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_SERIAL, 1, &c));
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



DEFINE_CXSD_DRIVER(dds300, "Shubin DDS300",
                   NULL, NULL,
                   sizeof(dds300_privrec_t), NULL,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   dds300_init_d, NULL, dds300_rw_p);
