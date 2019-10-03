#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

#include "misc_macros.h"
#include "cxscheduler.h"

#include "cda.h"
#include "cdaP.h"

#include "cda_d_epics.h"

#include "cadef.h"
#include "alarm.h"

#include "epics2cx_conv.h"


//////////////////////////////////////////////////////////////////////

enum {CA_HEARTBEAT_USECS = 100*1000*0 + 5000000*1}; // Use 5s for debugging

static int       is_initialized;
static sl_tid_t  ca_hbt_tid = -1;

static void CaHeartbeatProc(int uniq, void *unsdptr,
                            sl_tid_t tid, void *privptr)
{
    ca_hbt_tid = sl_enq_tout_after(0/*!!!uniq*/, NULL,
                                   CA_HEARTBEAT_USECS, CaHeartbeatProc, NULL);
    ca_poll();
}

//////////////////////////////////////////////////////////////////////

typedef struct
{
    int       fd;
    sl_fdh_t  handle;
} fd_info_t;

enum
{
    FDI_MIN_VAL   = 1,
    FDI_MAX_COUNT = 10000,  // An arbitrary limit; shouldn't be >FD_SETSIZE
    FDI_ALLOC_INC = 100,
};


static fd_info_t     *fdis_list;
static int            fdis_list_allocd;

//--------------------------------------------------------------------

// GetFdiSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Fdi, fd_info_t,
                                 fdis, fd, 0, 1,
                                 FDI_MIN_VAL, FDI_ALLOC_INC, FDI_MAX_COUNT,
                                 , , void)

static void RlsFdiSlot(int id)
{
  fd_info_t *fdi;

    if (fdi->fd == 0) return;
    sl_del_fd(fdi->handle);
    fdi->fd     = 0;
    fdi->handle = 0;
}

//--------------------------------------------------------------------

static void HandleCaInput(int uniq, void *privptr1,
                          sl_fdh_t fdh, int fd, int mask, void *privptr2)
{
    ca_poll();
}

static int  FindFdiIterator(fd_info_t *fdi, void *privptr)
{
    return fdi->fd == ptr2lint(privptr);
}
static void FdRegistrationCB(void *userargs, int fd, int opened)
{
  sl_fdh_t   handle;
  int        id;
  fd_info_t *fdi;

    if (opened)
    {
        // Register with cxscheduler
        handle = sl_add_fd(0, NULL, fd, SL_RD, HandleCaInput, NULL);
        if (handle < 0)
        {
            cda_dat_p_report(-1, "%s: sl_add_fd(fd=%d): %s",
                             __FUNCTION__, fd, strerror(errno));
            return;
        }
        // Get a slot
        id = GetFdiSlot();
        if (id < 0)
        {
            cda_dat_p_report(-1, "%s: GetFdiSlot for fd=%d failed",
                             __FUNCTION__, fd);
            sl_del_fd(handle);
            return;
        }
        // Fill data
        fdi = AccessFdiSlot(id);
        fdi->fd     = fd;
        fdi->handle = handle;
    }
    else
    {
        id = ForeachFdiSlot(FindFdiIterator, lint2ptr(fd));
        if (id < 0)
        {
            return;
        }
        RlsFdiSlot(id);
    }
}

//////////////////////////////////////////////////////////////////////

typedef struct
{
    int            in_use;

    cda_dataref_t  dataref; // "Backreference" to corr.entry in the global table
    cxdtype_t      dtype;

    char          *name;
    chid           ca_chid;

    cda_hwcnref_t  next;    // Link to next hwr of sid
    cda_hwcnref_t  prev;    // Link to previous hwr of sid

    struct _cda_d_epics_privrec_t_struct *me; // Backreference to a containing server
} hwrinfo_t;

typedef struct _cda_d_epics_privrec_t_struct
{
    cda_srvconn_t  sid;

    cda_hwcnref_t  frs_hwr;
    cda_hwcnref_t  lst_hwr;
} cda_d_epics_privrec_t;

