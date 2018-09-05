#include <ctype.h>

#include "cxsd_driver.h"
#include "cda.h"

#include "cxsd_hwP.h"


typedef struct
{
    cda_context_t  cid;
    cda_dataref_t  chan_r;
} privrec_t;


static void chan_evproc(int            devid,
                        void          *devptr,
                        cda_dataref_t  ref,
                        int            reason,
                        void          *info_ptr,
                        void          *privptr2)
{
  privrec_t  *me = devptr;
  cxdtype_t   dtype;
  int         nelems;
  void       *buf;
  rflags_t    rflags;
  cx_time_t   timestamp;
  int         phys_count;
  double     *phys_rds;
  cx_time_t   fresh_age;
  const char *src_p;

  static int  chan_zero = 0;

    if      (reason == CDA_REF_R_UPDATE)
    {
        /* Note: 
             we use "ref" instead of "me->chan_r" because for "insrv::" refs
             this is first called during initialization,
             upon cda_add_chan(), when me->chan_r isn't filled yet
             and is still ==-1 */
        dtype  = cda_current_dtype_of_ref (ref);
        nelems = cda_current_nelems_of_ref(ref);
        cda_acc_ref_data                  (ref, &buf,     NULL);
        cda_get_ref_stat                  (ref, &rflags, &timestamp);
        ReturnDataSet(devid, 1,
                      &chan_zero, &dtype, &nelems,
                      &buf, &rflags, &timestamp);
    }
    else if (reason == CDA_REF_R_RDSCHG)
    {
        if (cda_phys_rds_of_ref(ref, &phys_count, &phys_rds) == 0  &&
            phys_count > 1)
            SetChanRDs     (devid, 0, 1, phys_rds[0], phys_rds[1]);
            
    }
    else if (reason == CDA_REF_R_FRESHCHG)
    {
        /* Note: <0 -- error, ==0 -- no fresh_age specified, >0 -- specified */
        if (cda_fresh_age_of_ref(ref, &fresh_age) > 0)
            SetChanFreshAge(devid, 0, 1, fresh_age);
        /* Note 2: is this a valid approach?  If 
                   1) some fresh age is specified;
                   2) reconnect happens,
                   3) ...with no fresh age specified in the new incarnation
                   4) thus the OLD fresh age remains in effect...
           Is it fixable at all? */
    }
    else if (reason == CDA_REF_R_RSLVSTAT)
    {
        if (ptr2lint(info_ptr) == CDA_RSLVSTAT_NOTFOUND)
        {
            if (cda_src_of_ref(ref, &src_p) < 0) src_p = "???";
            DoDriverLog(devid, 0, "target channel \"%s\" not found", src_p);
        }
    }
}

