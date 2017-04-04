#include "timeval_utils.h"

#include "cxsd_driver.h"
#include "vdev.h"


void vdev_forget_all(vdev_context_t *ctx)
{
  int  sodc;

    for (sodc = 0;  sodc < ctx->num_sodcs;  sodc++)
    {
        ctx->cur[sodc].rcvd = 0;
        ctx->cur[sodc].ts   = (cx_time_t){CX_TIME_SEC_NEVER_READ,0};
    }
}

//////////////////////////////////////////////////////////////////////

static int AllImportantAreReady(vdev_context_t *ctx)
{
  int              sodc;

    for (sodc = 0;  sodc < ctx->num_sodcs;  sodc++)
        if (ctx->map[sodc].mode & VDEV_IMPR  &&
            ctx->cur[sodc].rcvd == 0)
            return 0;

    return 1;
}

static void sodc_evproc(int            devid,
                        void          *privptr1,
                        cda_dataref_t  ref,
                        int            reason,
                        void          *info_ptr,
                        void          *privptr2)
{
  vdev_context_t *ctx  = privptr1;
  int             sodc = ptr2lint(privptr2);
  int             mode = ctx->map[sodc].mode;

  struct timeval    now;
  vdev_state_dsc_t *curs;
  vdev_state_dsc_t *nxts;

    if      (reason == CDA_REF_R_UPDATE)
    {
        /* Get data */
        cda_get_ref_data(ref, 0, sizeof(ctx->cur[sodc].v.i32), &(ctx->cur[sodc].v.i32));
        cda_get_ref_stat(ref, &(ctx->cur[sodc].flgs), &(ctx->cur[sodc].ts));
        ctx->cur[sodc].rcvd = 1;

        /* Perform tubbing */
        if (mode & VDEV_TUBE) /*!!! Should obey dtype and nelems!!! */
            ReturnInt32DatumTimed(devid, ctx->map[sodc].ourc,
                                  vdev_get_int(ctx, sodc),
                                  ctx->cur[sodc].flgs, ctx->cur[sodc].ts);
        
        /* Check if we are ready to operate */
        if (ctx->cur_state == ctx->state_unknown_val  &&
            ctx->state_determine_val >= 0             &&
            mode & VDEV_IMPR                          &&
            AllImportantAreReady(ctx))
            vdev_set_state(ctx, ctx->state_determine_val);
        
        /* Call the client */
        if (ctx->sodc_cb != NULL)
            ctx->sodc_cb(ctx->devid, ctx->devptr, sodc, vdev_get_int(ctx, sodc));

        /* If not all info is gathered yet, then there's nothing else to do yet */
        if (ctx->cur_state == ctx->state_unknown_val) return;

        /* No messing with state_descr[] if no states */
        if (ctx->state_count <= 0)                    return;

        /* Perform state transitions depending on current state */
        curs = ctx->state_descr + ctx->cur_state;
        if (/* There is a state to switch to */
            curs->next_state >= 0
            &&
            /* ...and no delay or delay has expired */
            (curs->delay_us <= 0  ||
             (gettimeofday(&(now), NULL),
              TV_IS_AFTER(now, ctx->next_state_time))))
        {
            nxts = ctx->state_descr + curs->next_state;
            if (/* This channel IS "interesting"... */
                ctx->state_important_channels != NULL    &&
                ctx->state_important_channels[(curs->next_state * ctx->num_sodcs) + sodc]  &&
                /* ...and condition is met */
                nxts->sw_alwd != NULL  &&
                nxts->sw_alwd(ctx->devptr, ctx->cur_state))
            {
                vdev_set_state(ctx, curs->next_state);
            }
        }
    }
    else if (reason == CDA_REF_R_STATCHG)
    {
    }
    else if (reason == CDA_REF_R_RSLVSTAT)
    {
    }
}

static int CalcSubordDevState(vdev_context_t *ctx)
{
  int             tdev;
  int             off_count;
  int             nrd_count;

    off_count = nrd_count = 0;
    for (tdev = 0;  tdev < ctx->devstate_count;  tdev++)
    {
        if (ctx->devstate_cur[tdev].v.i32 == DEVSTATE_OFFLINE)  off_count++;
        if (ctx->devstate_cur[tdev].v.i32 == DEVSTATE_NOTREADY) nrd_count++;
    }

    if      (off_count > 0) return DEVSTATE_OFFLINE;
    else if (nrd_count > 0) return DEVSTATE_NOTREADY;
    else                    return DEVSTATE_OPERATING;
}

static int SelectOurDevState(int subord_state)
{
    if (subord_state == DEVSTATE_OFFLINE) return DEVSTATE_NOTREADY;
    else                                  return subord_state;
}

