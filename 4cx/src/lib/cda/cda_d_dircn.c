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
                            int            client_id,
                            int            channel_n);

typedef struct
{
    varchg_cb_t  proc;
    int          client_id;
    int          channel_n;
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

typedef struct
{
    void *pdt_privptr;
} lclconn_t;

static lclconn_t *lclconns_list;
static int        lclconns_list_allocd;

// GetDircnSrvSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, DircnSrv, lclconn_t,
                                 lclconns, pdt_privptr, NULL, (void*)1,
                                 0, 2, 0,
                                 , , void)

static void RlsDircnSrvSlot(int n)
{
  lclconn_t *p = AccessDircnSrvSlot(n);

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
    p->proc(privptr, p->client_id, p->channel_n);
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

static int update_cycle_caller(lclconn_t *p, void *privptr)
{
    PerformCycleUpdate(p->pdt_privptr);
    return 0;
}
void           cda_d_dircn_update_cycle (void)
{
    // Foreach client: call cda_dat_p_update_server_cycle()
    ForeachDircnSrvSlot(update_cycle_caller, NULL);
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

static int RegisterVarCB(cda_d_dircn_t  var,
                         varchg_cb_t    proc,
                         int            client_id,
                         int            channel_n)
{
  varinfo_t      *vi = AccessVarSlot(var);
  int             n;
  var_cbrec_t    *p;

    n = GetVarCbSlot(vi);
    if (n < 0) return -1;
    p = AccessVarCbSlot(n, vi);
    p->proc      = proc;
    p->client_id = client_id;
    p->channel_n = channel_n;

    return 0;
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
    int              dircn_srv_id;

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


}


//--------------------------------------------------------------------

static void VarChgCB(varinfo_t *vi,
                     int        client_id,
                     int        channel_n)
{
  cda_d_dircn_privrec_t *me = cda_dat_p_privp_of_sid(client_id);
  hwrinfo_t             *hi = AccessHwrSlot(channel_n, me);

fprintf(stderr, "%s id=%d n=%d\n", __FUNCTION__, client_id, channel_n);
    cda_dat_p_update_dataset(me->sid,
                             1, &(hi->dataref),
                             &(vi->addr), &(vi->dtype), &(vi->current_nelems),
                             &(vi->rflags), &(vi->timestamp),
                             1);
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

    me = cda_dat_p_get_server(ref, &CDA_DAT_P_MODREC_NAME(dircn), NULL);
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
  lclconn_t             *p;

    me->sid = sid;
    me->dircn_srv_id = GetDircnSrvSlot();
    if (me->dircn_srv_id < 0) return CDA_DAT_P_ERROR;
    p = AccessDircnSrvSlot(me->dircn_srv_id);
    p->pdt_privptr = pdt_privptr;

    return CDA_DAT_P_OPERATING;
}

static void cda_d_dircn_del_srv (cda_srvconn_t  sid, void *pdt_privptr)
{
  cda_d_dircn_privrec_t *me = pdt_privptr;
  
}

//////////////////////////////////////////////////////////////////////

CDA_DEFINE_DAT_PLUGIN(dircn, "Dircn data-access plugin",
                      NULL, NULL,
                      sizeof(cda_d_dircn_privrec_t),
                      '.', ':', '\0',
                      cda_d_dircn_new_chan, NULL,
                      cda_d_dircn_snd_data, NULL,
                      cda_d_dircn_new_srv,  cda_d_dircn_del_srv);
