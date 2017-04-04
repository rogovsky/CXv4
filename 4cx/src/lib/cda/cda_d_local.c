#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

#include "misc_macros.h"
#include "misclib.h"
#include "cxscheduler.h"
#include "fdiolib.h"

#include "cda.h"
#include "cdaP.h"

#include "cda_d_local.h"
#include "cda_d_local_api.h"


//#### Internals #####################################################
// Note: "local"<->"insrv"

enum {VARCB_VAL_UNUSED = -1, VARCB_VAL_USED = 0};
enum {LCN_VAL_UNUSED   = -1, LCN_VAL_USED   = 0};

enum
{
    HWR_VAL_CYCLE    = -1,
    HWR_VAL_STRSCHG  = -2,
    HWR_VAL_RDSCHG   = -3,
};

typedef int local_lcn_t;

typedef struct
{
    local_lcn_t    lcn;
    cda_hwcnref_t  client_hwr;
} var_cbrec_t;
typedef struct
{
    int                  in_use;
    const char          *name;
    cxdtype_t            dtype;
    int                  n_items;
    void                *addr;
    cda_d_local_write_f  do_write;
    void                *do_write_privptr;

    //
    int                  current_nelems;
    rflags_t             rflags;
    cx_time_t            timestamp;

    var_cbrec_t         *cb_list;
    int                  cb_list_allocd;
} varinfo_t;

typedef struct
{
    int            fd;
    fdio_handle_t  iohandle;
} lcninfo_t;

//////////////////////////////////////////////////////////////////////

enum
{
    VAR_MIN_VAL   = 1,
    VAR_MAX_COUNT = 10000,    // An arbitrary limit
    VAR_ALLOC_INC = 100,

    LCN_MIN_VAL   = 1,
    LCN_MAX_COUNT = 10000,    // An arbitrary limit
    LCN_ALLOC_INC = 2,        // Note: for inserver should be 10 or 100
};

static varinfo_t *vars_list        = NULL;
static int        vars_list_allocd = 0;

static lcninfo_t *lcns_list;
static int        lcns_list_allocd;


static int var_name_checker(varinfo_t *p, void *privptr)
{
    return strcasecmp(p->name, privptr) == 0;
}

//--------------------------------------------------------------------

// GetVarCbSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, VarCb, var_cbrec_t,
                                 cb, lcn, VARCB_VAL_UNUSED, VARCB_VAL_USED,
                                 0, 2, 0,
                                 vi->, vi,
                                 varinfo_t *vi, varinfo_t *vi)

static void RlsVarCbSlot(int id, varinfo_t *vi)
{
  var_cbrec_t *p = AccessVarCbSlot(id, vi);

    p->lcn = VARCB_VAL_UNUSED;
}

//--------------------------------------------------------------------

// GetVarSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Var, varinfo_t,
                                 vars, in_use, 0, 1,
                                 VAR_MIN_VAL, VAR_ALLOC_INC, VAR_MAX_COUNT,
                                 , , void)

static void RlsVarSlot(cda_d_local_t var)
{
  varinfo_t *vi = AccessVarSlot(var);

    if (vi->in_use == 0) return; /*!!! In fact, an internal error */
    vi->in_use = 0;

    DestroyVarCbSlotArray(vi);

    safe_free(vi->name);
}

static int CheckVar(cda_d_local_t var)
{
    if (var >= VAR_MIN_VAL  &&
        var < (cda_d_local_t)vars_list_allocd  &&  vars_list[var].in_use != 0)
        return 0;

    errno = EBADF;
    return -1;
}

//--------------------------------------------------------------------

// GetLcnSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Lcn, lcninfo_t,
                                 lcns, fd, LCN_VAL_UNUSED, LCN_VAL_USED,
                                 0, 2, 0,
                                 , , void)

static void RlsLcnSlot(int n)
{
  lcninfo_t *p = AccessLcnSlot(n);

    if (p->fd > 0)        close          (p->fd);
    if (p->iohandle >= 0) fdio_deregister(p->iohandle);
    p->fd = LCN_VAL_UNUSED;
}

