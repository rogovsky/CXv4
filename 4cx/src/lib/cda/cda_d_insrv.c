#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>

#include "misc_macros.h"
#include "misclib.h"
#include "cxscheduler.h"
#include "fdiolib.h"

#include "cda.h"
#include "cdaP.h"

#include "cda_d_insrv.h"
#include "cxsd_hw.h"
#include "cxsd_hwP.h"


enum {RDS_MAX_COUNT = 20};

//#### Internals #####################################################

enum {LCN_VAL_UNUSED   = -1, LCN_VAL_USED   = 0};

enum
{   /*!!! Should better unify with CXSD_HW_CHAN_R_nnn */
    HWR_VAL_CYCLE    = -1,
    HWR_VAL_STRSCHG  = -2,
    HWR_VAL_RDSCHG   = -3,
    HWR_VAL_FRESHCHG = -4,
};

typedef int insrv_lcn_t;

typedef struct
{
    int            fd;
    fdio_handle_t  iohandle;
    int            uniq;

    cxsd_gchnid_t *periodics;
    int            periodics_allocd;
    int            periodics_used;
} lcninfo_t;

//////////////////////////////////////////////////////////////////////

enum
{
    LCN_MIN_VAL   = 1,
    LCN_MAX_COUNT = 10000,    // An arbitrary limit
    LCN_ALLOC_INC = 2,        // Note: for inserver should be 10 or 100
};

static lcninfo_t *lcns_list;
static int        lcns_list_allocd;

//--------------------------------------------------------------------

static void InsrvCycleEvproc(int   uniq,
                             void *privptr1,
                             int   reason,
                             void *privptr2);

// GetLcnSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Lcn, lcninfo_t,
                                 lcns, fd, LCN_VAL_UNUSED, LCN_VAL_USED,
                                 LCN_MIN_VAL, LCN_ALLOC_INC, LCN_MAX_COUNT,
                                 , , void)

static void UnRegisterInsrvSid(insrv_lcn_t lcn); /*!!! BAD-BAD-BAD!!!*/
static void RlsLcnSlot(int n)
{
  lcninfo_t *p = AccessLcnSlot(n);

    CxsdHwDelCycleEvproc(p->uniq, NULL, InsrvCycleEvproc, lint2ptr(n));
    if (p->fd > 0)        close          (p->fd);
    if (p->iohandle >= 0) fdio_deregister(p->iohandle);
    safe_free(p->periodics);

    p->fd = LCN_VAL_UNUSED;
    UnRegisterInsrvSid(n);
}

//#### Internal API ##################################################

static void ProcessFdioEvent(int uniq, void *privptr1,
                             fdio_handle_t handle, int reason,
                             void *inpkt, size_t inpktsize,
                             void *privptr2)
{
}

