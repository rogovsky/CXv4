//#include <stdio.h>
//#include <unistd.h>
//#include <stdlib.h>

#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/cpks8_drv_i.h"


/* CPKS8 specifics */

enum
{
    DEVTYPE   = 7, /* CPKS is 7 */
};

enum
{
    DESC_WRITE_n_BASE  = 0,
    DESC_READ_n_BASE   = 0x10,
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


static void cpks8_ff(int devid, void *devptr, int is_a_reset);
static void cpks8_in(int devid, void *devptr,
                     int desc,  size_t dlc, uint8 *data);

static int cpks8_init_d(int devid, void *devptr, 
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
                               cpks8_ff, cpks8_in,
                               CPKS8_NUMCHANS * 2/*!!!*/);
    if (me->handle < 0) return me->handle;

    SetChanReturnType(devid, CPKS8_CHAN_HW_VER, 1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, CPKS8_CHAN_SW_VER, 1, IS_AUTOUPDATED_TRUSTED);

    return DEVSTATE_OPERATING;
}


static void cpks8_ff(int devid, void *devptr, int is_a_reset)
{
  privrec_t *me = devptr;

    me->lvmt->get_dev_ver(me->handle, &(me->hw_ver), &(me->sw_ver), NULL);
    ReturnInt32Datum(me->devid, CPKS8_CHAN_HW_VER, me->hw_ver, 0);
    ReturnInt32Datum(me->devid, CPKS8_CHAN_SW_VER, me->sw_ver, 0);
}

static void cpks8_in(int devid, void *devptr,
                     int desc,  size_t dlc, uint8 *data)
{
  privrec_t  *me     = (privrec_t *) devptr;
  int         l;
  int32       code;
  rflags_t    rflags;

    switch (desc)
    {
        case DESC_READ_n_BASE ... DESC_READ_n_BASE + CPKS8_CHAN_OUT_n_count-1:
            if (dlc < 3) return;
            me->lvmt->q_erase_and_send_next_v(me->handle, 1, desc);

            l    = desc - DESC_READ_n_BASE;
            code = data[1] + (data[2] << 8);

            ReturnInt32Datum(devid, CPKS8_CHAN_OUT_n_base + l, code, 0);
            break;
    }
}

static void cpks8_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = (privrec_t *)devptr;

  int          n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int          chn;   // channel
  int          l;     // Line #
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

        if      (chn >= CPKS8_CHAN_OUT_n_base  &&
                 chn <= CPKS8_CHAN_OUT_n_base+CPKS8_CHAN_OUT_n_count-1)
        {
            l = chn - CPKS8_CHAN_OUT_n_base;

            if (action == DRVA_WRITE)
            {
                if (val < 0)     val = 0;
                if (val > 65535) val = 65535;

                /*!!! Should use SQ_REPLACE_NOTFIRST and send READ only if NOTFOUND */
                me->lvmt->q_enq_ons_v(me->handle, SQ_ALWAYS, 3,
                                      DESC_WRITE_n_BASE + l,
                                      (val)      & 0xFF,
                                      (val >> 8) & 0xFF);
                me->lvmt->q_enq_v    (me->handle, SQ_ALWAYS, 1,
                                      DESC_READ_n_BASE + l);
            }
            else
                me->lvmt->q_enq_v    (me->handle, SQ_IF_NONEORFIRST, 1,
                                      DESC_READ_n_BASE  + l);
        }
        else if (chn == CPKS8_CHAN_HW_VER  ||  chn == CPKS8_CHAN_SW_VER)
        {/* Do-nothing -- returned from _ff() */}
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(cpks8, "CPKS8 driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   cpks8_init_d, NULL, cpks8_rw_p);
