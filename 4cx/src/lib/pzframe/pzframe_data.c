#include <stdio.h>
#include <ctype.h>
#include <math.h>

#include "misclib.h"
#include "timeval_utils.h"

#include "cda.h"

#include "pzframe_data.h"


psp_paramdescr_t  pzframe_data_text2cfg[] =
{
    PSP_P_FLAG   ("readonly", pzframe_cfg_t, readonly, 1, 0),
    PSP_P_INT    ("maxfrq",   pzframe_cfg_t, maxfrq,   0, 0, 100),
    PSP_P_FLAG   ("loop",     pzframe_cfg_t, running,  1, 1),
    PSP_P_FLAG   ("pause",    pzframe_cfg_t, running,  0, 0),
    PSP_P_END()
};

//////////////////////////////////////////////////////////////////////

// GetCblistSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Cblist, pzframe_data_cbinfo_t,
                                 cb, cb, NULL, (void*)1,
                                 0, 2, 0,
                                 pfr->, pfr,
                                 pzframe_data_t *pfr, pzframe_data_t *pfr)

void PzframeDataCallCBs(pzframe_data_t *pfr,
                        int             reason,
                        int             info_int)
{
  int                  x;

    if (pfr->ftd->vmt.evproc != NULL)
        pfr->ftd->vmt.evproc(pfr, reason, info_int, NULL);

    for (x = 0;  x < pfr->cb_list_allocd;  x++)
        if (pfr->cb_list[x].cb != NULL)
            pfr->cb_list[x].cb(pfr, reason,
                               info_int, pfr->cb_list[x].privptr);
}

//////////////////////////////////////////////////////////////////////

static void ProcessOneChanUpdate(pzframe_data_t *pfr,
                                 int cn, cda_dataref_t ref, /* Note: ref is required for updated-during-initialization cases, such as "insrv::" */
                                 cxdtype_t *dtype_p, int *max_nelems_p)
{
  pzframe_type_dscr_t *ftd = pfr->ftd;

  cxdtype_t            dtype;
  int                  max_nelems;
  size_t               valsize;
  void                *dst;

    dtype      = ftd->chan_dscrs[cn].dtype;
    max_nelems = ftd->chan_dscrs[cn].max_nelems;
    if (dtype      == 0) dtype      = CXDTYPE_INT32;
    if (max_nelems == 0) max_nelems = 1;
    valsize = sizeof_cxdtype(dtype) * max_nelems;

    /* Shift generations */
    pfr->prv_data[cn].valbuf    = pfr->cur_data[cn].valbuf;
    pfr->prv_data[cn].rflags    = pfr->cur_data[cn].rflags;
    pfr->prv_data[cn].timestamp = pfr->cur_data[cn].timestamp;

    /* Get current value */
    if (valsize > sizeof(pfr->cur_data[cn].valbuf))
        dst = pfr->cur_data[cn].current_val;
    else
        dst = &(pfr->cur_data[cn].valbuf);
/*!!! Take {r,d}s into account!!! */

    cda_get_ref_data(ref, 0, valsize, dst);
    cda_get_ref_stat(ref,
                     &(pfr->cur_data[cn].rflags),
                     &(pfr->cur_data[cn].timestamp));

    if (dtype_p      != NULL) *dtype_p      = dtype;
    if (max_nelems_p != NULL) *max_nelems_p = max_nelems;
}