static void stat_evproc(int            devid,
                        void          *privptr1,
                        cda_dataref_t  ref,
                        int            reason,
                        void          *info_ptr,
                        void          *privptr2)
{
  vdev_context_t *ctx  = privptr1;
  int             tdev = ptr2lint(privptr2);
  int             sodc;
  int32           prev_val;
  int             subord_state;
  int             our_state;

    prev_val = ctx->devstate_cur[tdev].v.i32;
    cda_get_ref_data(ref, 0, sizeof(ctx->devstate_cur[tdev].v.i32), &(ctx->devstate_cur[tdev].v.i32));
    ////DoDriverLog(ctx->devid, 0, "%s SUBORD old=%d ->%d", __FUNCTION__, prev_val, ctx->devstate_cur[tdev].val);
    if (ctx->devstate_cur[tdev].v.i32 == prev_val) return;

    if (ctx->devstate_cur[tdev].v.i32 != DEVSTATE_OPERATING)
        for (sodc = 0;  sodc < ctx->num_sodcs;  sodc++)
            if (ctx->map[sodc].subdev_n == tdev)
            {
                ctx->cur[sodc].rcvd = 0;
                ctx->cur[sodc].ts   = (cx_time_t){CX_TIME_SEC_NEVER_READ,0};
            }

    subord_state = CalcSubordDevState(ctx);
    our_state    = SelectOurDevState (subord_state);
    DoDriverLog(ctx->devid, DRIVERLOG_C_ENTRYPOINT,
                "%s subord=%d our=%d", __FUNCTION__, subord_state, our_state);
    SetDevState(devid, our_state, 0, NULL); // Here should try to make some description
////    if (our_state == DEVSTATE_OPERATING  &&
////        ctx->chan_state_n >= 0  &&  ctx->chan_state_n < our_numchans)
////        ReturnInt32Datum(ctx->devid, ctx->chan_state_n, our_state, 0);

    if      (subord_state == DEVSTATE_OFFLINE)
    {
        vdev_set_state(ctx, ctx->state_unknown_val);
    }
    else if (subord_state == DEVSTATE_NOTREADY)
    {
        if (ctx->cur_state != ctx->state_unknown_val  &&
            ctx->state_determine_val >= 0)
            vdev_set_state(ctx, ctx->state_determine_val);
    }
}

static void vdev_work_hbt(int devid, void *devptr,
                          sl_tid_t tid,
                          void *privptr)
{
  vdev_context_t    *ctx = (vdev_context_t *)privptr;
  
  struct timeval     now;
  vdev_state_dsc_t *curs = ctx->state_descr + ctx->cur_state;
  vdev_state_dsc_t *nxts;

    ctx->work_hbt_tid = -1;

    if (curs->next_state >= 0  &&
        curs->delay_us   >  0  &&
        (gettimeofday(&(now), NULL), TV_IS_AFTER(now, ctx->next_state_time)))
    {
        nxts = ctx->state_descr + curs->next_state;
        if (nxts->sw_alwd == NULL  ||
            nxts->sw_alwd(devptr, ctx->cur_state))
            vdev_set_state(ctx, curs->next_state);
    }

    ctx->work_hbt_tid = sl_enq_tout_after(ctx->devid, ctx->devptr, ctx->work_hbt_period, vdev_work_hbt, ctx);
}

int  vdev_init(vdev_context_t *ctx,
               int devid, void *devptr,
               int work_hbt_period,
               const char *base)
{
  int         sodc;
  int         tdev;

  const char *base_to_use;
  const char *name_to_use;

    ctx->devid           = devid;
    ctx->devptr          = devptr;
    ctx->work_hbt_period = work_hbt_period;
    bzero(ctx->cur, sizeof(*(ctx->cur)) * ctx->num_sodcs);

    if ((ctx->cid = cda_new_context(devid, ctx,
                                    "insrv::", CDA_CONTEXT_OPT_NO_OTHEROP,
                                    NULL,
                                    0, NULL, 0)) <= 0)
    {
        DoDriverLog(devid, 0, "cda_new_context(): %s", cda_last_err());
        return -CXRF_DRV_PROBL;
    }

    for (sodc = 0;  sodc < ctx->num_sodcs;  sodc++)
    {
        base_to_use = base;
        name_to_use = ctx->map[sodc].name;
        if      (name_to_use == NULL) /*!!!???*/;
        else if (name_to_use[0] == '\0')
        {
            base_to_use = NULL;
            name_to_use = base;
        }
        if ((ctx->cur[sodc].ref = cda_add_chan(ctx->cid, base_to_use,
                                               name_to_use,
                                               CDA_DATAREF_OPT_NO_RD_CONV |
                                                   CDA_DATAREF_OPT_ON_UPDATE,
                                               CXDTYPE_INT32, 1,
                                               CDA_REF_EVMASK_UPDATE       |
                                                   CDA_REF_EVMASK_STATCHG  |
                                                   CDA_REF_EVMASK_RSLVSTAT,
                                               sodc_evproc, lint2ptr(sodc))) < 0)
        {
            DoDriverLog(devid, 0, "cda_new_chan([%d]=\"%s\"): %s",
                        sodc, ctx->map[sodc].name, cda_last_err());
            cda_del_context(ctx->cid);    ctx->cid = -1;
            return -CXRF_DRV_PROBL;
        }
        ctx->cur[sodc].ts = (cx_time_t){CX_TIME_SEC_NEVER_READ,0};
    }
    for (tdev = 0;  tdev < ctx->devstate_count;  tdev++)
    {
        if ((ctx->devstate_cur[tdev].ref =
             cda_add_chan(ctx->cid, base,
                          ctx->devstate_names[tdev],
                          CDA_DATAREF_OPT_NO_RD_CONV |
                              CDA_DATAREF_OPT_ON_UPDATE,
                          CXDTYPE_INT32, 1,
                          CDA_REF_EVMASK_UPDATE,
                          stat_evproc, lint2ptr(tdev))) < 0)
        {
            DoDriverLog(devid, 0, "cda_new_chan(devstate[%d]=\"%s\"): %s",
                        tdev, ctx->devstate_names[tdev], cda_last_err());
            cda_del_context(ctx->cid);    ctx->cid = -1;
            return -CXRF_DRV_PROBL;
        }
        /*!!! cda_get_ref_data() for insrv:: channel to have current value?  */
    }

    if (ctx->chan_state_n >= 0  &&  ctx->chan_state_n < ctx->our_numchans)
        SetChanReturnType(ctx->devid, ctx->chan_state_n, 1, IS_AUTOUPDATED_TRUSTED);

    if (work_hbt_period > 0  &&  ctx->state_count > 0)
        vdev_work_hbt(devid, devptr, -1, ctx);

    return SelectOurDevState(CalcSubordDevState(ctx));
}

