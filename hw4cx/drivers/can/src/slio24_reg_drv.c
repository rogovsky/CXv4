//#include <stdio.h>
//#include <unistd.h>
//#include <stdlib.h>

#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/slio24_reg_drv_i.h"


/* SLIO24 specifics */

enum
{
    DEVTYPE   = 5, /* SLIO24 is 5 */
};

enum
{
    DESC_DO_READ   = 0x01,
    DESC_DO_WRITE  = 0x02,
    DESC_RD_INPREG = 0x03,
    DESC_ERR_IO    = 0xF0,
};

/*  */

typedef struct
{
    cankoz_vmt_t    *lvmt;
    int              devid;
    int              handle;

    int              hw_ver;
    int              sw_ver;
} privrec_t;


static void slio24_reg_ff(int devid, void *devptr, int is_a_reset);
static void slio24_reg_in(int devid, void *devptr,
                          int desc,  size_t dlc, uint8 *data);

static int slio24_reg_init_d(int devid, void *devptr, 
                             int businfocount, int businfo[],
                             const char *auxinfo)
{
  privrec_t *me    = (privrec_t *) devptr;
    
    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "ENTRY %s(%s)", __FUNCTION__, auxinfo);

    /* Initialize interface */
    me->lvmt   = GetLayerVMT(devid);
    me->devid  = devid;
    me->handle = me->lvmt->add(devid, devptr,
                               businfocount, businfo,
                               DEVTYPE,
                               slio24_reg_ff, slio24_reg_in,
                               SLIO24_REG_NUMCHANS * 2/*!!!*/,
                               CANKOZ_LYR_OPTION_NONE);
    if (me->handle < 0) return me->handle;

    SetChanReturnType(devid, SLIO24_REG_CHAN_HW_VER, 1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, SLIO24_REG_CHAN_SW_VER, 1, IS_AUTOUPDATED_TRUSTED);

    return DEVSTATE_OPERATING;
}


static void slio24_reg_ff(int devid, void *devptr, int is_a_reset)
{
  privrec_t *me = devptr;

    me->lvmt->get_dev_ver(me->handle, &(me->hw_ver), &(me->sw_ver), NULL);
    ReturnInt32Datum(me->devid, SLIO24_REG_CHAN_HW_VER, me->hw_ver, 0);
    ReturnInt32Datum(me->devid, SLIO24_REG_CHAN_SW_VER, me->sw_ver, 0);
}

static void slio24_reg_in(int devid, void *devptr,
                          int desc,  size_t dlc, uint8 *data)
{
  privrec_t  *me     = (privrec_t *) devptr;
  int32       code;
  rflags_t    rflags;

    switch (desc)
    {
        case DESC_DO_READ:
            if (dlc < 4) return;
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);

            code = data[1] + (data[2] << 8) + (data[3] << 16);

            ReturnInt32Datum(devid, SLIO24_REG_CHAN_WORD24, code, 0);
            break;
    }
}

static void slio24_reg_rw_p(int devid, void *devptr,
                            int action,
                            int count, int *addrs,
                            cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = (privrec_t *)devptr;

  int          n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int          chn;   // channel
  int32        val;   // Value

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

        if      (chn == SLIO24_REG_CHAN_WORD24)
        {
            if (action == DRVA_WRITE)
            {
                me->lvmt->q_enq_ons_v(me->handle, SQ_ALWAYS, 4,
                                      DESC_DO_WRITE,
                                      (val)       & 0xFF, 
                                      (val >>  8) & 0xFF,
                                      (val >> 16) & 0xFF);
                me->lvmt->q_enq_v    (me->handle, SQ_ALWAYS,    1, DESC_DO_READ);
            }
            else
                me->lvmt->q_enq_v    (me->handle, SQ_IF_ABSENT, 1, DESC_DO_READ);
        }
        else if (chn == SLIO24_REG_CHAN_HW_VER  ||  chn == SLIO24_REG_CHAN_SW_VER)
        {/* Do-nothing -- returned from _ff() */}
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(slio24_reg, "SLIO24-reg (works as a register) driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   slio24_reg_init_d, NULL, slio24_reg_rw_p);