//#### Public API ####################################################

cda_d_local_t  cda_d_local_register_chan(const char          *name,
                                         cxdtype_t            dtype,
                                         int                  n_items,
                                         void                *addr,
                                         cda_d_local_write_f  do_write,
                                         void                *do_write_privptr)
{
  cda_d_local_t  var;
  varinfo_t     *vi;

    /* 0. Checks */

    /* a. Check name sanity */
    if (name == NULL  ||  *name == '\0')
    {
        errno = ENOENT;
        return CDA_D_LOCAL_ERROR;
    }

    /* b. Check properties */
    if (addr == NULL)
    {
        errno = EFAULT;
        return CDA_D_LOCAL_ERROR;
    }
    /*!!! Other?*/

    /* c. Check duplicates */
    var = ForeachVarSlot(var_name_checker, name);
    if (var >= 0)
    {
        errno = EEXIST;
        return CDA_D_LOCAL_ERROR;
    }

    /* 1. Allocate */
    var = GetVarSlot();
    if (var < 0) return var;
    vi = AccessVarSlot(var);
    //
    if ((vi->name = strdup(name)) == NULL)
    {
        RlsVarSlot(var);
        return CDA_D_LOCAL_ERROR;
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
  lcninfo_t *lci = AccessLcnSlot(p->lcn);

fprintf(stderr, "%s(fd=%d,ioh=%d)\n", __FUNCTION__, lci->fd, lci->iohandle);
    fdio_send(lci->iohandle, &(p->client_hwr), sizeof(p->client_hwr));
    return 0;
}
int            cda_d_local_update_chan  (cda_d_local_t  var,
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

    ForeachVarCbSlot(CallVarCBs, NULL, vi);

    return 0;
}

static int update_cycle_sender(lcninfo_t *p, void *privptr)
{
  static cda_hwcnref_t val_cycle = HWR_VAL_CYCLE;

    fdio_send(p->iohandle, &val_cycle, sizeof(val_cycle));
    return 0;
}
void           cda_d_local_update_cycle (void)
{
    ForeachLcnSlot(update_cycle_sender, NULL);
}

//---- Hack/debug-API ------------------------------------------------

int            cda_d_local_chans_count  (void)
{
  int  ret;

    for (ret = vars_list_allocd - 1;  ret > 0;  ret--)
        if (CheckVar(ret) == 0) return ret + 1;

    return 0;
}

int            cda_d_local_chan_n_props (cda_d_local_t  var,
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

int            cda_d_local_override_wr  (cda_d_local_t  var,
                                         cda_d_local_write_f  do_write,
                                         void                *do_write_privptr)
{
  varinfo_t      *vi = AccessVarSlot(var);

    if (CheckVar(var) != 0) return -1;

    vi->do_write         = do_write;
    vi->do_write_privptr = do_write_privptr;

    return 0;
}

//#### Internal API ##################################################

static void ProcessFdioEvent(int uniq, void *privptr1,
                             fdio_handle_t handle, int reason,
                             void *inpkt, size_t inpktsize,
                             void *privptr2)
{
}

static int RegisterLocalSid(int fd)
{
  local_lcn_t  lcn;
  lcninfo_t   *p;

    lcn = GetLcnSlot();
    if (lcn < 0) return -1;
    p = AccessLcnSlot(lcn);

    p->fd       = fd;
    p->iohandle = fdio_register_fd(0, NULL, fd, FDIO_STREAM,
                                   ProcessFdioEvent, lint2ptr(lcn),
                                   sizeof(cda_hwcnref_t),
                                   0,
                                   0,
                                   0,
                                   FDIO_LEN_LITTLE_ENDIAN,
                                   1, sizeof(cda_hwcnref_t));
    if (p->iohandle < 0)
    {
        RlsLcnSlot(lcn);
        return -1;
    }
    
    return lcn;
}

static int RegisterLocalHwr(local_lcn_t    lcn,
                            cda_d_local_t  var, cda_hwcnref_t  client_hwr)
{
  varinfo_t      *vi = AccessVarSlot(var);
  int             n;
  var_cbrec_t    *p;

    n = GetVarCbSlot(vi);
    if (n < 0) return -1;
    p = AccessVarCbSlot(n, vi);
    p->lcn        = lcn;
    p->client_hwr = client_hwr;

    return 0;
}

static int WriteToVar(cda_d_local_t  var,
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

    cda_d_local_t  var;
} hwrinfo_t;

typedef struct
{
    cda_srvconn_t    sid;
    int              being_processed;
    int              being_destroyed;

    int              read_fd;
    sl_fdh_t         fdhandle;
    local_lcn_t      lcn;

    hwrinfo_t       *hwrs_list;
    int              hwrs_list_allocd;
} cda_d_local_privrec_t;

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
                                 cda_d_local_privrec_t *me, cda_d_local_privrec_t *me)

static void RlsHwrSlot(cda_hwcnref_t hwr, cda_d_local_privrec_t *me)
{
  hwrinfo_t *hi = AccessHwrSlot(hwr, me);

    if (hi->in_use == 0) return; /*!!! In fact, an internal error */
    hi->in_use = 0;
}


//--------------------------------------------------------------------

static void local_fd_p(int uniq, void *privptr1,
                       sl_fdh_t fdh, int fd, int mask, void *privptr2)
{
  cda_d_local_privrec_t *me = privptr1;

  cda_hwcnref_t          hwr;
  hwrinfo_t             *hi;
  varinfo_t             *vi;

  int                    r;

  int                    repcount;

    for (repcount = 100;  repcount > 0;  repcount--)
    {
        r = read(fd, &hwr, sizeof(hwr));
        if (r != sizeof(hwr))
        {
            /*!!!*/
            return;
        }
    
        me->being_processed++;
    
        if (hwr < 0)
        {
            if      (hwr == HWR_VAL_CYCLE)
                cda_dat_p_update_server_cycle(me->sid);
            else if (hwr == HWR_VAL_STRSCHG  ||
                     hwr == HWR_VAL_RDSCHG)
            {
                r = read(fd, &hwr, sizeof(hwr));
                if (r != sizeof(hwr))
                {
                    /*!!!*/
                    repcount = 0;
                    goto END_PROCESSED;
                }
                /* Here, in cda_d_local, for now -- do nothing */
            }
        }
        else
        {
            /*!!! Check hwr */
            hi = AccessHwrSlot(hwr, me);
            vi = AccessVarSlot(hi->var);
            cda_dat_p_update_dataset(me->sid,
                                     1, &(hi->dataref),
                                     &(vi->addr), &(vi->dtype), &(vi->current_nelems),
                                     &(vi->rflags), &(vi->timestamp),
                                     1);
        }
    
 END_PROCESSED:
        me->being_processed--;
        if (me->being_processed == 0  &&  me->being_destroyed)
        {
            /*!!!*/
        }
    }
}

static int  cda_d_local_new_chan(cda_dataref_t ref, const char *name,
                                 int options,
                                 cxdtype_t dtype, int nelems)
{
  cda_d_local_privrec_t *me;

  cda_d_local_t          var;
  varinfo_t             *vi;

  cda_hwcnref_t          hwr;
  hwrinfo_t             *hi;

  const char            *envval;
  void                  *vp;

    var = ForeachVarSlot(var_name_checker, name);
    if (var < 0)
    {
        if ((envval = getenv("CDA_D_LOCAL_AUTO_CREATE")) == NULL  ||
            (*envval != '1'  &&  tolower(*envval) != 'y')         ||
            (var = cda_d_local_register_chan(name, CXDTYPE_DOUBLE, 1,
                                             vp = malloc(sizeof(double)),
                                             NULL, NULL)) == CDA_D_LOCAL_ERROR)
        {
            cda_set_err("unknown channel");
            return CDA_DAT_P_ERROR;
        }
        *((double*)vp) = 0.0;
    }
    vi = AccessVarSlot(var);

    me = cda_dat_p_get_server(ref, &CDA_DAT_P_MODREC_NAME(local), NULL);
    if (me == NULL) return CDA_DAT_P_ERROR;

    hwr = GetHwrSlot(me);
    if (hwr < 0) return CDA_DAT_P_ERROR;
    hi = AccessHwrSlot(hwr, me);
    // Fill data
    hi->dataref = ref;
    hi->var     = var;

    // Subscribe to updates
    RegisterLocalHwr(me->lcn, var, hwr);

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

static int  cda_d_local_snd_data(void *pdt_privptr, cda_hwcnref_t hwr,
                                 cxdtype_t dtype, int nelems, void *value)
{
  cda_d_local_privrec_t *me = pdt_privptr;
  hwrinfo_t             *hi = AccessHwrSlot(hwr, me);

    if (hwr < 0  ||  hwr >= me->hwrs_list_allocd  ||
        hi->in_use == 0)
    {
        /*!!!Bark? */
        return CDA_PROCESS_ERR;
    }

    return WriteToVar(hi->var, dtype, nelems, value) == 0? CDA_PROCESS_DONE
                                                         : CDA_PROCESS_SEVERE_ERR;
}

static int  cda_d_local_new_srv (cda_srvconn_t  sid, void *pdt_privptr,
                                 int            uniq,
                                 const char    *srvrspec,
                                 const char    *argv0)
{
  cda_d_local_privrec_t *me = pdt_privptr;

  int                    filedes[2] = {-1, -1};
  sl_fdh_t               fdhandle   = -1; // <->filedes[0]

    me->sid = sid;
    /* Just precaution, for case if cda_core would someday call del_srv() on undercreated srvs */
    me->read_fd  = -1;
    me->fdhandle = -1;

    if (pipe(filedes) < 0) goto CLEANUP;
    set_fd_flags(filedes[0], O_NONBLOCK, 1);
    set_fd_flags(filedes[1], O_NONBLOCK, 1);
#if 0
    /* Code to test behaviour of select() against ro and wo descriptors */
    {
      int  n;
      int  r;

        for (n = 0;  n < 4;  n++)
        {
            errno = 0;
            r = check_fd_state(filedes[n/2], n&1?O_WRONLY:O_RDONLY);
            fprintf(stderr, "\tr=%d errno=%d/%s\n", r, errno, strerror(errno));
        }
    }
#endif
    if ((fdhandle = sl_add_fd(cda_dat_p_suniq_of_sid(me->sid), me,
                              filedes[0], SL_RD, local_fd_p, NULL)) < 0)
        goto CLEANUP;

    me->lcn = RegisterLocalSid(filedes[1]);
    if (me->lcn < 0) goto CLEANUP;

    me->read_fd  = filedes[0];
    me->fdhandle = fdhandle;

    return CDA_DAT_P_OPERATING;

 CLEANUP:
    if (filedes[0] >= 0) close(filedes[0]);
    if (filedes[1] >= 0) close(filedes[1]);
    if (fdhandle   >= 0) sl_del_fd(fdhandle);

    return CDA_DAT_P_ERROR;
}

static void cda_d_local_del_srv (cda_srvconn_t  sid, void *pdt_privptr)
{
  cda_d_local_privrec_t *me = pdt_privptr;
  
}

//////////////////////////////////////////////////////////////////////

CDA_DEFINE_DAT_PLUGIN(local, "Local data-access plugin",
                      NULL, NULL,
                      sizeof(cda_d_local_privrec_t),
                      '.', ':', '\0',
                      cda_d_local_new_chan, NULL,
                      cda_d_local_snd_data, NULL,
                      cda_d_local_new_srv,  cda_d_local_del_srv);