//////////////////////////////////////////////////////////////////////

enum
{
    HWR_MIN_VAL   = 1,
    HWR_MAX_COUNT = 1000000,  // An arbitrary limit
    HWR_ALLOC_INC = 100,
};


static hwrinfo_t     *hwrs_list;
static int            hwrs_list_allocd;

//--------------------------------------------------------------------

// GetHwrSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Hwr, hwrinfo_t,
                                 hwrs, in_use, 0, 1,
                                 HWR_MIN_VAL, HWR_ALLOC_INC, HWR_MAX_COUNT,
                                 , , void)

static void RlsHwrSlot(cda_hwcnref_t hwr)
{
  hwrinfo_t *hi = AccessHwrSlot(hwr);

    if (hi->in_use == 0) return; /*!!! In fact, an internal error */
    safe_free(hi->name); hi->name = NULL;

    if (hi->ca_chid != NULL)
    {
        ca_clear_channel(hi->ca_chid);
        hi->ca_chid = NULL;
    }

    hi->in_use = 0;
}

//--------------------------------------------------------------------

static void AddHwrToSrv  (cda_d_epics_privrec_t *me, cda_hwcnref_t hwr)
{
  hwrinfo_t     *hi   = AccessHwrSlot(hwr);

    hi->prev = me->lst_hwr;
    hi->next = -1;

    if (me->lst_hwr >= 0)
        AccessHwrSlot(me->lst_hwr)->next = hwr;
    else
        me->frs_hwr                      = hwr;
    me->lst_hwr = hwr;

    hi->me = me;
}

static void DelHwrFromSrv(cda_d_epics_privrec_t *me, cda_hwcnref_t hwr)
{
  hwrinfo_t     *hi   = AccessHwrSlot(hwr);
  cda_hwcnref_t  prev = hi->prev;
  cda_hwcnref_t  next = hi->next;

    if (prev < 0) me->frs_hwr = next; else AccessHwrSlot(prev)->next = next;
    if (next < 0) me->lst_hwr = prev; else AccessHwrSlot(next)->prev = prev;

    hi->prev = hi->next = -1;

    hi->me = NULL;
}

//////////////////////////////////////////////////////////////////////

static void GetPropsCB   (struct event_handler_args      ARGS)
{
  cda_hwcnref_t          hwr = ptr2lint(ARGS.usr);
  hwrinfo_t             *hi  = AccessHwrSlot(hwr);
  cda_d_epics_privrec_t *me  = hi->me;

  const char            *units;
  cxdtype_t              dtype;
  CxAnyVal_t             range[2];

  const struct dbr_ctrl_short  *c_short;
  const struct dbr_ctrl_float  *c_float;
  const struct dbr_ctrl_enum   *c_enum;
  const struct dbr_ctrl_char   *c_char;
  const struct dbr_ctrl_long   *c_long;
  const struct dbr_ctrl_double *c_double;

    if (ARGS.status != ECA_NORMAL  ||  ARGS.dbr == NULL) return;
    if (DBR2cxdtype(ARGS.type, &dtype) != DBR_class_CTRL) return;

    units = NULL;
    switch (ARGS.type)
    {
        case DBR_CTRL_SHORT:
            c_short  = ARGS.dbr;
            units        = c_short->units;
            range[0].i16 = c_short->lower_ctrl_limit;
            range[1].i16 = c_short->upper_ctrl_limit;
            break;

        case DBR_CTRL_FLOAT:
            c_float  = ARGS.dbr;
            units        = c_float->units;
            range[0].f32 = c_float->lower_ctrl_limit;
            range[1].f32 = c_float->upper_ctrl_limit;
            break;

        case DBR_CTRL_ENUM:
            /* Nothing to do... */
            dtype = CXDTYPE_UNKNOWN;
            break;

        case DBR_CTRL_CHAR:
            /*!!! UINT8/TEXT?*/
            c_char   = ARGS.dbr;
            units        = c_char->units;
            range[0].i8  = c_char->lower_ctrl_limit;
            range[1].i8  = c_char->upper_ctrl_limit;
            break;

        case DBR_CTRL_LONG:
            c_long   = ARGS.dbr;
            units        = c_long->units;
            range[0].i32 = c_long->lower_ctrl_limit;
            range[1].i32 = c_long->upper_ctrl_limit;
            break;

        case DBR_CTRL_DOUBLE:
            c_double = ARGS.dbr;
            units        = c_double->units;
            range[0].f64 = c_double->lower_ctrl_limit;
            range[1].f64 = c_double->upper_ctrl_limit;
            break;

        default: dtype = CXDTYPE_UNKNOWN;
    }

    if (units != NULL)
        cda_dat_p_set_strings(hi->dataref,
                              NULL, NULL, NULL,  NULL,
                              NULL, NULL, units, NULL);
    if (dtype != CXDTYPE_UNKNOWN)
        cda_dat_p_set_range(hi->dataref, range, dtype);
}