static void InsrvCycleEvproc(int   uniq,
                             void *privptr1,
                             int   reason,
                             void *privptr2)
{
  insrv_lcn_t  lcn = ptr2lint(privptr2);
  lcninfo_t   *p   = AccessLcnSlot(lcn);

  static cda_hwcnref_t val_cycle = HWR_VAL_CYCLE;

//fprintf(stderr, "%s reason=%d p_used=%d\n", __FUNCTION__, reason, p->periodics_used);
    if      (reason == CXSD_HW_CYCLE_R_BEGIN)
        CxsdHwDoIO(p->uniq, DRVA_READ,
                   p->periodics_used, p->periodics,
                   NULL, NULL, NULL);
    else if (reason == CXSD_HW_CYCLE_R_END)
        fdio_send(p->iohandle, &val_cycle, sizeof(val_cycle));
}
static int  RegisterInsrvSid  (int uniq, int fd)
{
  insrv_lcn_t  lcn;
  lcninfo_t   *p;

    lcn = GetLcnSlot();
    if (lcn < 0) return -1;
    p = AccessLcnSlot(lcn);

    p->fd       = fd;
    p->iohandle = fdio_register_fd(uniq, NULL, fd, FDIO_STREAM,
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
    p->uniq     = uniq;

    CxsdHwAddCycleEvproc(   uniq, NULL, InsrvCycleEvproc, lint2ptr(lcn));
    
    return lcn;
}

static void UnRegisterInsrvSid(insrv_lcn_t lcn)
{
  lcninfo_t   *p;

    p = AccessLcnSlot(lcn);
    CxsdHwDelCycleEvproc(p->uniq, NULL, InsrvCycleEvproc, lint2ptr(lcn));
}

enum {GCN_EVMASK = CXSD_HW_CHAN_EVMASK_UPDATE   |
                   CXSD_HW_CHAN_EVMASK_STRSCHG  |
                   CXSD_HW_CHAN_EVMASK_RDSCHG   |
                   CXSD_HW_CHAN_EVMASK_FRESHCHG};

static void GcnEvproc(int            uniq,
                      void          *privptr1,
                      cxsd_gchnid_t  gcid,
                      int            reason,
                      void          *privptr2)
{
  insrv_lcn_t    lcn        = ptr2lint(privptr1);
  lcninfo_t     *p          = AccessLcnSlot(lcn);
  cda_hwcnref_t  to_send[2]; // [0]=reason_code, [1]=client_hwr

    to_send[1] = ptr2lint(privptr2);

    if      (reason == CXSD_HW_CHAN_R_UPDATE)
    {
        fdio_send(p->iohandle, to_send+1, sizeof(to_send[1]));
    }
    else if (reason == CXSD_HW_CHAN_R_STRSCHG)
    {
        to_send[0] = HWR_VAL_STRSCHG;
        fdio_send(p->iohandle, to_send,   sizeof(to_send));
    }
    else if (reason == CXSD_HW_CHAN_R_RDSCHG)
    {
        to_send[0] = HWR_VAL_RDSCHG;
        fdio_send(p->iohandle, to_send,   sizeof(to_send));
    }
    else if (reason == CXSD_HW_CHAN_R_FRESHCHG)
    {
        to_send[0] = HWR_VAL_FRESHCHG;
        fdio_send(p->iohandle, to_send,   sizeof(to_send));
    }
}
static int  RegisterInsrvHwr  (insrv_lcn_t    lcn,
                               cxsd_gchnid_t  gcid, cda_hwcnref_t  client_hwr)
{
  lcninfo_t     *p          = AccessLcnSlot(lcn);
  cxsd_gchnid_t *new_periodics;

  enum {CHANS_ALLOC_INCREMENT = 100};

    CxsdHwAddChanEvproc(p->uniq, lint2ptr(lcn),
                        gcid,
                        GCN_EVMASK,
                        GcnEvproc, lint2ptr(client_hwr));

    if (p->periodics_used >= p->periodics_allocd)
    {
        new_periodics =
            safe_realloc(p->periodics,
                         sizeof(*new_periodics) * (p->periodics_allocd + CHANS_ALLOC_INCREMENT));
        if (new_periodics == NULL) return 0; /*!!! In fact, an error! */
        p->periodics         = new_periodics;
        p->periodics_allocd += CHANS_ALLOC_INCREMENT;
    }
    p->periodics[p->periodics_used++] = gcid;

    return 0;
}

static void UnRegisterInsrvHwr(insrv_lcn_t    lcn,
                               cxsd_gchnid_t  gcid, cda_hwcnref_t  client_hwr)
{
  lcninfo_t     *p          = AccessLcnSlot(lcn);

    CxsdHwDelChanEvproc(p->uniq, lint2ptr(lcn),
                        gcid,
                        GCN_EVMASK,
                        GcnEvproc, lint2ptr(client_hwr));
}

//#### Data-access plugin ############################################

typedef struct
{
    int            in_use;

    cda_dataref_t  dataref; // "Backreference" to corr.entry in the global table

    cxsd_cpntid_t  cpid;
    cxsd_gchnid_t  gcid;
} hwrinfo_t;

typedef struct
{
    cda_srvconn_t    sid;
    int              being_processed;
    int              being_destroyed;

    int              read_fd;
    sl_fdh_t         fdhandle;
    insrv_lcn_t      lcn;
    int              uniq;

    hwrinfo_t       *hwrs_list;
    int              hwrs_list_allocd;
} cda_d_insrv_privrec_t;

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
                                 cda_d_insrv_privrec_t *me, cda_d_insrv_privrec_t *me)

