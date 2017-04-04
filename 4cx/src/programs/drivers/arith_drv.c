#include "cxsd_driver.h"
#include "cda.h"


enum {MAX_INPUTS = 10};
enum
{
    ARITH_ADD = 1,
    ARITH_MUL = 2,
};

typedef struct
{
    cda_dataref_t  ref;
    double         curval;
    rflags_t       rflags;
    cx_time_t      timestamp;
} inp_props_t;

typedef struct
{
    int            devid;
    cda_context_t  cid;
    inp_props_t    inputs[MAX_INPUTS];
    int            inputs_used;
    int            kind;
} privrec_t;


#define TIMESTAMP_IS_AFTER(a, b) \
    ((a).sec > (b).sec  ||  ((a).sec == (b).sec  &&  (a).nsec > (b).nsec))

static void CalcAndReturn(privrec_t *me)
{
  int        x;
  double     val;
  rflags_t   rflags;
  cx_time_t  timestamp;
  void      *val_p = &val;

  static int        addr_zero    = 0;
  static cxdtype_t  dtype_double = CXDTYPE_DOUBLE;
  static int        nelems_one   = 1;

    if (me->kind == ARITH_MUL) val = 1.0; else val = 0.0;

    for (x = 0, rflags = 0, timestamp = (cx_time_t){0x7FFFFFFF/*!!!*/,0};
         x < me->inputs_used;
         x++)
    {
        if      (me->kind == ARITH_ADD) val += me->inputs[x].curval;
        else if (me->kind == ARITH_MUL) val *= me->inputs[x].curval;

        rflags |= me->inputs[x].rflags;
        if (TIMESTAMP_IS_AFTER(timestamp, me->inputs[x].timestamp))
            timestamp = me->inputs[x].timestamp;
    }

    ReturnDataSet(me->devid, 0,
                  &addr_zero, &dtype_double, &nelems_one,
                  &val_p, &rflags, &timestamp);
}

static void input_evproc(int            devid,
                         void          *devptr,
                         cda_dataref_t  ref,
                         int            reason,
                         void          *info_ptr,
                         void          *privptr2)
{
  privrec_t  *me = devptr;
  int         x  = ptr2lint(privptr2);
    
    if (x < 0  ||  x >= countof(me->inputs)) return; /*!!!?*/

    cda_get_ref_dval(me->inputs[x].ref,
                     &(me->inputs[x].curval),
                     NULL, NULL,
                     &(me->inputs[x].rflags), &(me->inputs[x].timestamp));

    CalcAndReturn(me);
}

static int arith_init_d(int devid, void *devptr,
                        int businfocount, int businfo[],
                        const char *auxinfo)
{
  privrec_t  *me = devptr;
  int         x;
  const char *src;
  const char *beg;
  size_t      len;
  char        buf[1000];

    me->devid = devid;

    src = GetDevTypename(devid);
    if (src == NULL)
    {
        DoDriverLog(devid, 0, "GetDevTypename()=NULL");
        return -CXRF_CFG_PROBL;
    }
    if      (strcasecmp(src, "add") == 0)
        me->kind = ARITH_ADD;
    else if (strcasecmp(src, "mul") == 0)
        me->kind = ARITH_MUL;
    else
    {
        DoDriverLog(devid, 0, "unknown type \"%s\"", src);
        return -CXRF_CFG_PROBL;
    }

    if ((me->cid = cda_new_context(devid, devptr,
                                   "insrv::", 0,
                                   NULL,
                                   0, NULL, 0)) < 0)
    {
        DoDriverLog(devid, 0, "cda_new_context(): %s", cda_last_err());
        return -CXRF_DRV_PROBL;
    }

    for (x = 0,                       src = auxinfo;
         x < countof(me->inputs)  &&  src != NULL  &&  *src != '\0';
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

        if ((me->inputs[x].ref = cda_add_chan(me->cid, NULL, buf, 0,
                                              CXDTYPE_INT32, 1,
                                              CDA_REF_EVMASK_UPDATE,
                                              input_evproc, lint2ptr(x))) < 0)
        {
            DoDriverLog(devid, 0, "cda_new_chan([%d]=\"%s\"): %s",
                        x, buf, cda_last_err());
            cda_del_context(me->cid);    me->cid = -1;
            return -CXRF_DRV_PROBL;
        }
    }
    if (x == 0)
    {
        DoDriverLog(devid, 0, "no channels to calc, deactivating");
        cda_del_context(me->cid);    me->cid = -1;
        return -CXRF_CFG_PROBL;
    }
    me->inputs_used = x;

    SetChanReturnType(devid, 0, 1, IS_AUTOUPDATED_YES);

    return DEVSTATE_OPERATING;
}

static void arith_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;
  int        x;

    cda_del_context(me->cid);
    me->cid = -1;
    for (x = 0;  x < countof(me->inputs);  x++) me->inputs[x].ref = -1;
}

static void arith_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = devptr;

    CalcAndReturn(me);
}


DEFINE_CXSD_DRIVER(mux_write, "Simple arithmetics driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   arith_init_d, arith_term_d, arith_rw_p);
