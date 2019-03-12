#include "cxsd_driver.h"
#include "cda.h"
#include "memcasecmp.h"


enum
{
    CHAN_SELECTOR      = 0,
    RESERVED_CHANCOUNT = 10,
};

enum
{
    MAX_OUTPUTS = 100,
    MAX_PRESETS = 20,  // An arbitrary limit
};

typedef struct
{
    cda_context_t  cid;
    int            num_presets;
    int            num_outputs;
    cda_dataref_t  out_r[MAX_OUTPUTS];

    CxAnyVal_t    *presets;
    cxdtype_t     *dtypes;

    void          *data_buf;
} privrec_t;


static void ReturnOutRDs(int devid, privrec_t *me, int nl)
{
  int         phys_count;
  double     *phys_rds;
  int         np;

    if (cda_phys_rds_of_ref(me->out_r[nl], &phys_count, &phys_rds) == 0  &&
        phys_count >= 1)
        for (np = 0;  np < me->num_presets;  np++)
            SetChanRDs(devid, 
                       RESERVED_CHANCOUNT + np * me->num_outputs + nl, 1,
                       phys_rds[0], phys_rds[1]);
}

static void out_evproc(int            devid,
                       void          *devptr,
                       cda_dataref_t  ref,
                       int            reason,
                       void          *info_ptr,
                       void          *privptr2)
{
  privrec_t  *me = devptr;
  int         nl = ptr2lint(privptr2);

    if (reason == CDA_REF_R_RDSCHG)
    {
        /* data_buf==NULL means "parsing not completed, allocation not performed",
           so should NOT tunnel properties */
        if (me->data_buf != NULL) ReturnOutRDs(devid, me, nl);
    }
}

static int preset_selector_init_d(int devid, void *devptr,
                                  int businfocount, int businfo[],
                                  const char *auxinfo)
{
  privrec_t  *me = devptr;

  const char *p;
  const char *endp;

  char        defpfx_buf[200];
  char        base_buf  [200];
  char        spec_buf  [1000];

  size_t      bufsize;

  typedef enum
  {
      KWD_NONE   = 0,
      KWD_DEFPFX = 1,
      KWD_BASE   = 2,
  } kwd_t;

  kwd_t       kwd;
  size_t      len;

  cxdtype_t   dtype;
  int         n_items;

  int         np;
  int         nl;

    /* 0. Is there anything at all? */
    if (auxinfo == NULL  ||  auxinfo[0] == '\0')
    {
        DoDriverLog(devid, DRIVERLOG_ERR, "%s: auxinfo is empty...", __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }

    /* 1. Number of presets */
    p = auxinfo;
    me->num_presets = strtol(p, &endp, 10);
    if (endp == p  ||
        (*endp != '\0'  &&  !isspace(*endp)))
    {
        DoDriverLog(devid, DRIVERLOG_ERR, "invalid NUMBER_OF_PRESETS spec");
        return -CXRF_CFG_PROBL;
    }
    p = endp;
    if (me->num_presets < 1  ||  me->num_presets > MAX_PRESETS)
    {
        DoDriverLog(devid, DRIVERLOG_ERR, "num_presets=%d, out of [%d,%d] range",
                    me->num_presets, 1, MAX_PRESETS);
        return -CXRF_CFG_PROBL;
    }

    /* 2. Parse channel specs */
    me->cid         = CDA_CONTEXT_ERROR;
    me->num_outputs = 0;
    defpfx_buf[0]   = '\0';
    base_buf  [0]   = '\0';
    for (; ;  p = endp)
    {
        while (*p != '\0'  &&  isspace(*p)) p++;
        if (*p == '\0') goto END_PARSE_LIST;

        for (endp = p, kwd = KWD_NONE;
             *endp != '\0'  &&  !isspace(*endp);
             endp++)
        {
            if (*endp == '='  &&  kwd == KWD_NONE)
            {
                len = endp - p;
                if (len == 0)
                {
                    DoDriverLog(devid, DRIVERLOG_ERR, "bare '=' without a preceding parameter name");
                    return -CXRF_CFG_PROBL;
                }
                else if (cx_strmemcasecmp("defpfx", p, len) == 0)
                {
                    kwd = KWD_DEFPFX;
                    if (me->num_outputs > 0)
                    {
                        DoDriverLog(devid, DRIVERLOG_ERR, "defpfx spec after channels");
                        return -CXRF_CFG_PROBL;
                    }
                }
                else if (cx_strmemcasecmp("base",   p, len) == 0)
                {
                    kwd = KWD_BASE;
                }
                else
                {
                    DoDriverLog(devid, DRIVERLOG_ERR, "unknown \"%.*s=\" parameter",
                                (int)len, p);
                    return -CXRF_CFG_PROBL;
                }
                p = endp + 1;
            }
            else
            {
                /* Do-nothing, right? */
            }
        }
        len = endp - p;

        if (kwd == KWD_NONE)
        {
            if (len != 0) /* How can it be??? */
            {
                /* Register context upon 1st channel */
                if (me->num_outputs == 0)
                {
                    if (defpfx_buf[0] == '\0') strcpy(defpfx_buf, "insrv::");
                    if ((me->cid = cda_new_context(devid, devptr,
                                                   defpfx_buf, 0,
                                                   NULL,
                                                   0, NULL, 0)) < 0)
                    {
                        DoDriverLog(devid, 0, "cda_new_context(): %s", cda_last_err());
                        return -CXRF_DRV_PROBL;
                    }
                }

                /* Note: for now, NO @t[NNN] parsing */
                dtype   = CXDTYPE_INT32;
                n_items = 1;
                if (len > sizeof(spec_buf) - 1)
                {
                    len = sizeof(spec_buf) - 1;
                }
                memcpy(spec_buf, p, len); spec_buf[len] = '\0';
                me->out_r[me->num_outputs] =
                    cda_add_chan(me->cid, base_buf, spec_buf,
                                 CDA_DATAREF_OPT_NO_RD_CONV | CDA_DATAREF_OPT_SHY,
                                 dtype, n_items,
                                 CDA_REF_EVMASK_RDSCHG,
                                 out_evproc, lint2ptr(me->num_outputs));
                if (me->out_r[me->num_outputs] < 0)
                {
                    DoDriverLog(devid, DRIVERLOG_WARNING,
                                "cda_add_chan(%d:\"%s\") fail: %s",
                                me->num_outputs, spec_buf, cda_last_err());
////fprintf(stderr, "[%d]<%s>\n", me->num_outputs, spec_buf);
                    return -CXRF_CFG_PROBL;
                }
                me->num_outputs++;
            }
        }
        else if (kwd == KWD_DEFPFX)
        {
            if (len > sizeof(defpfx_buf) - 1)
            {
                DoDriverLog(devid, DRIVERLOG_WARNING,
                            "WARNING: \"defpfx=\" spec is %zd bytes long, truncating to %zd",
                            len, sizeof(defpfx_buf) - 1);
                len = sizeof(defpfx_buf) - 1;
            }
            memcpy(defpfx_buf, p, len); defpfx_buf[len] = '\0';
        }
        else if (kwd == KWD_BASE)
        {
            if (len > sizeof(base_buf) - 1)
            {
                DoDriverLog(devid, DRIVERLOG_WARNING,
                            "WARNING: \"base=\" spec is %zd bytes long, truncating to %zd",
                            len, sizeof(base_buf) - 1);
                len = sizeof(base_buf) - 1;
            }
            memcpy(base_buf, p, len); base_buf[len] = '\0';
        }
 NEXT_ITEM:;
    }
 END_PARSE_LIST:;

    /* Note: dtypes are AFTER presets, thus 1-byte values after 16-byte ones,
             thus no alignment problems */
    bufsize = (sizeof(CxAnyVal_t) + sizeof(cxdtype_t)) * me->num_presets * me->num_outputs;
    if ((me->data_buf = malloc(bufsize)) == NULL)
    {
        return -CXRF_DRV_PROBL;
    }
    bzero(me->data_buf, bufsize);
    me->presets = me->data_buf;
    me->dtypes  = me->presets + (me->num_presets * me->num_outputs); // Parens are just for visibility, not because of precedence :-)

    for (nl = 0;  nl < me->num_outputs;  nl++)
    {
        for (np = 0;  np < me->num_presets;  np++)
            me->dtypes[np * me->num_outputs + nl] = CXDTYPE_INT32;
        ReturnOutRDs(devid, me, nl);
    }
////fprintf(stderr, "num_presets=%d num_outputs=%d\n", me->num_presets, me->num_outputs);

    return DEVSTATE_OPERATING;
}

static void preset_selector_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;

    safe_free(me->data_buf);  me->data_buf = NULL;
}