static void RlsHwrSlot(cda_hwcnref_t hwr, cda_d_insrv_privrec_t *me)
{
  hwrinfo_t *hi = AccessHwrSlot(hwr, me);

    if (hi->in_use == 0) return; /*!!! In fact, an internal error */
    hi->in_use = 0;
    UnRegisterInsrvHwr(me->lcn, hi->gcid, hwr);
}


//--------------------------------------------------------------------

static int DestroyInsrv_HwrIterator(hwrinfo_t *hi, void *privptr)
{
  cda_d_insrv_privrec_t *me  = privptr;
  cda_hwcnref_t          hwr = hi - me->hwrs_list; /* "hi2hwr()" */

    RlsHwrSlot(hwr, me);

    return 0;
}
static void DestroyInsrvPrivrec(cda_d_insrv_privrec_t *me)
{
    ForeachHwrSlot(DestroyInsrv_HwrIterator, me, me); DestroyHwrSlotArray(me);
    if (me->read_fd  >= 0)           close     (me->read_fd);  me->read_fd  = -1;
    if (me->fdhandle >= 0)           sl_del_fd (me->fdhandle); me->fdhandle = -1;
    if (me->lcn      >= LCN_MIN_VAL) RlsLcnSlot(me->lcn);      me->lcn      = -1;
}

//--------------------------------------------------------------------

static void insrv_fd_p(int uniq, void *privptr1,
                       sl_fdh_t fdh, int fd, int mask, void *privptr2)
{
  cda_d_insrv_privrec_t *me = privptr1;

  cda_hwcnref_t          hwr;
  hwrinfo_t             *hi;
  cxsd_hw_chan_t        *chn_p;
  int                    opcode;

  int                    r;

  int                    repcount;

  cxsd_gchnid_t          dummy_gcid;
  int                    phys_count;
  double                 rds_buf[RDS_MAX_COUNT*2];
  char                  *ident;
  char                  *label;
  char                  *tip;
  char                  *comment;
  char                  *geoinfo;
  char                  *rsrvd6;
  char                  *units;
  char                  *dpyfmt;

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
            opcode = hwr;
            if      (opcode == HWR_VAL_CYCLE)
                cda_dat_p_update_server_cycle(me->sid);
            else if (opcode == HWR_VAL_STRSCHG   ||
                     opcode == HWR_VAL_RDSCHG    ||
                     opcode == HWR_VAL_FRESHCHG)
            {
                r = read(fd, &hwr, sizeof(hwr));
                if (r != sizeof(hwr))
                {
                    /*!!!*/
                    repcount = 0;
                    goto END_PROCESSED;
                }

                hi = AccessHwrSlot(hwr, me);
                if (hwr < 0  ||  hwr >= me->hwrs_list_allocd  ||
                    hi->in_use == 0) goto END_PROCESSED;
                chn_p = cxsd_hw_channels + hi->gcid;
                if      (opcode == HWR_VAL_STRSCHG)
                {
                    CxsdHwGetCpnProps(hi->cpid, &dummy_gcid,
                                      &phys_count, rds_buf, RDS_MAX_COUNT,
                                      &ident,   &label,  &tip,   &comment,
                                      &geoinfo, &rsrvd6, &units, &dpyfmt);
                    cda_dat_p_set_strings  (hi->dataref,
                                            ident,   label,  tip,   comment,
                                            geoinfo, rsrvd6, units, dpyfmt);
                }
                else if (opcode == HWR_VAL_RDSCHG)
                {
                    CxsdHwGetCpnProps(hi->cpid, &dummy_gcid,
                                      &phys_count, rds_buf, RDS_MAX_COUNT,
                                      NULL, NULL, NULL, NULL,
                                      NULL, NULL, NULL, NULL);
                    cda_dat_p_set_phys_rds (hi->dataref, phys_count, rds_buf);
                }
                else if (opcode == HWR_VAL_FRESHCHG)
                {
                    cda_dat_p_set_fresh_age(hi->dataref, chn_p->fresh_age);
                }
            }
        }
        else
        {
            hi = AccessHwrSlot(hwr, me);
            if (hwr < 0  ||  hwr >= me->hwrs_list_allocd  ||
                hi->in_use == 0) goto END_PROCESSED;
            chn_p = cxsd_hw_channels + hi->gcid;
            cda_dat_p_update_dataset(me->sid,
                                     1, &(hi->dataref),
                                     &(chn_p->current_val),
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
                                     &(chn_p->current_dtype),
#else
                                     &(chn_p->dtype),
#endif
                                     &(chn_p->current_nelems),
                                     &(chn_p->rflags),
                                     &(chn_p->timestamp),
                                     1);
        }
    
 END_PROCESSED:
        me->being_processed--;
        if (me->being_processed == 0  &&  me->being_destroyed)
        {
            DestroyInsrvPrivrec(me);
            cda_dat_p_del_server_finish(me->sid);
            return;
        }
    }
}

