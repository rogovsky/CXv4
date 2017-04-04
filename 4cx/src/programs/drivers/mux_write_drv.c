#include "cxsd_driver.h"
#include "cda.h"


enum {MAX_OUTS = 10};

typedef struct
{
    cda_context_t  cid;
    cda_dataref_t  out_r[MAX_OUTS];
    int            out_r_used;
} privrec_t;


static int mux_write_init_d(int devid, void *devptr,
                            int businfocount, int businfo[],
                            const char *auxinfo)
{
  privrec_t  *me = devptr;
  int         x;
  const char *src;
  const char *beg;
  size_t      len;
  char        buf[1000];

    if ((me->cid = cda_new_context(devid, devptr,
                                   "insrv::", 0,
                                   NULL,
                                   0, NULL, 0)) < 0)
    {
        DoDriverLog(devid, 0, "cda_new_context(): %s", cda_last_err());
        return -CXRF_DRV_PROBL;
    }

    for (x = 0,                      src = auxinfo;
         x < countof(me->out_r)  &&  src != NULL  &&  *src != '\0';
         x++)
    {
        /* Skip whitespace */
        while (*src != '\0'  &&  isspace(*src)) src++;
        if (*src == '\0') break;

        beg = src;
        while (*src != '\0'  &&  !isspace(*src)) src++;
        len = src - beg;
        if (len > sizeof(buf) - 1)
            len = sizeof(buf) - 1;
        memcpy(buf, beg, len); buf[len] = '\0';

        if ((me->out_r[x] = cda_add_chan(me->cid, NULL, buf, 0,
                                         CXDTYPE_INT32, 1,
                                         0,
                                         NULL, NULL)) < 0)
        {
            DoDriverLog(devid, 0, "cda_new_chan([%d]=\"%s\"): %s",
                        x, buf, cda_last_err());
            cda_del_context(me->cid);    me->cid = -1;
            return -CXRF_DRV_PROBL;
        }
    }
    if (x == 0)
    {
        DoDriverLog(devid, 0, "no channels to multiplex to, deactivating");
        cda_del_context(me->cid);    me->cid = -1;
        return -CXRF_CFG_PROBL;
    }
    me->out_r_used = x;

    return DEVSTATE_OPERATING;
}

static void mux_write_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;
  int        x;

    cda_del_context(me->cid);
    me->cid = -1;
    for (x = 0;  x < countof(me->out_r);  x++) me->out_r[x] = -1;
}

static void mux_write_rw_p(int devid, void *devptr,
                           int action,
                           int count, int *addrs,
                           cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = devptr;
  int        n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int        chn;   // channel
  int32      val;
  int        x;

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

            for (x = 0;  x < countof(me->out_r);  x++)
                if (cda_ref_is_sensible(me->out_r[x]))
                    cda_snd_ref_data(me->out_r[x], CXDTYPE_INT32, 1, &val);
        }

        ReturnInt32Datum(devid, chn, 0, 0);
 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(mux_write, "Write-multiplexor driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   mux_write_init_d, mux_write_term_d, mux_write_rw_p);
