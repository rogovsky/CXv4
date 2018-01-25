#include <stdio.h>

#include "cxsd_driver.h"
#include "drv_i/vdac20_drv_i.h"


enum {ADDRESS_MODIFIER = 0x29};

enum
{
    KREG_IO  = 0,
};

enum
{
    KCMD_WR_DACL      = 0,
    KCMD_WR_DACM      = 1,
    KCMD_WR_DACH      = 2,
    KCMD_DO_CALIBRATE = 3,
    KCMD_SET_DIGCORR  = 4,
    KCMD_READ_MEM     = 5,
};

static inline uint16 KCMD(uint8 cmd, uint8 modifier)
{
    return (((uint16)cmd) << 8) | modifier;
}

enum
{
    ADDR_FLAG1         = 34,

    ADDR_CHANNEL       = 36,

    ADDR_CORF          = 45,

    ADDR_DACL          = 89, // 0x59
    ADDR_DACM          = 90, // 0x5a
    ADDR_DACH          = 91, // 0x5b

    ADDR_CORL          = 100,
    ADDR_CORM          = 101,
    ADDR_CORH          = 102,

    ADDR_SWversion     = 113,
    ADDR_HWversion     = 114,
    
    ADDR_CHNNDATA_BASE = 128,
};

enum
{
    ADDR_CORF_FLAG_ON    = 1 << 0,
    ADDR_CORF_FLAG_VALID = 1 << 1,
};

enum
{
    MAX_ALWD_VAL =   9999999,
    MIN_ALWD_VAL = -10000000,
    THE_QUANT    = 2,
};

static inline int32 dac20_daccode_to_val(uint32 kozak32)
{
    return scale32via64((int32)kozak32 - 0x800000, 10000000, 0x800000);
}

static inline int32 dac20_val_to_daccode(int32 val)
{
    return scale32via64(val, 0x800000, 10000000) + 0x800000;
}


typedef struct
{
    int  iohandle;
} privrec_t;


static uint8 KVRdByte(privrec_t *me, uint8 addr)
{
  uint16  w;

    bivme2_io_a16wr16(me->iohandle, KREG_IO, KCMD(KCMD_READ_MEM, addr));
    bivme2_io_a16rd16(me->iohandle, KREG_IO, &w);

    return w & 0xFF;
}


static void ReturnAdcChan(int devid, privrec_t *me, int l)
{
  int32       code;
  int32       cod2;
  int32       val;
  rflags_t    rflags;

    if (l < 0  ||  l > VDAC20_CHAN_ADC_n_count) return;

    /* Read twice, ... */
    code =
        (KVRdByte(me, ADDR_CHNNDATA_BASE + l * 4 + 0)     ) |
        (KVRdByte(me, ADDR_CHNNDATA_BASE + l * 4 + 1) << 8) |
        (KVRdByte(me, ADDR_CHNNDATA_BASE + l * 4 + 2) << 16) ;
    cod2 =
        (KVRdByte(me, ADDR_CHNNDATA_BASE + l * 4 + 0)     ) |
        (KVRdByte(me, ADDR_CHNNDATA_BASE + l * 4 + 1) << 8) |
        (KVRdByte(me, ADDR_CHNNDATA_BASE + l * 4 + 2) << 16) ;

    /* ...and if results differ (an extremely rare probability --
     only if we managed to read when device updates value), then ... */
    if (cod2 != code)
        /* ...repeat read */
        code =
            (KVRdByte(me, ADDR_CHNNDATA_BASE + l * 4 + 0)     ) |
            (KVRdByte(me, ADDR_CHNNDATA_BASE + l * 4 + 1) << 8) |
            (KVRdByte(me, ADDR_CHNNDATA_BASE + l * 4 + 2) << 16) ;

    if ((code &  0x800000) != 0)
        code -= 0x1000000;
    
    val = scale32via64(code, 10000000, 0x3fffff);
    rflags = (code >= -0x3FFFFF && code <= 0x3FFFFF)? 0 : CXRF_OVERLOAD;

    ReturnInt32Datum(devid, VDAC20_CHAN_ADC_n_base + l, val, rflags);
}