static int  cda_d_insrv_new_chan(cda_dataref_t ref, const char *name,
                                 int options,
                                 cxdtype_t dtype, int nelems)
{
  cda_d_insrv_privrec_t *me;

  cxsd_cpntid_t          cpid;
  cxsd_gchnid_t          gcid;
  int                    phys_count;
  double                 rds_buf[RDS_MAX_COUNT*2];
  char                  *ident;
  char                  *label;
  char                  *tip;
  char                  *comment;
  char                  *geoinfo;
  char                  *rsrvd6;
  char                  *units;
  char                  *dpyfmt;

  cda_hwcnref_t          hwr;
  hwrinfo_t             *hi;
  cxsd_hw_chan_t        *chn_p;

    cpid = CxsdHwResolveChan(name, &gcid,
                             &phys_count, rds_buf, RDS_MAX_COUNT,
                             &ident,   &label,  &tip,   &comment,
                             &geoinfo, &rsrvd6, &units, &dpyfmt);
    if (cpid < 0)
    {
        cda_set_err("unknown channel");
        return CDA_DAT_P_ERROR;
    }

    me = cda_dat_p_get_server(ref, &CDA_DAT_P_MODREC_NAME(insrv), NULL, CDA_DAT_P_GET_SERVER_OPT_NONE);
    if (me == NULL) return CDA_DAT_P_ERROR;

    hwr = GetHwrSlot(me);
    if (hwr < 0) return CDA_DAT_P_ERROR;
    hi = AccessHwrSlot(hwr, me);
    // Fill data
    hi->dataref = ref;
    hi->cpid    = cpid;
    hi->gcid    = gcid;

    chn_p = cxsd_hw_channels + hi->gcid;

    cda_dat_p_set_hwr      (ref, hwr);
    cda_dat_p_set_ready    (ref, 1);
    // Set properties
    cda_dat_p_set_phys_rds (ref, phys_count, rds_buf);
    cda_dat_p_set_fresh_age(ref, chn_p->fresh_age);
    cda_dat_p_set_strings  (ref,
                            ident,   label,  tip,   comment,
                            geoinfo, rsrvd6, units, dpyfmt);

    // Subscribe to updates
    RegisterInsrvHwr(me->lcn, gcid, hwr);

    // Return current data
    if (chn_p->timestamp.sec != 0  ||  1)
        cda_dat_p_update_dataset(me->sid,
                                 1, &(hi->dataref),
                                 &(chn_p->current_val),
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
                                 &(chn_p->current_dtype),
#else
                                 &(chn_p->dtype),
#endif
                                 &(chn_p->current_nelems),
                                 &(chn_p->rflags),
                                 &(chn_p->timestamp),
                                 chn_p->is_internal
#if 1
                                 || 
                                 (chn_p->rw  &&
                                  chn_p->timestamp.sec  != CX_TIME_SEC_NEVER_READ)
                                 ||
                                 (chn_p->is_autoupdated       &&
                                  chn_p->fresh_age.sec  == 0  &&
                                  chn_p->fresh_age.nsec == 0  &&
                                  chn_p->timestamp.sec  != CX_TIME_SEC_NEVER_READ)
#endif
                                                   ? 1 : 0);

    return CDA_DAT_P_OPERATING;
}

