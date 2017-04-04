#include "cxsd_driver.h"
#include "cda.h"


enum {NUM_REFS = 2};

typedef struct
{
    cda_context_t  cid;
    cda_dataref_t  refs[NUM_REFS];
    double         vals[NUM_REFS];
    rflags_t       rfls[NUM_REFS];
} privrec_t;


static void refs_evproc(int            devid,
                        void          *devptr,
                        cda_dataref_t  ref,
                        int            reason,
                        void          *info_ptr,
                        void          *privptr2)
{
  privrec_t  *me = devptr;
  int         x  = ptr2lint(privptr2);

  double      val;
  rflags_t    rfl;
  void       *val_p = &val;

  static int        n0        = 0;
  static cxdtype_t  dt_double = CXDTYPE_DOUBLE;
  static int        nel1      = 1;

    cda_get_ref_dval(ref, me->vals + x, NULL, NULL, me->rfls + x, NULL);

    val = me->vals[0] + me->vals[1];
    rfl = me->rfls[0] + me->rfls[1];
    ReturnDataSet(devid, 1, 
                  &n0, &dt_double, &nel1,
                  &val_p, &rfl, NULL);
}

static int double_iset_init_d(int devid, void *devptr,
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

    for (x = 0,                     src = auxinfo;
         x < countof(me->refs)  &&  src != NULL  &&  *src != '\0';
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

        if ((me->refs[x] = cda_add_chan(me->cid, NULL, buf, 0,
                                        CXDTYPE_DOUBLE, 1,
                                        CDA_REF_EVMASK_UPDATE,
                                        refs_evproc, lint2ptr(x))) < 0)
        {
            DoDriverLog(devid, 0, "cda_new_chan([%d]=\"%s\"): %s",
                        x, buf, cda_last_err());
            cda_del_context(me->cid);    me->cid = -1;
            return -CXRF_DRV_PROBL;
        }
    }
    if (x != 2)
    {
        DoDriverLog(devid, 0, "2 channel names expected in auxinfo; deactivating");
        cda_del_context(me->cid);    me->cid = -1;
        me->refs[0] = me->refs[1] = -1;
        return -CXRF_CFG_PROBL;
    }

    return DEVSTATE_OPERATING;
}

static void double_iset_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;
  int        x;

    cda_del_context(me->cid);
    me->cid = -1;
    for (x = 0;  x < countof(me->refs);  x++) me->refs[x] = -1;
}

static void double_iset_rw_p(int devid, void *devptr,
                             int action,
                             int count, int *addrs,
                             cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = devptr;
  int        n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int        chn;   // channel
  double     val;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
        if (action == DRVA_WRITE)
        {
            if (nelems[n] != 1) goto NEXT_CHANNEL;
            if      (dtypes[n] == CXDTYPE_DOUBLE) val = *((float64*)(values[n]));
            else if (dtypes[n] == CXDTYPE_INT32  ||
                     dtypes[n] == CXDTYPE_UINT32) val = *((  int32*)(values[n]));
            else goto NEXT_CHANNEL;

            val /= 2;

            cda_snd_ref_data(me->refs[0], CXDTYPE_DOUBLE, 1, &val);
            cda_snd_ref_data(me->refs[1], CXDTYPE_DOUBLE, 1, &val);
        }


 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(double_iset, "double-Iset driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   double_iset_init_d, double_iset_term_d, double_iset_rw_p);
