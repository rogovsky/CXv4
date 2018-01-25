#include <errno.h>
#include <sys/time.h>

#include "misc_macros.h"

#include "cda.h"
#include "cdaP.h"

#include "cda_d_dircn.h"
#include "cda_d_dircn_api.h"


//#### Internals #####################################################

struct _varinfo_t_struct;

typedef void (*varchg_cb_t)(struct _varinfo_t_struct *vi,
                            cda_srvconn_t  client_sid,
                            cda_hwcnref_t  client_hwr);

typedef int dircn_lcn_t;

typedef struct
{
    varchg_cb_t    proc;
    cda_srvconn_t  client_sid;
    cda_hwcnref_t  client_hwr;
} var_cbrec_t;
typedef struct _varinfo_t_struct
{
    int                  in_use;
    const char          *name;
    cxdtype_t            dtype;
    int                  n_items;
    void                *addr;
    cda_d_dircn_write_f  do_write;
    void                *do_write_privptr;

    //
    int                  current_nelems;
    rflags_t             rflags;
    cx_time_t            timestamp;

    var_cbrec_t         *cb_list;
    int                  cb_list_allocd;
} varinfo_t;

enum
{
    VAR_MIN_VAL   = 1,
    VAR_MAX_COUNT = 10000,    // An arbitrary limit
    VAR_ALLOC_INC = 100,
};

static varinfo_t *vars_list        = NULL;
static int        vars_list_allocd = 0;

static int var_name_checker(varinfo_t *p, void *privptr)
{
    return strcasecmp(p->name, privptr) == 0;
}

//--------------------------------------------------------------------

// GetVarCbSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, VarCb, var_cbrec_t,
                                 cb, proc, NULL, (void*)1,
                                 0, 2, 0,
                                 vi->, vi,
                                 varinfo_t *vi, varinfo_t *vi)

static void RlsVarCbSlot(int id, varinfo_t *vi)
{
  var_cbrec_t *p = AccessVarCbSlot(id, vi);

    p->proc = NULL;
}

//--------------------------------------------------------------------

// GetVarSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Var, varinfo_t,
                                 vars, in_use, 0, 1,
                                 VAR_MIN_VAL, VAR_ALLOC_INC, VAR_MAX_COUNT,
                                 , , void)

static void RlsVarSlot(cda_d_dircn_t var)
{
  varinfo_t *vi = AccessVarSlot(var);

    if (vi->in_use == 0) return; /*!!! In fact, an internal error */
    vi->in_use = 0;

    DestroyVarCbSlotArray(vi);

    safe_free(vi->name);
}

static int CheckVar(cda_d_dircn_t var)
{
    if (var >= VAR_MIN_VAL  &&
        var < (cda_d_dircn_t)vars_list_allocd  &&  vars_list[var].in_use != 0)
        return 0;

    errno = EBADF;
    return -1;
}

//--------------------------------------------------------------------

enum
{
    LCN_MIN_VAL   = 1,
    LCN_MAX_COUNT = 10000,    // An arbitrary limit
    LCN_ALLOC_INC = 2,        // Note: for inserver should be 10 or 100
};

typedef struct
{
    void *pdt_privptr;
} lcninfo_t;

static lcninfo_t *lcns_list;
static int        lcns_list_allocd;

// GetLcnSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Lcn, lcninfo_t,
                                 lcns, pdt_privptr, NULL, (void*)1,
                                 LCN_MIN_VAL, LCN_ALLOC_INC, LCN_MAX_COUNT,
                                 , , void)

static void RlsLcnSlot(int n)
{
  lcninfo_t *p = AccessLcnSlot(n);

    p->pdt_privptr = NULL;
}

//--------------------------------------------------------------------

//#### Public API ####################################################

cda_d_dircn_t  cda_d_dircn_register_chan(const char          *name,
                                         cxdtype_t            dtype,
                                         int                  n_items,
                                         void                *addr,
                                         cda_d_dircn_write_f  do_write,
                                         void                *do_write_privptr)
{
  cda_d_dircn_t  var;
  varinfo_t     *vi;

    /* 0. Checks */

    /* a. Check name sanity */
    if (name == NULL  ||  *name == '\0')
    {
        errno = ENOENT;
        return CDA_D_DIRCN_ERROR;
    }

    /* b. Check properties */
    if (addr == NULL)
    {
        errno = EFAULT;
        return CDA_D_DIRCN_ERROR;
    }
    /*!!! Other?*/

    /* c. Check duplicates */
    var = ForeachVarSlot(var_name_checker, name);
    if (var >= 0)
    {
        errno = EEXIST;
        return CDA_D_DIRCN_ERROR;
    }

    /* 1. Allocate */
    var = GetVarSlot();
    if (var < 0) return var;
    vi = AccessVarSlot(var);
    //
    if ((vi->name = strdup(name)) == NULL)
    {
        RlsVarSlot(var);
        return CDA_D_DIRCN_ERROR;
    }
    vi->dtype            = dtype;
    vi->n_items          = n_items;
    vi->addr             = addr;
    vi->do_write         = do_write;
    vi->do_write_privptr = do_write_privptr;

    vi->current_nelems   = (n_items == 1)? n_items : 0;

    return var;
}