static void StateChangeCB(struct connection_handler_args ARGS)
{
  cda_hwcnref_t          hwr = ptr2lint(ca_puser(ARGS.chid));
  hwrinfo_t             *hi  = AccessHwrSlot(hwr);
  cda_d_epics_privrec_t *me  = hi->me;

  int                    c_s = (ARGS.op == CA_OP_CONN_UP)? CDA_RSLVSTAT_FOUND
                                                         : CDA_RSLVSTAT_NOTFOUND;

  int                    stat;

  chtype                 DBR_type;

////fprintf(stderr, "%s: op=%d c_s=%d\n", __FUNCTION__, ARGS.op, c_s);
    if (c_s == CDA_RSLVSTAT_FOUND)
    {
        DBR_type = cxdtype2DBR(hi->dtype, DBR_class_CTRL);

        if (DBR_type != DBR_CTRL_STRING) /* dbr_ctrl_string is not implemented because is senseless */
            stat = ca_array_get_callback(DBR_type, 1, hi->ca_chid,
                                         GetPropsCB, lint2ptr(hwr));
    }
    cda_dat_p_report_rslvstat(hi->dataref, c_s);
    cda_dat_p_set_ready      (hi->dataref, c_s == CDA_RSLVSTAT_FOUND);
}

static void NewDataCB    (struct event_handler_args      ARGS)
{
  cda_hwcnref_t          hwr = ptr2lint(ARGS.usr);
  hwrinfo_t             *hi  = AccessHwrSlot(hwr);
  cda_d_epics_privrec_t *me  = hi->me;
  

  const void            *value_p;
  cxdtype_t              dtype;
  int                    nelems;
  rflags_t               rflags;
  epicsTimeStamp         epics_stamp;
  cx_time_t              timestamp;

    if (ARGS.status != ECA_NORMAL  ||  ARGS.dbr == NULL) return;
    if (DBR2cxdtype(ARGS.type, &dtype) != DBR_class_TIME) return;

    nelems = ARGS.count;
    rflags = alarm2rflags(((struct dbr_time_string *)(ARGS.dbr))->status);
    epics_stamp =         ((struct dbr_time_string *)(ARGS.dbr))->stamp;
    timestamp.sec  = epics_stamp.secPastEpoch + POSIX_TIME_AT_EPICS_EPOCH;
    timestamp.nsec = epics_stamp.nsec;

    switch (ARGS.type)
    {
        case DBR_TIME_STRING: value_p = &(((const struct dbr_time_string *)(ARGS.dbr))->value); break;
        case DBR_TIME_SHORT:
        /* ==DBR_TIME_INT */  value_p = &(((const struct dbr_time_short  *)(ARGS.dbr))->value); break;
        case DBR_TIME_FLOAT:  value_p = &(((const struct dbr_time_float  *)(ARGS.dbr))->value); break;
        case DBR_TIME_ENUM:   value_p = &(((const struct dbr_time_enum   *)(ARGS.dbr))->value); break;
        case DBR_TIME_CHAR:   value_p = &(((const struct dbr_time_char   *)(ARGS.dbr))->value); break;
        case DBR_TIME_LONG:   value_p = &(((const struct dbr_time_long   *)(ARGS.dbr))->value); break;
        case DBR_TIME_DOUBLE: value_p = &(((const struct dbr_time_double *)(ARGS.dbr))->value); break;
        default: return;
    }

    /*!!! timestamp -- ? */

    cda_dat_p_update_dataset(me->sid, 1, &(hi->dataref),
                             &value_p, &dtype, &nelems,
                             &rflags, &timestamp, CDA_DAT_P_IS_UPDATE);
}