static int init_d(int devid, void *devptr,
                  int businfocount, int *businfo,
                  const char *auxinfo)
{
  privrec_t  *me = (privrec_t*)devptr;

  int         jumpers;

    if (businfocount < 1) return -CXRF_CFG_PROBL;
    jumpers      = businfo[0] & 0xFFF;

    me->iohandle = bivme2_io_open(devid, devptr,
                                  jumpers << 4, 2, ADDRESS_MODIFIER,
                                  0, NULL);
    if (me->iohandle < 0)
    {
        DoDriverLog(devid, 0, "open: %d/%s", me->iohandle, strerror(errno));
        return -CXRF_DRV_PROBL;
    }

    SetChanRDs  (devid, VDAC20_CHAN_ADC_n_base, VDAC20_CHAN_ADC_n_count, 1000000.0, 0.0);
    SetChanRDs  (devid, VDAC20_CHAN_OUT_n_base, VDAC20_CHAN_OUT_n_count, 1000000.0, 0.0);
    SetChanQuant(devid, VDAC20_CHAN_OUT_n_base, VDAC20_CHAN_OUT_n_count, (CxAnyVal_t){.i32=THE_QUANT}, CXDTYPE_INT32);

    return DEVSTATE_OPERATING;
}

static void term_d(int devid, void *devptr)
{
  privrec_t  *me = (privrec_t*)devptr;

    bivme2_io_close  (me->iohandle);
}

static void rdwr_p(int devid, void *devptr,
                   int action,
                   int count, int *addrs,
                   cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t  *me = (privrec_t*)devptr;

  int         n;
  int         chn;
  int         l;

  int32       code;
  int32       val;
  rflags_t    rflags;

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

        switch (chn)
        {
            case VDAC20_CHAN_ADC_n_base ... VDAC20_CHAN_ADC_n_base + VDAC20_CHAN_ADC_n_count-1:
                l = chn - VDAC20_CHAN_ADC_n_base;
                ReturnAdcChan(devid, me, l);
                break;

            case VDAC20_CHAN_OUT_n_base:
                if (action == DRVA_WRITE)
                {
                    if (val > MAX_ALWD_VAL) val = MAX_ALWD_VAL;
                    if (val < MIN_ALWD_VAL) val = MIN_ALWD_VAL;
                    code = dac20_val_to_daccode(val);

                    bivme2_io_a16wr16(me->iohandle, KREG_IO,
                                      KCMD(KCMD_WR_DACL, code         & 0xFF));
                    bivme2_io_a16wr16(me->iohandle, KREG_IO,
                                      KCMD(KCMD_WR_DACM, (code >>  8) & 0xFF));
                    bivme2_io_a16wr16(me->iohandle, KREG_IO,
                                      KCMD(KCMD_WR_DACH, (code >> 16) & 0xFF));
                }
                code =
                    (((int32)KVRdByte(me, ADDR_DACL))      ) |
                    (((int32)KVRdByte(me, ADDR_DACM)) << 8 ) |
                    (((int32)KVRdByte(me, ADDR_DACH)) << 16);
                ReturnInt32Datum(devid, chn, dac20_daccode_to_val(code), 0);
                break;

            case VDAC20_CHAN_DO_CALIBRATE:
                if (action == DRVA_WRITE  &&  val == CX_VALUE_COMMAND)
                {
                    bivme2_io_a16wr16(me->iohandle, KREG_IO,
                                      KCMD(KCMD_DO_CALIBRATE, 0));
                }
                ReturnInt32Datum(devid, chn, 0, 0);
                break;

            case VDAC20_CHAN_DIGCORR_MODE:
                if (action == DRVA_WRITE)
                {
                    /* Note: "digcorr on" is switched by bit7,
                             but its status is read in bit0 */
                    bivme2_io_a16wr16(me->iohandle, KREG_IO,
                                      KCMD(KCMD_SET_DIGCORR, (val & 1) << 7));
                }
                ReturnInt32Datum(devid, chn,
                                 (KVRdByte(me, ADDR_CORF)) & ADDR_CORF_FLAG_ON,
                                 0);
                break;

            case VDAC20_CHAN_DIGCORR_V:
                val    = 0;
                rflags = 0;
                code = KVRdByte(me, ADDR_CORF);
                if      ((code & ADDR_CORF_FLAG_ON)    == 0)
                    ;
                else if ((code & ADDR_CORF_FLAG_VALID) == 0)
                    rflags = CXRF_OVERLOAD;
                else
                {
                    val =
                        (((int32)KVRdByte(me, ADDR_CORL))      ) |
                        (((int32)KVRdByte(me, ADDR_CORM)) << 8 ) |
                        (((int32)KVRdByte(me, ADDR_CORH)) << 16);
                }
                ReturnInt32Datum(devid, chn, val, rflags);
                break;

            default:
                ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);
        }
 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(vadc16, "VDAC20 VME DAC driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   init_d, term_d, rdwr_p);