static int CallVarCBs(var_cbrec_t *p, void *privptr)
{
    p->proc(privptr, p->client_sid, p->client_hwr);
    return 0;
}
int            cda_d_dircn_update_chan  (cda_d_dircn_t  var,
                                         int            nelems,
                                         rflags_t       rflags)
{
  varinfo_t      *vi = AccessVarSlot(var);

  struct timeval  timenow;

    if (CheckVar(var) != 0) return -1;

    if (nelems < 0)
        vi->current_nelems = vi->n_items;
    else
        /*!!! Should check nelems, as in ReturnDataSet() */
        vi->current_nelems = nelems;

    vi->rflags         = rflags;
    gettimeofday(&timenow, NULL);
    vi->timestamp.sec  = timenow.tv_sec;
    vi->timestamp.nsec = timenow.tv_usec * 1000;

    ForeachVarCbSlot(CallVarCBs, vi, vi);

    return 0;
}

static void PerformCycleUpdate(void *pdt_privptr);

static int update_cycle_caller(lcninfo_t *p, void *privptr)
{
    PerformCycleUpdate(p->pdt_privptr);
    return 0;
}
void           cda_d_dircn_update_cycle (void)
{
    // Foreach client: call cda_dat_p_update_server_cycle()
    ForeachLcnSlot(update_cycle_caller, NULL);
}

//---- Hack/debug-API ------------------------------------------------

int            cda_d_dircn_chans_count  (void)
{
  int  ret;

    for (ret = vars_list_allocd - 1;  ret > 0;  ret--)
        if (CheckVar(ret) == 0) return ret + 1;

    return 0;
}

int            cda_d_dircn_chan_n_props (cda_d_dircn_t  var,
                                         const char   **name_p,
                                         cxdtype_t     *dtype_p,
                                         int           *n_items_p,
                                         void         **addr_p)
{
  varinfo_t      *vi = AccessVarSlot(var);

    if (CheckVar(var) != 0) return -1;

    if (name_p    != NULL) *name_p    = vi->name;
    if (dtype_p   != NULL) *dtype_p   = vi->dtype;
    if (n_items_p != NULL) *n_items_p = vi->n_items;
    if (addr_p    != NULL) *addr_p    = vi->addr;

    return 0;
}

int            cda_d_dircn_override_wr  (cda_d_dircn_t  var,
                                         cda_d_dircn_write_f  do_write,
                                         void                *do_write_privptr)
{
  varinfo_t      *vi = AccessVarSlot(var);

    if (CheckVar(var) != 0) return -1;

    vi->do_write         = do_write;
    vi->do_write_privptr = do_write_privptr;

    return 0;
}

//#### Internal API ##################################################

static int  RegisterVarCB  (cda_d_dircn_t  var,
                            varchg_cb_t    proc,
                            cda_srvconn_t  client_sid,
                            cda_hwcnref_t  client_hwr)
{
  varinfo_t      *vi = AccessVarSlot(var);
  int             n;
  var_cbrec_t    *p;

    n = GetVarCbSlot(vi);
    if (n < 0) return -1;
    p = AccessVarCbSlot(n, vi);
    p->proc       = proc;
    p->client_sid = client_sid;
    p->client_hwr = client_hwr;

    return 0;
}

typedef struct
{
    varinfo_t     *vi;
    varchg_cb_t    proc;
    cda_srvconn_t  client_sid;
    cda_hwcnref_t  client_hwr;
} DelVarCb_info_t;
static int DelVarCb(var_cbrec_t *p, void *privptr)
{
  DelVarCb_info_t *info_p = privptr;

    if (p->proc       == info_p->proc        &&
        p->client_sid == info_p->client_sid  &&
        p->client_hwr == info_p->client_hwr)
        RlsVarCbSlot(p - info_p->vi->cb_list, info_p->vi); /* "p2id" for VarCbSlot */

    return 0;
}
static void UnRegisterVarCB(cda_d_dircn_t  var,
                            varchg_cb_t    proc,
                            cda_srvconn_t  client_sid,
                            cda_hwcnref_t  client_hwr)
{
  varinfo_t      *vi = AccessVarSlot(var);
  DelVarCb_info_t info;

    info.vi         = vi;
    info.proc       = proc;
    info.client_sid = client_sid;
    info.client_hwr = client_hwr;

    ForeachVarCbSlot(DelVarCb, &info, vi);
}