static void PzframeDataEvproc(int            uniq,
                              void          *privptr1,
                              cda_dataref_t  ref,
                              int            reason,
                              void          *info_ptr,
                              void          *privptr2)
{
  pzframe_data_t      *pfr = privptr2;
  pzframe_type_dscr_t *ftd = pfr->ftd;

  struct timeval       timenow;
  struct timeval       timediff;

  int                  cn;
  int                  info_changed;
  rflags_t             rflags;
  cxdtype_t            dtype;
  int                  max_nelems;

////fprintf(stderr, "%s %s %p\n", strcurtime(), __FUNCTION__, pfr);
    if (reason == CDA_REF_R_RSLVSTAT)
    {
        if (ptr2lint(info_ptr) != CDA_RSLVSTAT_FOUND)
            for (cn = 0;  cn < ftd->chan_count;  cn++)
                pfr->cur_data[cn].rflags = CXCF_FLAG_NOTFOUND;
        PzframeDataCallCBs(pfr, PZFRAME_REASON_RSLVSTAT, ptr2lint(info_ptr));
        return;
    }

    if (reason != CDA_REF_R_UPDATE)                return;
    if (!(pfr->cfg.running  ||  pfr->cfg.oneshot)) return;
    gettimeofday(&timenow, NULL);
    if (pfr->cfg.oneshot == 0  &&  pfr->cfg.maxfrq != 0)
    {
        timeval_subtract(&timediff, &timenow, &(pfr->timeupd));
        if (timediff.tv_sec  == 0  &&
            timediff.tv_usec <  1000000 / pfr->cfg.maxfrq)
            return;
    }
    pfr->timeupd     = timenow;
    PzframeDataSetRunMode(pfr, -1, 0);

    /* Get data and find if "info" had changed */
    for (cn = 0,
             info_changed = pfr->rds_had_changed, pfr->rds_had_changed = 0,
             rflags = 0;
         cn < ftd->chan_count;
         cn++)
    {
        if (ftd->chan_dscrs[cn].name != NULL  &&
            (
             (ftd->chan_dscrs[cn].chan_type & PZFRAME_CHAN_TYPE_MASK) == PZFRAME_CHAN_IS_FRAME
             ||
             (ftd->chan_dscrs[cn].chan_type & PZFRAME_CHAN_IMMEDIATE_MASK) == 0
            )
           )
        {
            ProcessOneChanUpdate(pfr, cn, pfr->refs[cn], &dtype, &max_nelems);
            if ((ftd->chan_dscrs[cn].chan_type & PZFRAME_CHAN_TYPE_MASK) == PZFRAME_CHAN_IS_FRAME)
                rflags |= pfr->cur_data[cn].rflags;

            /* Compare */
            if (!info_changed                         &&
                ftd->chan_dscrs[cn].change_important  &&
                max_nelems == 1)
            {
                if      (dtype == CXDTYPE_INT32  ||  dtype == CXDTYPE_UINT32)
                {
                    if (pfr->prv_data[cn].valbuf.i32 != 
                        pfr->cur_data[cn].valbuf.i32) info_changed = 1;
                }
                else if (dtype == CXDTYPE_INT16  ||  dtype == CXDTYPE_UINT16)
                {
                    if (pfr->prv_data[cn].valbuf.i16 != 
                        pfr->cur_data[cn].valbuf.i16) info_changed = 1;
                }
                else if (dtype == CXDTYPE_INT8   ||  dtype == CXDTYPE_UINT8)
                {
                    if (pfr->prv_data[cn].valbuf.i8  != 
                        pfr->cur_data[cn].valbuf.i8)  info_changed = 1;
                }
#if MAY_USE_INT64  ||  1
                else if (dtype == CXDTYPE_INT64  ||  dtype == CXDTYPE_UINT64)
                {
                    if (pfr->prv_data[cn].valbuf.i64 != 
                        pfr->cur_data[cn].valbuf.i64) info_changed = 1;
                }
#endif /* MAY_USE_INT64 */
                else if (dtype == CXDTYPE_DOUBLE)
                {
                    if (pfr->prv_data[cn].valbuf.f64 != 
                        pfr->cur_data[cn].valbuf.f64) info_changed = 1;
                }
                else if (dtype == CXDTYPE_SINGLE)
                {
                    if (pfr->prv_data[cn].valbuf.f32 != 
                        pfr->cur_data[cn].valbuf.f32) info_changed = 1;
                }
                else if (dtype == CXDTYPE_TEXT)
                {
                    if (pfr->prv_data[cn].valbuf.c8  != 
                        pfr->cur_data[cn].valbuf.c8)  info_changed = 1;
                }
                else if (dtype == CXDTYPE_UCTEXT)
                {
                    if (pfr->prv_data[cn].valbuf.c32 != 
                        pfr->cur_data[cn].valbuf.c32) info_changed = 1;
                }
            }
        }
    }
    pfr->rflags = rflags;

    /* Call upstream */
    PzframeDataCallCBs(pfr, PZFRAME_REASON_DATA, info_changed);
}

static void PzframeRDsCEvproc(int            uniq,
                              void          *privptr1,
                              cda_dataref_t  ref,
                              int            reason,
                              void          *info_ptr,
                              void          *privptr2)
{
  pzframe_data_dpl_t  *dpl = privptr2;
  pzframe_data_t      *pfr = dpl->p;
  int                  cn  = dpl->n;

    /*!!! Remember for info_changed somehow? */
    /* Call upstream */
    PzframeDataCallCBs(pfr, PZFRAME_REASON_RDSCHG, cn);
}