static int  cda_d_epics_new_chan(cda_dataref_t ref, const char *name,
                                 int options,
                                 cxdtype_t dtype, int nelems)
{
  cda_d_epics_privrec_t *me;

  cda_hwcnref_t          hwr;
  hwrinfo_t             *hi;

  int                    stat;

  chtype                 DBR_type;

    if ((DBR_type = cxdtype2DBR(dtype, DBR_class_TIME)) < 0)
    {
        errno = EINVAL;
        return CDA_DAT_P_ERROR;
    }

    me = cda_dat_p_get_server(ref, &CDA_DAT_P_MODREC_NAME(epics), NULL, CDA_DAT_P_GET_SERVER_OPT_NONE);
    if (me == NULL) return CDA_DAT_P_ERROR;

    hwr = GetHwrSlot();
    if (hwr < 0) return CDA_DAT_P_ERROR;
    hi = AccessHwrSlot(hwr);
    // Fill data
    hi->next    = -1;
    hi->prev    = -1;
    hi->dataref = ref;
    hi->dtype   = dtype;
    if ((hi->name = strdup(name)) == NULL)
    {
        RlsHwrSlot(hwr);
        return CDA_DAT_P_ERROR;
    }
    AddHwrToSrv(me, hwr);
    cda_dat_p_set_hwr  (ref, hwr);

    stat = ca_create_channel(name,
                             StateChangeCB, lint2ptr(hwr),
                             CA_PRIORITY_DEFAULT,
                             &(hi->ca_chid));
    if (stat != ECA_NORMAL)
    {
        cda_dat_p_report(me->sid, "ca_create_channel(\"%s\") failed: %s",
                         name, ca_message(stat));
        RlsHwrSlot(hwr);
        return CDA_DAT_P_ERROR;
    }

    stat = ca_create_subscription(DBR_type, nelems, hi->ca_chid,
                                  DBE_VALUE | DBE_ALARM,
                                  NewDataCB, lint2ptr(hwr),
                                  NULL);
    if (stat != ECA_NORMAL)
    {
        cda_dat_p_report(me->sid, "ca_create_subscription(\"%s\") failed: %s",
                         name, ca_message(stat));
        RlsHwrSlot(hwr);
        return CDA_DAT_P_ERROR;
    }

    return CDA_DAT_P_NOTREADY;
}