static int WriteToVar(cda_d_dircn_t  var,
                      cxdtype_t dtype, int nelems, void *value)
{
  varinfo_t      *vi = AccessVarSlot(var);

    /*!!! Check!!! */

    if (dtype != vi->dtype  ||
        (vi->n_items == 1  &&  nelems != 1))
    {
        errno = EINVAL;
        return -1;
    }

    if (nelems > vi->n_items)
        nelems = vi->n_items;

    if (vi->do_write != NULL)
        return vi->do_write(var, vi->do_write_privptr,
                            value, dtype, nelems);
    else
        return 0;
}

//#### Data-access plugin ############################################

typedef struct
{
    int            in_use;

    cda_dataref_t  dataref; // "Backreference" to corr.entry in the global table

    cda_d_dircn_t  var;
} hwrinfo_t;

typedef struct
{
    cda_srvconn_t    sid;
    dircn_lcn_t      lcn;
    int              being_processed;
    int              being_destroyed;

    hwrinfo_t       *hwrs_list;
    int              hwrs_list_allocd;
} cda_d_dircn_privrec_t;

enum
{
    HWR_MIN_VAL   = 1,
    HWR_MAX_COUNT = 1000000,  // An arbitrary limit
    HWR_ALLOC_INC = 100,
};


//--------------------------------------------------------------------

static void VarChgCB(varinfo_t     *vi,
                     cda_srvconn_t  client_sid,
                     cda_hwcnref_t  client_hwr);

// GetHwrSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Hwr, hwrinfo_t,
                                 hwrs, in_use, 0, 1,
                                 HWR_MIN_VAL, HWR_ALLOC_INC, HWR_MAX_COUNT,
                                 me->, me,
                                 cda_d_dircn_privrec_t *me, cda_d_dircn_privrec_t *me)

static void RlsHwrSlot(cda_hwcnref_t hwr, cda_d_dircn_privrec_t *me)
{
  hwrinfo_t *hi = AccessHwrSlot(hwr, me);

    if (hi->in_use == 0) return; /*!!! In fact, an internal error */
    hi->in_use = 0;

    UnRegisterVarCB(hi->var, VarChgCB, me->sid, hwr);
}


//--------------------------------------------------------------------

static int DestroyDircn_HwrIterator(hwrinfo_t *hi, void *privptr)
{
  cda_d_dircn_privrec_t *me  = privptr;
  cda_hwcnref_t          hwr = hi - me->hwrs_list; /* "hi2hwr()" */

    RlsHwrSlot(hwr, me);

    return 0;
}
static void DestroyDircnPrivrec(cda_d_dircn_privrec_t *me)
{
    ForeachHwrSlot(DestroyDircn_HwrIterator, me, me); DestroyHwrSlotArray(me);
    /* Note: should do "me->lcn=-1" only AFTER destroying hwr-array, since it is used for identification in HwrCb's */
    /* Note 2: in fact, in dircn the lcn has almost no use at all */
    if (me->lcn      >= LCN_MIN_VAL) RlsLcnSlot(me->lcn);      me->lcn      = -1;
}

//--------------------------------------------------------------------

static void VarChgCB(varinfo_t     *vi,
                     cda_srvconn_t  client_sid,
                     cda_hwcnref_t  client_hwr)
{
  cda_d_dircn_privrec_t *me = cda_dat_p_privp_of_sid(client_sid);
  hwrinfo_t             *hi = AccessHwrSlot(client_hwr, me);

//fprintf(stderr, "%s sid=%d hwr=%d\n", __FUNCTION__, client_sid, client_hwr);
    me->being_processed++;

    cda_dat_p_update_dataset(me->sid,
                             1, &(hi->dataref),
                             &(vi->addr), &(vi->dtype), &(vi->current_nelems),
                             &(vi->rflags), &(vi->timestamp),
                             1);

    me->being_processed--;
    if (me->being_processed == 0  &&  me->being_destroyed)
    {
        DestroyDircnPrivrec(me);
        cda_dat_p_del_server_finish(me->sid);
        return;
    }
}


