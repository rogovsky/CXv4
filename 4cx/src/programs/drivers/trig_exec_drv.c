#include <math.h>

#include "cxsd_driver.h"
#include "cda.h"


typedef struct
{
    int            devid;
    cda_context_t  cid;
    cda_dataref_t  tg_ref;
    cda_dataref_t  ex_ref;
    const char    *tg_src;
    const char    *ex_src;
} privrec_t;

static psp_paramdescr_t trig_exec_params[] =
{
    PSP_P_MSTRING("t", privrec_t, tg_src, NULL, 1000),
    PSP_P_MSTRING("e", privrec_t, ex_src, NULL, 1000),
    PSP_P_END()
};

static void trig_exec_evproc(int            devid,
                             void          *devptr,
                             cda_dataref_t  ref,
                             int            reason,
                             void          *info_ptr,
                             void          *privptr2)
{
  privrec_t  *me = devptr;
  double      val;

////fprintf(stderr, "%s ref=%d\n", __FUNCTION__, ref);
    cda_process_ref (me->tg_ref, CDA_OPT_RD_FLA | CDA_OPT_DO_EXEC,
                     NAN,
                     NULL, 0);
    cda_get_ref_dval(me->tg_ref,
                     &val,
                     NULL, NULL,
                     NULL, NULL);
    cda_set_dcval(me->ex_ref, val);
}

static int trig_exec_init_d(int devid, void *devptr,
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

    if (me->tg_src != NULL)
    {
        me->tg_ref = cda_add_formula(me->cid, "",
                                     me->tg_src, CDA_OPT_RD_FLA,
                                     NULL, 0,
                                     CDA_REF_EVMASK_UPDATE, trig_exec_evproc, NULL);
        if (me->tg_ref < 0)
        {
            DoDriverLog(devid, 0, "cda_add_formula(trigger): %s", cda_last_err());
            return -CXRF_DRV_PROBL;
        }
    }
    else
    {
    }

    if (me->ex_src != NULL)
    {
        me->ex_ref = cda_add_formula(me->cid, "",
                                     me->ex_src, CDA_OPT_WR_FLA,
                                     NULL, 0,
                                     0, NULL, NULL);
        if (me->ex_ref < 0)
        {
            DoDriverLog(devid, 0, "cda_add_formula(exec): %s", cda_last_err());
            return -CXRF_DRV_PROBL;
        }
    }

    return DEVSTATE_OPERATING;
}

static void trig_exec_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;

    cda_del_context(me->cid);
    me->cid    = -1;
    me->tg_ref = me->ex_ref = -1;
}


DEFINE_CXSD_DRIVER(trig_exec, "Triggered-execute driver",
                   NULL, NULL,
                   sizeof(privrec_t), trig_exec_params,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   trig_exec_init_d, trig_exec_term_d, NULL);
