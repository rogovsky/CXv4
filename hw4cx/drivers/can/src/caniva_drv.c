#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "misc_macros.h"
#include "cxsd_driver.h"

#include "cankoz_lyr.h"
#include "drv_i/caniva_drv_i.h"


/* CANIVA specifics */

enum
{
    DEVTYPE   = 17, /* CANIVA is 17 */
};

static inline int chan2cset(int chan)
{
    return chan & 0x0F;
}

static void CalcAlarms(int32 *curvals, int cset)
{
    curvals[CANIVA_CHAN_IALM_n_base + cset] =
        (curvals[CANIVA_CHAN_ILIM_n_base + cset] == 0)? 0
        : (curvals[CANIVA_CHAN_IMES_n_base + cset] > curvals[CANIVA_CHAN_ILIM_n_base + cset]);
    
    curvals[CANIVA_CHAN_UALM_n_base + cset] =
        (curvals[CANIVA_CHAN_ULIM_n_base + cset] == 0)? 0
        : (curvals[CANIVA_CHAN_UMES_n_base + cset] < curvals[CANIVA_CHAN_ULIM_n_base + cset]);
}

enum
{
    DESC_VAL_n_BASE   = 0x00,
    DESC_READ_n_BASE  = 0x10,
    DESC_WRITEDEVSTAT = 0xFD,
};


/*  */

typedef struct
{
    cankoz_vmt_t     *lvmt;
    int               handle;

    int32             curvals[CANIVA_NUMCHANS];
} privrec_t;


static void caniva_ff(int devid, void *devptr, int is_a_reset);
static void caniva_in(int devid, void *devptr,
                      int desc, size_t dlc, uint8 *data);

static int caniva_init_d(int devid, void *devptr, 
                         int businfocount, int businfo[],
                         const char *auxinfo)
{
  privrec_t *me    = (privrec_t *) devptr;
  int        cset;
    
    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "ENTRY %s(%s)", __FUNCTION__, auxinfo);

    /* Initialize interface */
    me->lvmt   = GetLayerVMT(devid);
    me->handle = me->lvmt->add(devid, devptr,
                               businfocount, businfo,
                               DEVTYPE,
                               caniva_ff, caniva_in,
                               10);
    if (me->handle < 0) return me->handle;

    for (cset = 0;  cset < CANIVA_CHAN_ILIM_n_count; cset++)
    {
        me->curvals[CANIVA_CHAN_ILIM_n_base + cset] = 2560;
        me->curvals[CANIVA_CHAN_ULIM_n_base + cset] = 20;
    }

    SetChanRDs       (devid, CANIVA_CHAN_IMES_n_base, CANIVA_CHAN_IMES_n_count, 10.0, 0.0);
    SetChanRDs       (devid, CANIVA_CHAN_UMES_n_base, CANIVA_CHAN_UMES_n_count, 10.0, 0.0);
    SetChanRDs       (devid, CANIVA_CHAN_ILIM_n_base, CANIVA_CHAN_ILIM_n_count, 10.0, 0.0);
    SetChanRDs       (devid, CANIVA_CHAN_ULIM_n_base, CANIVA_CHAN_ULIM_n_count, 10.0, 0.0);
    SetChanReturnType(devid, CANIVA_CHAN_IMES_n_base, CANIVA_CHAN_IMES_n_count, IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, CANIVA_CHAN_UMES_n_base, CANIVA_CHAN_UMES_n_count, IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, CANIVA_CHAN_IALM_n_base, CANIVA_CHAN_IALM_n_count, IS_AUTOUPDATED_YES);
    SetChanReturnType(devid, CANIVA_CHAN_UALM_n_base, CANIVA_CHAN_UALM_n_count, IS_AUTOUPDATED_YES);

    return DEVSTATE_OPERATING;
}