static void PzframeChanEvproc(int            uniq,
                              void          *privptr1,
                              cda_dataref_t  ref,
                              int            reason,
                              void          *info_ptr,
                              void          *privptr2)
{
  pzframe_data_dpl_t  *dpl = privptr2;
  pzframe_data_t      *pfr = dpl->p;
  int                  cn  = dpl->n;

    ProcessOneChanUpdate(pfr, cn, ref, NULL, NULL);
    /* Call upstream */
    PzframeDataCallCBs(pfr, PZFRAME_REASON_PARAM, cn);
}


//////////////////////////////////////////////////////////////////////

void
PzframeDataFillDscr(pzframe_type_dscr_t *ftd, const char *type_name,
                    int behaviour,
                    pzframe_chan_dscr_t *chan_dscrs, int chan_count)
{
    bzero(ftd, sizeof(*ftd));  

    ftd->type_name        = type_name;
    ftd->behaviour        = behaviour;
    ftd->chan_dscrs       = chan_dscrs;
    ftd->chan_count       = chan_count;
}

void PzframeDataInit      (pzframe_data_t *pfr, pzframe_type_dscr_t *ftd)
{
    bzero(pfr, sizeof(*pfr));
    pfr->ftd = ftd;
    pfr->cid = CDA_CONTEXT_ERROR;
}