int  vdev_fini(vdev_context_t *ctx)
{
    /*!!!*/

    return 0;
}

void vdev_set_state(vdev_context_t *ctx, int nxt_state)
{
  int                 prev_state = ctx->cur_state;
  int32               val;
  struct timeval      timenow;
  vdev_sr_chan_dsc_t *sdp;

    if (nxt_state < 0  ||  nxt_state > ctx->state_count)
    {
        return;
    }

    if (nxt_state == ctx->cur_state) return;

    ctx->cur_state = nxt_state;
    val = ctx->cur_state;
    if (ctx->chan_state_n >= 0  &&  ctx->chan_state_n < ctx->our_numchans)
        ReturnInt32Datum(ctx->devid, ctx->chan_state_n, val, 0);
    DoDriverLog(ctx->devid, DRIVERLOG_C_ENTRYPOINT, "state:=%d", val);

    bzero(&(ctx->next_state_time), sizeof(ctx->next_state_time));
    if (ctx->state_descr[nxt_state].delay_us > 0)
    {
        gettimeofday(&timenow, NULL);
        timeval_add_usecs(&(ctx->next_state_time),
                          &timenow,
                          ctx->state_descr[nxt_state].delay_us);
    }

    if (ctx->state_descr[nxt_state].swch_to != NULL)
        ctx->state_descr[nxt_state].swch_to(ctx->devptr, prev_state);

    for (sdp = ctx->state_related_channels;
         sdp != NULL  &&  sdp->ourc >= 0  &&  ctx->do_rw != NULL;
         sdp++)
        ctx->do_rw(ctx->devid, ctx->devptr,
                   DRVA_READ,
                   1, &(sdp->ourc),
                   NULL, NULL, NULL);
}

int  vdev_get_int(vdev_context_t *ctx, int sodc)
{
    if (sodc < 0  ||  sodc >= ctx->num_sodcs)
    {
        errno = EINVAL;
        return 0;
    }
    if (ctx->map[sodc].max_nelems != 1  &&
        ctx->map[sodc].max_nelems != 0 /*==0 means 1*/)
    {
        errno = EDOM;
        return 0;
    }

    switch (ctx->map[sodc].dtype)
    {
        case 0: /* "0" dafaults to INT32 */
        case CXDTYPE_INT32:  return ctx->cur[sodc].v.i32;
        case CXDTYPE_UINT32: return ctx->cur[sodc].v.u32;
        case CXDTYPE_INT16:  return ctx->cur[sodc].v.i16;
        case CXDTYPE_UINT16: return ctx->cur[sodc].v.u16;
        case CXDTYPE_INT8:   return ctx->cur[sodc].v.i8;
        case CXDTYPE_UINT8:  return ctx->cur[sodc].v.u8;
#if MAY_USE_INT64  ||  1
        case CXDTYPE_INT64:  return ctx->cur[sodc].v.i64;
        case CXDTYPE_UINT64: return ctx->cur[sodc].v.u64;
#endif /* MAY_USE_INT64 */
        case CXDTYPE_SINGLE: return ctx->cur[sodc].v.f64;
        case CXDTYPE_DOUBLE: return ctx->cur[sodc].v.f32;
        default:
            errno = ERANGE;
            return 0;
    }
}