static int  cda_d_dircn_new_chan(cda_dataref_t ref, const char *name,
                                 int options,
                                 cxdtype_t dtype, int nelems)
{
  cda_d_dircn_privrec_t *me;

  cda_d_dircn_t          var;
  varinfo_t             *vi;

  cda_hwcnref_t          hwr;
  hwrinfo_t             *hi;

  const char            *envval;
  void                  *vp;

    var = ForeachVarSlot(var_name_checker, name);
    if (var < 0)
    {
        if ((envval = getenv("CDA_D_DIRCN_AUTO_CREATE")) == NULL  ||
            (*envval != '1'  &&  tolower(*envval) != 'y')         ||
            (var = cda_d_dircn_register_chan(name, CXDTYPE_DOUBLE, 1,
                                             vp = malloc(sizeof(double)),
                                             NULL, NULL)) == CDA_D_DIRCN_ERROR)
        {
            cda_set_err("unknown channel");
            return CDA_DAT_P_ERROR;
        }
        *((double*)vp) = 0.0;
    }
    vi = AccessVarSlot(var);

    me = cda_dat_p_get_server(ref, &CDA_DAT_P_MODREC_NAME(dircn), NULL, CDA_DAT_P_GET_SERVER_OPT_NONE);
    if (me == NULL) return CDA_DAT_P_ERROR;

    hwr = GetHwrSlot(me);
    if (hwr < 0) return CDA_DAT_P_ERROR;
    hi = AccessHwrSlot(hwr, me);
    // Fill data
    hi->dataref = ref;
    hi->var     = var;

    // Subscribe to updates
    RegisterVarCB(var, VarChgCB, me->sid, hwr);

    // Return current data
    cda_dat_p_update_dataset(me->sid,
                             1, &(hi->dataref),
                             &(vi->addr), &(vi->dtype), &(vi->current_nelems),
                             &(vi->rflags), &(vi->timestamp),
                             1);

    cda_dat_p_set_hwr  (ref, hwr);
    cda_dat_p_set_ready(ref, 1);

    return CDA_DAT_P_OPERATING;
}

static void cda_d_dircn_del_chan(void *pdt_privptr, cda_hwcnref_t hwr)
{
  cda_d_dircn_privrec_t *me = pdt_privptr;
  hwrinfo_t             *hi = AccessHwrSlot(hwr, me);

    if (hwr < 0  ||  hwr >= me->hwrs_list_allocd  ||
        hi->in_use == 0)
    {
        /*!!!Bark? */
        return;
    }

    RlsHwrSlot(hwr, me);
}

static int  cda_d_dircn_snd_data(void *pdt_privptr, cda_hwcnref_t hwr,
                                 cxdtype_t dtype, int nelems, void *value)
{
  cda_d_dircn_privrec_t *me = pdt_privptr;
  hwrinfo_t             *hi = AccessHwrSlot(hwr, me);

    if (hwr < 0  ||  hwr >= me->hwrs_list_allocd  ||
        hi->in_use == 0)
    {
        /*!!!Bark? */
        return CDA_PROCESS_SEVERE_ERR;
    }

    return WriteToVar(hi->var, dtype, nelems, value) == 0? CDA_PROCESS_DONE
                                                         : CDA_PROCESS_SEVERE_ERR;
}

static void PerformCycleUpdate(void *pdt_privptr)
{
  cda_d_dircn_privrec_t *me = pdt_privptr;

    cda_dat_p_update_server_cycle(me->sid);
}

static int  cda_d_dircn_new_srv (cda_srvconn_t  sid, void *pdt_privptr,
                                 int            uniq,
                                 const char    *srvrspec,
                                 const char    *argv0)
{
  cda_d_dircn_privrec_t *me = pdt_privptr;
  lcninfo_t             *p;

    me->sid = sid;
    me->lcn = GetLcnSlot();
    if (me->lcn < 0) return CDA_DAT_P_ERROR;
    p = AccessLcnSlot(me->lcn);
    p->pdt_privptr = pdt_privptr;

    return CDA_DAT_P_OPERATING;
}

static int  cda_d_dircn_del_srv (cda_srvconn_t  sid, void *pdt_privptr)
{
  cda_d_dircn_privrec_t *me = pdt_privptr;
  
    if (me->being_processed)
    {
        me->being_destroyed = 1;
        return CDA_DAT_P_DEL_SRV_DEFERRED;
    }
    else
    {
        DestroyDircnPrivrec(me);
        return CDA_DAT_P_DEL_SRV_SUCCESS;
    }
}

//////////////////////////////////////////////////////////////////////

CDA_DEFINE_DAT_PLUGIN(dircn, "Dircn data-access plugin",
                      NULL, NULL,
                      sizeof(cda_d_dircn_privrec_t),
                      '.', ':', '\0',
                      cda_d_dircn_new_chan, cda_d_dircn_del_chan,
                      cda_d_dircn_snd_data, NULL,
                      cda_d_dircn_new_srv,  cda_d_dircn_del_srv);