int  PzframeDataRealize   (pzframe_data_t *pfr,
                           cda_context_t   present_cid,
                           const char     *base)
{
  pzframe_type_dscr_t *ftd = pfr->ftd;

  void                 *buf;
  uint8                *buf_p;
  size_t                r_total; // "r" stands for "room"
  size_t                r_refs;
  size_t                r_dpls;
  size_t                r_data;
  size_t                r_vals;
  size_t                valsize;
  size_t                pddsize; // PaDdeD valsize

  int                   cn;
  int                   is_marker;
  int                   cda_options;
  cxdtype_t             dtype;
  int                   max_nelems;

  int                   evmask;
  cda_dataref_evproc_t  evproc;
  void                 *privptr2;

    /* 1. Allocate buffers */
    if ((ftd->behaviour & PZFRAME_B_NO_ALLOC) == 0)
    {
        r_refs = (sizeof(*(pfr->refs))     * ftd->chan_count + 15) &~15UL;
        r_dpls = (sizeof(*(pfr->dpls))     * ftd->chan_count + 15) &~15UL;
        r_data = (sizeof(*(pfr->cur_data)) * ftd->chan_count + 15) &~15UL;

        for (cn = 0,  r_vals = 0;
             cn < ftd->chan_count;
             cn++)
            if (ftd->chan_dscrs[cn].name != NULL)
            {
                /* Note: (***) this code should be kept in sync
                   with below */
                dtype      = ftd->chan_dscrs[cn].dtype;
                max_nelems = ftd->chan_dscrs[cn].max_nelems;
                if (dtype      == 0) dtype      = CXDTYPE_INT32;
                if (max_nelems == 0) max_nelems = 1;

                valsize = sizeof_cxdtype(dtype) * max_nelems;
                if (valsize > sizeof(pfr->cur_data[cn].valbuf))
                {
                    pddsize = (valsize + 15) &~15UL;
                    r_vals += pddsize;
                }
                /* (***) */
            }

        r_total = r_refs + r_dpls + r_data*2 + r_vals;
        if ((buf = malloc(r_total)) == NULL) return -1;
        bzero(buf, r_total);
        buf_p = pfr->buf = buf;

        pfr->refs     = buf_p;  buf_p += r_refs;
        pfr->dpls     = buf_p;  buf_p += r_dpls;
        pfr->cur_data = buf_p;  buf_p += r_data;
        pfr->prv_data = buf_p;  buf_p += r_data;

        for (cn = 0;  cn < ftd->chan_count;  cn++)
        {
            pfr->dpls[cn].p = pfr;
            pfr->dpls[cn].n = cn;
        }
    }
////fprintf(stderr, "pfr=%p ftd=%p dpls=%p\n", pfr, ftd, pfr->dpls);

    if ((pfr->ftd->behaviour & PZFRAME_B_ARTIFICIAL) != 0) return 0;

    if (present_cid != CDA_CONTEXT_ERROR)
        pfr->cid = present_cid;
    else
    {
        pfr->cid = cda_new_context(0/*!!!uniq*/, NULL,
                                   NULL, 0,
                                   NULL/*!!!argv0*/,
                                   0, NULL, NULL);
        if (pfr->cid < 0)
        {
            fprintf(stderr, "%s %s() cda_new_context(): %s",
                    strcurtime(), __FUNCTION__, cda_last_err());
            goto ERREXIT;
        }
    }

    for (cn = 0;  cn < ftd->chan_count;  cn++)
        if (ftd->chan_dscrs[cn].name != NULL)
        {
            is_marker   = (ftd->chan_dscrs[cn].chan_type & PZFRAME_CHAN_MARKER_MASK) != 0;
            cda_options = CDA_DATAREF_OPT_PRIVATE | CDA_DATAREF_OPT_ON_UPDATE;
            if ((ftd->chan_dscrs[cn].chan_type & PZFRAME_CHAN_TYPE_MASK) == PZFRAME_CHAN_IS_FRAME
                ||
                (ftd->chan_dscrs[cn].chan_type & PZFRAME_CHAN_NO_RD_CONV_MASK) != 0)
                cda_options |= CDA_DATAREF_OPT_NO_RD_CONV;
            /* Note: (***) this code should be kept in sync
               with above */
            dtype      = ftd->chan_dscrs[cn].dtype;
            max_nelems = ftd->chan_dscrs[cn].max_nelems;
            if (dtype      == 0) dtype      = CXDTYPE_INT32;
            if (max_nelems == 0) max_nelems = 1;

            valsize = sizeof_cxdtype(dtype) * max_nelems;
            if (valsize > sizeof(pfr->cur_data[cn].valbuf))
            {
                pddsize = (valsize + 15) &~15UL;
                pfr->cur_data[cn].current_val = buf_p;  buf_p += pddsize;
            }
            /* (***) */

            if      (is_marker)
            {
                evmask   = CDA_REF_EVMASK_UPDATE | CDA_REF_EVMASK_RSLVSTAT;
                evproc   = PzframeDataEvproc;
                privptr2 = pfr;
            }
            else if ((ftd->chan_dscrs[cn].chan_type & PZFRAME_CHAN_IMMEDIATE_MASK) != 0)
            {
                evmask   = CDA_REF_EVMASK_UPDATE;
                evproc   = PzframeChanEvproc;
                privptr2 = pfr->dpls + cn;

                if ((ftd->chan_dscrs[cn].chan_type & PZFRAME_CHAN_ON_CYCLE_MASK) != 0)
                    cda_options &=~ CDA_DATAREF_OPT_ON_UPDATE;
            }
            else
            {
                evmask   = CDA_REF_EVMASK_RDSCHG;
                evproc   = PzframeRDsCEvproc;
                privptr2 = pfr->dpls + cn;
            }

            pfr->refs[cn] = cda_add_chan(pfr->cid,
                                         base,
                                         ftd->chan_dscrs[cn].name,
                                         cda_options,
                                         dtype, max_nelems,
                                         evmask, evproc, privptr2);
            if (pfr->refs[cn] == CDA_DATAREF_ERROR)
                fprintf(stderr, "%s %s(): cda_add_chan(\"%s\"): %s\n",
                        strcurtime(), __FUNCTION__,
                        ftd->chan_dscrs[cn].name, cda_last_err());
        }
        else
            pfr->refs[cn] = CDA_DATAREF_ERROR;

    return 0;

 ERREXIT:

    if (pfr->cid != present_cid) cda_del_context(pfr->cid);
    pfr->cid = CDA_CONTEXT_ERROR;

    safe_free(pfr->buf);
    pfr->buf = NULL;

    return -1;
}

int  PzframeDataAddEvproc (pzframe_data_t *pfr,
                           pzframe_data_evproc_t cb, void *privptr)
{
  int                    id;
  pzframe_data_cbinfo_t *slot;

    if (cb == NULL) return 0;

    id = GetCblistSlot(pfr);
    if (id < 0) return -1;
    slot = AccessCblistSlot(id, pfr);

    slot->cb      = cb;
    slot->privptr = privptr;

    return 0;
}

void PzframeDataSetRunMode(pzframe_data_t *pfr,
                           int running, int oneshot)
{
    /* Default to current values */
    if (running < 0) running = pfr->cfg.running;
    if (oneshot < 0) oneshot = pfr->cfg.oneshot;

    /* Sanitize */
    running = running != 0;
    oneshot = oneshot != 0;

    /* Had anything changed? */
    if (running == pfr->cfg.running  &&
        oneshot == pfr->cfg.oneshot)
        return;

    /* Store */
    pfr->cfg.running = running;
    pfr->cfg.oneshot = oneshot;

    /* Reflect state */
    if (pfr->cid == CDA_CONTEXT_ERROR) return;
    if (running  ||  oneshot)
        /*!!!cda_run_server(pfr->bigc_sid)*/;
    else
        /*!!!cda_hlt_server(pfr->bigc_sid)*/;
}