static void cda_d_insrv_del_chan(void *pdt_privptr, cda_hwcnref_t hwr)
{
  cda_d_insrv_privrec_t *me = pdt_privptr;
  hwrinfo_t             *hi = AccessHwrSlot(hwr, me);

    if (hwr < 0  ||  hwr >= me->hwrs_list_allocd  ||
        hi->in_use == 0)
    {
        /*!!!Bark? */
        return;
    }

    RlsHwrSlot(hwr, me);
}

static int  cda_d_insrv_snd_data(void *pdt_privptr, cda_hwcnref_t hwr,
                                 cxdtype_t dtype, int nelems, void *value)
{
  cda_d_insrv_privrec_t *me = pdt_privptr;
  hwrinfo_t             *hi = AccessHwrSlot(hwr, me);

    if (hwr < 0  ||  hwr >= me->hwrs_list_allocd  ||
        hi->in_use == 0)
    {
        /*!!!Bark? */
        return CDA_PROCESS_SEVERE_ERR;
    }

    CxsdHwDoIO(me->uniq,
               DRVA_WRITE,
               1, &(hi->gcid),
               &dtype, &nelems, &value);

    return CDA_PROCESS_DONE;
}

static int  cda_d_insrv_new_srv (cda_srvconn_t  sid, void *pdt_privptr,
                                 int            uniq,
                                 const char    *srvrspec,
                                 const char    *argv0)
{
  cda_d_insrv_privrec_t *me = pdt_privptr;

  int                    filedes[2] = {-1, -1};
  sl_fdh_t               fdhandle   = -1; // <->filedes[0]

    me->sid = sid;
    /* Just precaution, for case if cda_core would someday call del_srv() on undercreated srvs */
    me->read_fd  = -1;
    me->fdhandle = -1;

    if (pipe(filedes) < 0) goto CLEANUP;
    set_fd_flags(filedes[0], O_NONBLOCK, 1);
    set_fd_flags(filedes[1], O_NONBLOCK, 1);
    if ((fdhandle = sl_add_fd(cda_dat_p_suniq_of_sid(me->sid), me,
                              filedes[0], SL_RD, insrv_fd_p, NULL)) < 0)
        goto CLEANUP;

    me->lcn = RegisterInsrvSid(uniq, filedes[1]);
    if (me->lcn < 0) goto CLEANUP;

    me->read_fd  = filedes[0];
    me->fdhandle = fdhandle;
    me->uniq     = uniq;

    return CDA_DAT_P_OPERATING;

 CLEANUP:
    if (filedes[0] >= 0) close(filedes[0]);
    if (filedes[1] >= 0) close(filedes[1]);
    if (fdhandle   >= 0) sl_del_fd(fdhandle);

    return CDA_DAT_P_ERROR;
}

static int  cda_d_insrv_del_srv (cda_srvconn_t  sid, void *pdt_privptr)
{
  cda_d_insrv_privrec_t *me = pdt_privptr;

    if (me->being_processed)
    {
        me->being_destroyed = 1;
        return CDA_DAT_P_DEL_SRV_DEFERRED;
    }
    else
    {
        DestroyInsrvPrivrec(me);
        return CDA_DAT_P_DEL_SRV_SUCCESS;
    }
}

//////////////////////////////////////////////////////////////////////

static int  cda_d_insrv_init_m(void)
{
    return cda_register_dat_plugin(&(CDA_DAT_P_MODREC_NAME(insrv))) < 0? -1 : 0;
}

static void cda_d_insrv_term_m(void)
{
    cda_deregister_dat_plugin(&(CDA_DAT_P_MODREC_NAME(insrv)));
}

CDA_DEFINE_DAT_PLUGIN(insrv, "Insrv (inserver) data-access plugin",
                      cda_d_insrv_init_m, cda_d_insrv_term_m,
                      sizeof(cda_d_insrv_privrec_t),
                      '.', ':', '\0',
                      cda_d_insrv_new_chan, cda_d_insrv_del_chan,
                      cda_d_insrv_snd_data, NULL,
                      cda_d_insrv_new_srv,  cda_d_insrv_del_srv);