static void caniva_term_d(int devid, void *devptr __attribute__((unused)))
{
  privrec_t *me    = (privrec_t *) devptr;

    me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                          SQ_TRIES_DIR, 0,
                          NULL, NULL,
                          0, 4, DESC_WRITEDEVSTAT, 0, 0, 0);
}

static void caniva_ff(int   devid    __attribute__((unused)),
                      void *devptr,
                      int   is_a_reset __attribute__((unused)))
{
  privrec_t  *me     = (privrec_t *) devptr;

    me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS,
                          SQ_TRIES_ONS/*!!!*/, 0,
                          NULL, NULL,
                          0, 4, DESC_WRITEDEVSTAT, 0xFF, 0xFF, 1);
}

static void caniva_in(int devid, void *devptr __attribute__((unused)),
                      int desc, size_t dlc, uint8 *data)
{
  privrec_t  *me     = (privrec_t *) devptr;
  int         cset;

  int32             vals  [4];
  int               addrs [4];
  static cxdtype_t  dtypes[4] = {CXDTYPE_INT32, CXDTYPE_INT32, CXDTYPE_INT32, CXDTYPE_INT32};
  static int        nelems[4] = {1, 1, 1, 1};
  void             *vals_p[4];
  static rflags_t   rflags[4] = {0, 0, 0, 0};

    switch (desc)
    {
        case DESC_VAL_n_BASE  ... DESC_VAL_n_BASE  + 0xF:
        case DESC_READ_n_BASE ... DESC_READ_n_BASE + 0xF:
            if (dlc < 4) return;
            cset = desc & 0x0F;
            me->curvals[CANIVA_CHAN_IMES_n_base + cset] = (data[1] * 256 + data[2]) * 2 / 3; /* That f@#king CANIVA uses 300ms integration period, so that all measurements are 1.5 times bigger than they should be... :-( */
            me->curvals[CANIVA_CHAN_UMES_n_base + cset] = data[3] * 2 / 3;
            CalcAlarms(me->curvals, cset);
            addrs[0] = CANIVA_CHAN_IMES_n_base + cset;  vals[0] = me->curvals[CANIVA_CHAN_IMES_n_base + cset];  vals_p[0] = vals + 0;
            addrs[1] = CANIVA_CHAN_UMES_n_base + cset;  vals[1] = me->curvals[CANIVA_CHAN_UMES_n_base + cset];  vals_p[1] = vals + 1;
            addrs[2] = CANIVA_CHAN_IALM_n_base + cset;  vals[2] = me->curvals[CANIVA_CHAN_IALM_n_base + cset];  vals_p[2] = vals + 2;
            addrs[3] = CANIVA_CHAN_UALM_n_base + cset;  vals[3] = me->curvals[CANIVA_CHAN_UALM_n_base + cset];  vals_p[3] = vals + 3;
            ReturnDataSet(devid,
                          4,
                          addrs, dtypes, nelems, vals_p, rflags, NULL);
            break;
    }
}

static void caniva_rw_p(int devid, void *devptr,
                        int action,
                        int count, int *addrs,
                        cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me     = (privrec_t *) devptr;
  int        n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int        chn;   // channel
  int32      val;   // Value

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

        /* "Set limit"? */
        if (action == DRVA_WRITE  &&
            (chn >= CANIVA_CHAN_ILIM_n_base  &&
             chn <= CANIVA_CHAN_ULIM_n_base+CANIVA_CHAN_ULIM_n_count-1))
        {
            me->curvals[chn] = val;
            CalcAlarms(me->curvals, chan2cset(chn));
        }

        /* Any artificial channel -- return it immediately */
        if (chn >= CANIVA_CHAN_ILIM_n_base  &&
            chn <= CANIVA_CHAN_UALM_n_base+CANIVA_CHAN_UALM_n_count-1)
            ReturnInt32Datum(devid, chn, me->curvals[chn], 0);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(caniva, "Gudkov/Tararyshkin CAN-IVA",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   caniva_init_d, caniva_term_d, caniva_rw_p);
