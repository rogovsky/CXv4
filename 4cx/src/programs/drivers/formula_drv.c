#include <math.h>

#include "cxsd_driver.h"
#include "cda.h"


typedef struct
{
    int            devid;
    cda_context_t  cid;
    cda_dataref_t  rd_ref;
    cda_dataref_t  wr_ref;
    const char    *rd_src;
    const char    *wr_src;
} privrec_t;

static psp_paramdescr_t formula_params[] =
{
    PSP_P_MSTRING("r", privrec_t, rd_src, NULL, 1000),
    PSP_P_MSTRING("w", privrec_t, wr_src, NULL, 1000),
    PSP_P_END()
};

static void formula_evproc(int            devid,
                           void          *devptr,
                           cda_dataref_t  ref,
                           int            reason,
                           void          *info_ptr,
                           void          *privptr2)
{
  privrec_t  *me = devptr;

  double      val;
  rflags_t    rflags;
  cx_time_t   timestamp;
  void       *val_p = &val;

fprintf(stderr, "%s ref=%d\n", __FUNCTION__, ref);
    cda_process_ref (me->rd_ref, CDA_OPT_RD_FLA | CDA_OPT_DO_EXEC,
                     NAN,
                     NULL, 0);
    cda_get_ref_dval(me->rd_ref,
                     &val,
                     NULL, NULL,
                     &rflags, &timestamp);
    ReturnDataSet(me->devid, 1,
                  (int      []){0},
                  (cxdtype_t[]){CXDTYPE_DOUBLE},
                  (int      []){1},
                  &val_p, &rflags, &timestamp);
}

static int formula_init_d(int devid, void *devptr,
                          int businfocount, int businfo[],
                          const char *auxinfo)
{
  privrec_t  *me = devptr;

    me->devid = devid;

    if ((me->cid = cda_new_context(devid, devptr,
                                   "insrv::", 0,
                                   NULL,
                                   0, NULL, 0)) < 0)
    {
        DoDriverLog(devid, 0, "cda_new_context(): %s", cda_last_err());
        return -CXRF_DRV_PROBL;
    }

    if (me->rd_src != NULL)
    {
        me->rd_ref = cda_add_formula(me->cid, "",
                                     me->rd_src, CDA_OPT_RD_FLA,
                                     NULL, 0,
                                     CDA_REF_EVMASK_UPDATE, formula_evproc, NULL);
        if (me->rd_ref < 0)
        {
            DoDriverLog(devid, 0, "cda_add_formula(rd): %s", cda_last_err());
            return -CXRF_DRV_PROBL;
        }
    }
    else
    {
    }

    if (me->wr_src != NULL)
    {
        me->wr_ref = cda_add_formula(me->cid, "",
                                     me->wr_src, CDA_OPT_WR_FLA,
                                     NULL, 0,
                                     0, NULL, NULL);
        if (me->wr_ref < 0)
        {
            DoDriverLog(devid, 0, "cda_add_formula(wr): %s", cda_last_err());
            return -CXRF_DRV_PROBL;
        }
    }

    SetChanReturnType(devid, 0, 1, IS_AUTOUPDATED_YES);

    return DEVSTATE_OPERATING;
}

static void formula_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;

    cda_del_context(me->cid);
    me->cid    = -1;
    me->rd_ref = me->wr_ref = -1;
}

static void formula_rw_p(int devid, void *devptr,
                         int action,
                         int count, int *addrs,
                         cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = devptr;

  static double      val    = 0;
  static void       *val_p  = &val;
  static rflags_t    rflags = 0;

    ////fprintf(stderr, "%s (%d %d) %d %f\n", __FUNCTION__, me->rd_ref, me->wr_ref, action, action==DRVA_WRITE?*((double*)(values[0])):NAN);
    if (action == DRVA_WRITE  &&  count > 0  &&  dtypes[0] == CXDTYPE_DOUBLE)
    {
        cda_set_dcval(me->wr_ref, *((double*)(values[0])));
        if (!cda_ref_is_sensible(me->rd_ref))
            ReturnDataSet(me->devid, 1,
                          (int      []){0},
                          (cxdtype_t[]){CXDTYPE_DOUBLE},
                          (int      []){1},
                          &val_p, &rflags, NULL);
    }
}


DEFINE_CXSD_DRIVER(formula, "Formula r/w driver",
                   NULL, NULL,
                   sizeof(privrec_t), formula_params,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   formula_init_d, formula_term_d, formula_rw_p);