static int  mirror_init_d(int devid, void *devptr,
                          int businfocount, int businfo[],
                          const char *auxinfo)
{
  privrec_t  *me = devptr;
  const char *p;

  int         no_rds_mask;
  cxdtype_t   dtype;
  cxdtype_t   is_unsigned_mask;
  char        ut_char;
  int         n_items;
  char       *endptr;

    me->cid = -1;
    me->chan_r = -1;

    no_rds_mask = 0;
    dtype   = CXDTYPE_UNKNOWN;
    n_items = -1;

    /* Parse auxinfo */
    if (auxinfo == NULL)
    {
        DoDriverLog(devid, 0, "%s(): empty auxinfo", __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }

    p = auxinfo;
    if (*p == '@')
    {
        p++;

        is_unsigned_mask = 0;

        /* Optional modifiers */
        while (1)
        {
            if      (*p == '+') is_unsigned_mask = CXDTYPE_USGN_MASK;
            else if (*p == '.') no_rds_mask      = CDA_DATAREF_OPT_NO_RD_CONV;
            else break;
            p++;
        }

        /* An OPTIONAL data-type designator */
        ut_char = tolower(*p);
        if (ut_char == '\0')
        {
            DoDriverLog(devid, 0, "data-chan spec expected after '@'");
            return -CXRF_CFG_PROBL;
        }
        else if (isdigit(ut_char)  ||
                 ut_char == ':')
            p--; /* Counter-effect for "p++" below */
        else if (ut_char == 'b') dtype = CXDTYPE_INT8  | is_unsigned_mask;
        else if (ut_char == 'h') dtype = CXDTYPE_INT16 | is_unsigned_mask;
        else if (ut_char == 'i') dtype = CXDTYPE_INT32 | is_unsigned_mask;
        else if (ut_char == 'q') dtype = CXDTYPE_INT64 | is_unsigned_mask;
        else if (ut_char == 's') dtype = CXDTYPE_SINGLE;
        else if (ut_char == 'd') dtype = CXDTYPE_DOUBLE;
        else if (ut_char == 't') dtype = CXDTYPE_TEXT;  
        else if (ut_char == 'u') dtype = CXDTYPE_UCTEXT;
        else if (ut_char == 'x') dtype = CXDTYPE_UNKNOWN;
        else
        {
            DoDriverLog(devid, 0, "invalid data-chan dtype after '@'");
            return -CXRF_CFG_PROBL;
        }
        p++;

        /* An also optional NELEMS */
        if (isdigit(*p))
        {
            /* Note: the usual "if (endptr==p)" check is useless,
                     since 'p' points to a digit, thus strtol() will ALWAYS
                     yeild some number;
                     a check for n_items<0 is also useless because of
                     the same reason. */
            n_items = strtol(p, &endptr, 0);
            p = endptr;
            /* ...but 0xFFFFFFFF yeilds -1 on 32-bit systems;
               so, we MUST check for n_items<0! */
            if (n_items < 0)
            {
                DoDriverLog(devid, 0, "negative n_items (%d=0x%d)", n_items, n_items);
                return -CXRF_CFG_PROBL;
            }
        }

        /* Now, a MANDATORY ':' separator */
        if (*p != ':')
        {
            DoDriverLog(devid, 0, "':' expected after '@...' spec");
            return -CXRF_CFG_PROBL;
        }
        p++;

        /* And one more check for non-empty data-chan spec */
        if (ut_char == '\0')
        {
            DoDriverLog(devid, 0, "data-chan spec expected after '@...:'");
            return -CXRF_CFG_PROBL;
        }
    }

    /* Select n_items if not specified */
    if (n_items < 0) n_items = (dtype != CXDTYPE_UNKNOWN)? 1
                                                         : sizeof(CxAnyVal_t);

    if ((me->cid = cda_new_context(devid, devptr,
                                   NULL, 0,
                                   NULL,
                                   0, NULL, 0)) < 0)
    {
        DoDriverLog(devid, 0, "cda_new_context(): %s", cda_last_err());
        return -CXRF_DRV_PROBL;
    }
    if ((me->chan_r = cda_add_chan(me->cid, NULL, p,
                                   no_rds_mask,
                                   dtype, n_items,
                                   CDA_REF_EVMASK_UPDATE | 
                                     (no_rds_mask == 0 ? 0
                                                       : CDA_REF_EVMASK_RDSCHG)|
                                     CDA_REF_EVMASK_FRESHCHG |
                                     CDA_REF_EVMASK_RSLVSTAT,
                                   chan_evproc, NULL)) < 0)
    {
        DoDriverLog(devid, 0, "cda_new_chan(\"%s\"): %s",
                    p, cda_last_err());
        cda_del_context(me->cid);    me->cid    = -1;
        return -CXRF_DRV_PROBL;
    }
    SetChanReturnType(devid, 0, 1, IS_AUTOUPDATED_YES);
    
    return DEVSTATE_OPERATING;
}

static void mirror_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;

    cda_del_context(me->cid);
    me->cid = -1;
    me->chan_r = -1;
}

static void mirror_rw_p(int devid, void *devptr,
                        int action,
                        int count, int *addrs,
                        cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = devptr;
}


DEFINE_CXSD_DRIVER(mirror, "Mirror-channel driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   mirror_init_d, mirror_term_d, mirror_rw_p);
