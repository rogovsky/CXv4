#include "cxsd_driver.h"

#include "drv_i/dds500_drv_i.h"


enum
{
    A_BUF01    = 0,
    A_BUF23    = 1,
    A_BUFIN    = 2,

    A_STATUS   = 5,
    A_INFOWORD = 15,
};

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

static rflags_t rd_stat(int N, int *v_p)
{
    return x2rflags(DO_NAF(CAMAC_REF, N, A_STATUS, 0, v_p));
}

static rflags_t wr_stat(int N, int v)
{
    return x2rflags(DO_NAF(CAMAC_REF, N, A_STATUS, 16, &v));
}


//////////////////////////////////////////////////////////////////////

typedef struct
{
    int  N;
    
    int32     infoword;
    rflags_t  infoword_rflags;
} dds500_privrec_t;



static int dds500_init_d(int devid, void *devptr,
                         int businfocount, int *businfo,
                         const char *auxinfo)
{
  dds500_privrec_t *me = (dds500_privrec_t*)devptr;
  int               c;
  
    me->N = businfo[0];

    me->infoword_rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_INFOWORD, 1, &(me->infoword)));
    
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
  int               ra;    // Reg endAddress
  
  int               c;
  int32             value;
  rflags_t          rflags;
  int               junk;

  int               ndc;
  struct {int n; int32 v;} def_vals[] =
  {
  };

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

            case DDS500_CHAN_RESET_TO_DEFAULTS:
                if (action == DRVA_WRITE  &&
                    value  == DDS500_V_RESET_TO_DEFAULTS_KEYVAL)
                    for (ndc = 0;  ndc < countof(def_vals);  ndc++)
                        /*dds500_rw_p(devid, devptr,
                                    def_vals[ndc].n, 1,
                                    &(def_vals[ndc].v), DRVA_WRITE)*/;
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
                   sizeof(dds500_privrec_t), NULL,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   dds500_init_d, NULL, dds500_rw_p);