int  PzframeDataGetChanInt(pzframe_data_t *pfr, int cn, int    *val_p)
{
    if (cn < 0  ||  cn >= pfr->ftd->chan_count)
    {
        errno = EINVAL;
        goto ERR;
    }
    if (pfr->ftd->chan_dscrs[cn].max_nelems != 1  &&
        pfr->ftd->chan_dscrs[cn].max_nelems != 0 /*==0 means 1*/)
    {
        errno = EDOM;
        goto ERR;
    }

    switch (pfr->ftd->chan_dscrs[cn].dtype)
    {
        case CXDTYPE_INT8:   *val_p = pfr->cur_data[cn].valbuf.i8;  break;
        case CXDTYPE_INT16:  *val_p = pfr->cur_data[cn].valbuf.i16; break;
        case 0: /* 0 means INT32 */
        case CXDTYPE_INT32:  *val_p = pfr->cur_data[cn].valbuf.i32; break;
        case CXDTYPE_UINT8:  *val_p = pfr->cur_data[cn].valbuf.u8;  break;
        case CXDTYPE_UINT16: *val_p = pfr->cur_data[cn].valbuf.u16; break;
        case CXDTYPE_UINT32: *val_p = pfr->cur_data[cn].valbuf.u32; break;
#if MAY_USE_INT64  ||  1
        case CXDTYPE_INT64:  *val_p = pfr->cur_data[cn].valbuf.i64; break;
        case CXDTYPE_UINT64: *val_p = pfr->cur_data[cn].valbuf.u64; break;
#endif /* MAY_USE_INT64 */
        case CXDTYPE_SINGLE: *val_p = pfr->cur_data[cn].valbuf.f32; break;
        case CXDTYPE_DOUBLE: *val_p = pfr->cur_data[cn].valbuf.f64; break;
        default:
            errno = ERANGE;
            goto ERR;
    }

    return 0;

 ERR:
    *val_p = 0;
    return -1;
}

int  PzframeDataGetChanDbl(pzframe_data_t *pfr, int cn, double *val_p)
{
    if (cn < 0  ||  cn >= pfr->ftd->chan_count)
    {
        errno = EINVAL;
        goto ERR;
    }
    if (pfr->ftd->chan_dscrs[cn].max_nelems != 1  &&
        pfr->ftd->chan_dscrs[cn].max_nelems != 0 /*==0 means 1*/)
    {
        errno = EDOM;
        goto ERR;
    }

    switch (pfr->ftd->chan_dscrs[cn].dtype)
    {
        case CXDTYPE_INT8:   *val_p = pfr->cur_data[cn].valbuf.i8;  break;
        case CXDTYPE_INT16:  *val_p = pfr->cur_data[cn].valbuf.i16; break;
        case 0: /* 0 means INT32 */
        case CXDTYPE_INT32:  *val_p = pfr->cur_data[cn].valbuf.i32; break;
        case CXDTYPE_UINT8:  *val_p = pfr->cur_data[cn].valbuf.u8;  break;
        case CXDTYPE_UINT16: *val_p = pfr->cur_data[cn].valbuf.u16; break;
        case CXDTYPE_UINT32: *val_p = pfr->cur_data[cn].valbuf.u32; break;
#if MAY_USE_INT64  ||  1
        case CXDTYPE_INT64:  *val_p = pfr->cur_data[cn].valbuf.i64; break;
        case CXDTYPE_UINT64: *val_p = pfr->cur_data[cn].valbuf.u64; break;
#endif /* MAY_USE_INT64 */
        case CXDTYPE_SINGLE: *val_p = pfr->cur_data[cn].valbuf.f32; break;
        case CXDTYPE_DOUBLE: *val_p = pfr->cur_data[cn].valbuf.f64; break;
        default:
            errno = ERANGE;
            goto ERR;
    }

    return 0;

 ERR:
    *val_p = NAN;
    return -1;
}


int  PzframeDataFdlgFilter(const char *filespec,
                           time_t *cr_time,
                           char *commentbuf, size_t commentbuf_size,
                           void *privptr)
{
  pzframe_data_t      *pfr = privptr;

    if (pfr->ftd->vmt.filter != NULL  &&
        pfr->ftd->vmt.filter(pfr, filespec,
                             cr_time,
                             commentbuf, commentbuf_size) == 0)
        return 0;
    else
        return 1;
}