static int  cda_d_epics_snd_data(void *pdt_privptr, cda_hwcnref_t hwr,
                                 cxdtype_t dtype, int nelems, void *value)
{
  cda_d_epics_privrec_t *me = pdt_privptr;
  hwrinfo_t             *hi = AccessHwrSlot(hwr);

  int                    stat;

  chtype                 DBR_type;

    /* A safety net */
    if (hwr < HWR_MIN_VAL  ||  hwr >= hwrs_list_allocd  ||
        hi->in_use == 0)
    {
        /* Ouch... A cda_core error? */
        cda_dat_p_report(-1,
                         "%s(me=%p): attempt to write to illegal hwr=%d (hwrs_list_allocd=%d)\n",
                         __FUNCTION__, me, hwr, hwrs_list_allocd);
        return CDA_PROCESS_SEVERE_ERR;
    }

    if ((DBR_type = cxdtype2DBR(dtype, DBR_class_PLAIN)) < 0)
        return CDA_PROCESS_ERR;
////fprintf(stderr, "%s: dtype=%d DBR_type=%d nelems=%d\n", __FUNCTION__, dtype, DBR_type, nelems);

    stat = ca_array_put(DBR_type, nelems, hi->ca_chid, value);
    if (stat != ECA_NORMAL)
    {
        cda_ref_p_report(hi->dataref, "ca_array_put() failed: %s",
                         ca_message(stat));
        return CDA_PROCESS_ERR;
    }
    /*!!! do we need something like ca_pend_io(0.0) or ca_poll() here? */
    //ca_pend_io(0.1+5);
    //ca_poll();

    return CDA_PROCESS_DONE;
}

//////////////////////////////////////////////////////////////////////

static int  cda_d_epics_new_srv (cda_srvconn_t  sid, void *pdt_privptr,
                                 int            uniq,
                                 const char    *srvrspec,
                                 const char    *argv0,
                                 int            srvtype  __attribute((unused)))
{
  cda_d_epics_privrec_t *me = pdt_privptr;
  int                    ec;

////fprintf(stderr, "ZZZ %s(%s)\n", __FUNCTION__, srvrspec);
    me->sid = sid;
    me->frs_hwr = -1;
    me->lst_hwr = -1;

    return CDA_DAT_P_OPERATING;
}

static int  cda_d_epics_del_srv (cda_srvconn_t  sid, void *pdt_privptr)
{
  cda_d_epics_privrec_t *me = pdt_privptr;

    return CDA_DAT_P_DEL_SRV_SUCCESS;  
}

//////////////////////////////////////////////////////////////////////

static int  cda_d_epics_init_m(void)
{
  int  r;

    r = ca_context_create(ca_disable_preemptive_callback);
    if (r != ECA_NORMAL)
    {
        cda_dat_p_report(-1, "%s(): ca_context_create()=%d, !=ECA_NORMAL=%d: %s",
                         __FUNCTION__, r, ECA_NORMAL, ca_message(r));
        return -1;
    }
    r = ca_add_fd_registration(FdRegistrationCB, NULL);
    if (r != ECA_NORMAL)
        cda_dat_p_report(-1, "%s(): ca_add_fd_registration()=%d, !=ECA_NORMAL=%d: %s",
                         __FUNCTION__, r, ECA_NORMAL, ca_message(r));

    r = cda_register_dat_plugin(&(CDA_DAT_P_MODREC_NAME(epics)));
    if (r < 0) return -1;

    ca_hbt_tid = sl_enq_tout_after(0/*!!!uniq*/, NULL,
                                   CA_HEARTBEAT_USECS, CaHeartbeatProc, NULL);
                                   
    is_initialized = 1;

    return 0;
}

static void cda_d_epics_term_m(void)
{
    if (!is_initialized) return;
    if (ca_hbt_tid >= 0)
    {
        sl_deq_tout(ca_hbt_tid); ca_hbt_tid = -1;
    }
    DestroyFdiSlotArray();
    DestroyHwrSlotArray();

    cda_deregister_dat_plugin(&(CDA_DAT_P_MODREC_NAME(epics)));

    ca_add_fd_registration(NULL, NULL);
    ca_context_destroy();
}

//////////////////////////////////////////////////////////////////////

CDA_DEFINE_DAT_PLUGIN(epics, "EPICS (Channel Access) data-access plugin",
                      cda_d_epics_init_m, cda_d_epics_term_m,
                      sizeof(cda_d_epics_privrec_t),
                      CDA_DAT_P_FLAG_NONE,
                      '.', ':', '@',
                      cda_d_epics_new_chan, NULL,
                      NULL,
                      cda_d_epics_snd_data, NULL,
                      cda_d_epics_new_srv,  cda_d_epics_del_srv);
