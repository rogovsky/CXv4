#include <ctype.h>

#include "cxsd_driver.h"
#include "cda.h"

#include "cxsd_hwP.h"


typedef struct
{
    cda_context_t  cid;
    cda_dataref_t  trgr_r;
    cda_dataref_t  data_r;
    int            n_to_skip;   // '/'=>1 means "return FIRST value after trigger"
    int            was_trigger;
    int            skip_count;
} privrec_t;


static void trgr_evproc(int            devid,
                        void          *devptr,
                        cda_dataref_t  ref,
                        int            reason,
                        void          *info_ptr,
                        void          *privptr2)
{
  privrec_t  *me = devptr;
  const char *src_p;

    if      (reason == CDA_REF_R_UPDATE)
    {
        if (me->was_trigger) return;

        me->was_trigger = 1;
        me->skip_count  = 0;
    }
    else if (reason == CDA_REF_R_RSLVSTAT)
    {
        if (ptr2lint(info_ptr) == CDA_RSLVSTAT_NOTFOUND)
        {
            if (cda_src_of_ref(ref, &src_p) < 0) src_p = "???";
            DoDriverLog(devid, 0, "trigger channel \"%s\" not found", src_p);
        }
    }
}

static void data_evproc(int            devid,
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
        if (!me->was_trigger) return;
        me->skip_count++;
        if (me->skip_count >= me->n_to_skip)
        {
            /* Note: 
                 we use "ref" instead of "me->data_r" because
                 this is first called during initialization,
                 upon cda_add_chan(), when me->data_r isn't filled yet
                 and is still ==-1 */
            dtype  = cda_current_dtype_of_ref (ref);
            nelems = cda_current_nelems_of_ref(ref);
            cda_acc_ref_data                  (ref, &buf,     NULL);
            cda_get_ref_stat                  (ref, &rflags, &timestamp);
            ReturnDataSet(devid, 1,
                          &chan_zero, &dtype, &nelems,
                          &buf, &rflags, &timestamp);
            me->was_trigger = 0;
        }
    }
    else if (reason == CDA_REF_R_RDSCHG)
    {
        if (cda_phys_rds_of_ref(ref, &phys_count, &phys_rds) == 0  &&
            phys_count >= 1)
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
            DoDriverLog(devid, 0, "data channel \"%s\" not found", src_p);
        }
    }
}

static int  trig_read_init_d(int devid, void *devptr,
                             int businfocount, int businfo[],
                             const char *auxinfo)
{
  privrec_t  *me = devptr;
  const char *p;
  char        buf[1024];
  size_t      trgr_len;

  cxdtype_t   dtype;
  cxdtype_t   is_unsigned_mask;
  char        ut_char;
  int         n_items;
  char       *endptr;

  cxsd_cpntid_t   gcid;
  cxsd_hw_chan_t *chn_p;

    me->cid = -1;
    me->trgr_r = me->data_r = -1;

    /* Parse auxinfo */
    if (auxinfo == NULL)
    {
        DoDriverLog(devid, 0, "%s(): empty auxinfo", __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }

    p = strchr(auxinfo, '/');
    if (p == auxinfo)
    {
        DoDriverLog(devid, 0, "missing trigger-chan spec");
        return -CXRF_CFG_PROBL;
    }
    trgr_len = p - auxinfo;
    if (p == NULL)
    {
        DoDriverLog(devid, 0, "'/' expected after trigger-chan spec");
        return -CXRF_CFG_PROBL;
    }
    while (*p == '/')
    {
        me->n_to_skip++;
        p++;
    }
    if (*p == '\0')
    {
        DoDriverLog(devid, 0, "data-chan spec expected after '/'");
        return -CXRF_CFG_PROBL;
    }

    if (trgr_len > sizeof(buf) - 1)
        trgr_len = sizeof(buf) - 1;
    memcpy(buf, auxinfo, trgr_len); buf[trgr_len] = '\0';

    if (*p == '@')
    {
        p++;

        is_unsigned_mask = 0;
        dtype   = CXDTYPE_UNKNOWN;
        n_items = 1;

        /* An "unsigned" modifier */
        if (*p == '+')
        {
            is_unsigned_mask = CXDTYPE_USGN_MASK;
            p++;
        }

        /* An OPTIONAL data-type designator */
        ut_char = tolower(*p);
        if (ut_char == '\0')
        {
            DoDriverLog(devid, 0, "data-chan spec expected after '@'");
            return -CXRF_CFG_PROBL;
        }
        else if (isdigit(ut_char))
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
    else
    {
        /* No @tNNN specified -- try to find out via DB */
        if (CxsdHwResolveChan(p, &gcid,
                              NULL, NULL, 0,
                              NULL, NULL, NULL, NULL, 
                              NULL, NULL, NULL, NULL) < 0)
        {
            /* Doesn't resolve?  Either an error or a foreign one */
            dtype   = CXDTYPE_UNKNOWN;
            n_items = sizeof(CxAnyVal_t);
        }
        else
        {
            chn_p = cxsd_hw_channels + gcid;
            dtype   = chn_p->dtype;
            n_items = chn_p->max_nelems;
        }
    }

    if ((me->cid = cda_new_context(devid, devptr,
                                   "insrv::", 0,
                                   NULL,
                                   0, NULL, 0)) < 0)
    {
        DoDriverLog(devid, 0, "cda_new_context(): %s", cda_last_err());
        return -CXRF_DRV_PROBL;
    }
    if ((me->trgr_r = cda_add_chan(me->cid, NULL, buf, 0,
                                   CXDTYPE_INT32, 1,
                                   CDA_REF_EVMASK_UPDATE |
                                     CDA_REF_EVMASK_RSLVSTAT,
                                   trgr_evproc, NULL)) < 0)
    {
        DoDriverLog(devid, 0, "cda_new_chan(trig=\"%s\"): %s",
                    buf, cda_last_err());
        cda_del_context(me->cid);    me->cid = -1;
        return -CXRF_DRV_PROBL;
    }
    if ((me->data_r = cda_add_chan(me->cid, NULL, p,
                                   CDA_DATAREF_OPT_NO_RD_CONV,
                                   dtype, n_items,
                                   CDA_REF_EVMASK_UPDATE     | 
                                     CDA_REF_EVMASK_RDSCHG   |
                                     CDA_REF_EVMASK_FRESHCHG |
                                     CDA_REF_EVMASK_RSLVSTAT,
                                   data_evproc, NULL)) < 0)
    {
        DoDriverLog(devid, 0, "cda_new_chan(data=\"%s\"): %s",
                    p, cda_last_err());
        cda_del_chan   (me->trgr_r); me->trgr_r = -1;
        cda_del_context(me->cid);    me->cid    = -1;
        return -CXRF_DRV_PROBL;
    }
    
    return DEVSTATE_OPERATING;
}

static void trig_read_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;

    cda_del_context(me->cid);
    me->cid = -1;
    me->trgr_r = me->data_r = -1;
}

static void trig_read_rw_p(int devid, void *devptr,
                           int action,
                           int count, int *addrs,
                           cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = devptr;
}


DEFINE_CXSD_DRIVER(trig_read, "Triggered-read driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   trig_read_init_d, trig_read_term_d, trig_read_rw_p);