static void preset_selector_rw_p(int devid, void *devptr,
                                 int action,
                                 int count, int *addrs,
                                 cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = devptr;
  int        n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int        chn;   // channel
  int32      val;
  int        x;
  int        cell;
  void      *vp;

  static int       n_1         = 1;
  static rflags_t  zero_rflags = 0;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];

        if      (chn == CHAN_SELECTOR)
        {
            if (action == DRVA_WRITE)
            {
                if (nelems[n] == 1  &&
                    (dtypes[n] == CXDTYPE_INT32  ||  dtypes[n] == CXDTYPE_UINT32))
                {
                    val = *((int32*)(values[n]));
fprintf(stderr, "activate #%d\n", val);
                    if (val >= 0  &&  val < me->num_presets)
                    {
                        for (x = 0, cell = val * me->num_outputs;
                             x < me->num_outputs;
                             x++,   cell++)
                            cda_snd_ref_data(me->out_r[x],
                                             me->dtypes[cell], 1,
                                             me->presets + cell);
                    }
                }
            }
            ReturnInt32Datum(devid, chn, 0, 0);
        }
        else if (chn >= RESERVED_CHANCOUNT  &&
                 chn <  RESERVED_CHANCOUNT + me->num_presets * me->num_outputs)
        {
            cell = chn - RESERVED_CHANCOUNT;
            if (action == DRVA_WRITE  &&
                nelems[n] == 1        &&
                sizeof_cxdtype(dtypes[n]) < sizeof(CxAnyVal_t))
            {
                memcpy(me->presets + cell, values[n], sizeof_cxdtype(dtypes[n]));
            }
            vp = me->presets + cell;
            ReturnDataSet(devid, 1,
                          &chn, me->dtypes + cell, &n_1,
                          &vp, &zero_rflags, NULL);
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(preset_selector, "Preset selector driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   preset_selector_init_d,
                   preset_selector_term_d,
                   preset_selector_rw_p);
