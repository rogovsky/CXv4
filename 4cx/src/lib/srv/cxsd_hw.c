#define __CXSD_HW_C

#include "cxsd_lib_includes.h"


#ifndef MAY_USE_INT64
  #define MAY_USE_INT64 1
#endif
#ifndef MAY_USE_FLOAT
  #define MAY_USE_FLOAT 1
#endif


enum {INITIAL_TIMESTAMP_SECS = CX_TIME_SEC_NEVER_READ};  /* Set to was-measured (!=0), but long ago (1970-01-01_00:00:01) */
enum {INITIAL_CURRENT_CYCLE  = 1};  /* For all channels' initial upd_cycle (=0) to be !=current_cycle */

enum {_DEVSTATE_DESCRIPTION_MAX_NELEMS = 100};



static int             MustSimulateHardware = 0;
static int             MustCacheRFW         = 1;
static int             IsReqRofWrChsIng     = 0; // Is-(request-read-of-write-channels)'ing
static int             ReturningInternal    = 0;

static int             basecyclesize        = 1000000;
static struct timeval  cycle_start;  /* The time when the current cycle had started */
static struct timeval  cycle_end;
static sl_tid_t        cycle_tid            = -1;
static int             current_cycle        = INITIAL_CURRENT_CYCLE;
static int             cycle_pass_count     = -1;
static int             cycle_pass_count_lim = 10;

static cxsd_hw_cleanup_proc  cleanup_proc   = NULL;

//////////////////////////////////////////////////////////////////////

static inline int CheckGcid(cxsd_gchnid_t gcid)
{
    if (gcid > 0  &&  gcid < cxsd_hw_numchans) return 0;

    errno = EBADF;
    return -1;
}

//--------------------------------------------------------------------

// GetChanCbSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, ChanCb, cxsd_hw_chan_cbrec_t,
                                 cb, evmask, 0, 1,
                                 0, 2, 0,
                                 chn_p->, chn_p,
                                 cxsd_hw_chan_t *chn_p, cxsd_hw_chan_t *chn_p)

static void RlsChanCbSlot(int id, cxsd_hw_chan_t *chn_p)
{
  cxsd_hw_chan_cbrec_t *p = AccessChanCbSlot(id, chn_p);

    p->evmask = 0;
}

//////////////////////////////////////////////////////////////////////

typedef struct
{
    int                     in_use;
    int                     uniq;
    void                   *privptr1;
    cxsd_hw_cycle_evproc_t  evproc;
    void                   *privptr2;
} cycle_cbrec_t;

static cycle_cbrec_t *cyclecbs_list        = NULL;
static int            cyclecbs_list_allocd = 0;

// GetCycleCBSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, CycleCB, cycle_cbrec_t,
                                 cyclecbs, in_use, 0, 1,
                                 1, 100, 0,
                                 , , void)

static void RlsCycleCBSlot(int n)
{
  cycle_cbrec_t *p = AccessCycleCBSlot(n);

    p->in_use = 0;
}

//////////////////////////////////////////////////////////////////////

typedef struct
{
    cxsd_gchnid_t  gcid;
    int            reason;
    int            evmask;
} CxsdHwChanEvCallInfo_t;

static int chan_evproc_caller(cxsd_hw_chan_cbrec_t *p, void *privptr)
{
  CxsdHwChanEvCallInfo_t *info = privptr;

    if (p->evmask & info->evmask)
        p->evproc(p->uniq, p->privptr1,
                  info->gcid, info->reason, p->privptr2);
    return 0;
}
static int  CxsdHwCallChanEvprocs(cxsd_gchnid_t gcid,
                                  CxsdHwChanEvCallInfo_t *info)
{
  cxsd_hw_chan_t       *chn_p = cxsd_hw_channels + gcid;

    if (CheckGcid(gcid) != 0) return -1;

    info->gcid = gcid;
    ForeachChanCbSlot(chan_evproc_caller, info, chn_p);

    return 0;
}

//////////////////////////////////////////////////////////////////////

static void CxsdHwList(FILE *fp)
{
  int                lyr_n;
  int                devid;
  int                lio_n;

  cxsd_hw_lyr_t     *lyr_p;
  cxsd_hw_dev_t     *dev_p;
  const char        *lyrname;

    for (lyr_n = 0,  lyr_p = cxsd_hw_layers + lyr_n;
         lyr_n < cxsd_hw_numlyrs;
         lyr_n++,    lyr_p++)
    {
        fprintf(fp, "layer[%d] <%s>\n", lyr_n,
                CxsdDbGetStr(cxsd_hw_cur_db, lyr_p->lyrname_ofs));
    }

    for (lio_n = 0;  lio_n < cxsd_hw_cur_db->numlios;  lio_n++)
        fprintf(fp, "layerinfo[%d] <%s>:%d<%s>\n",
                lio_n,
                CxsdDbGetStr(cxsd_hw_cur_db, cxsd_hw_cur_db->liolist[lio_n].lyrname_ofs),
                cxsd_hw_cur_db->liolist[lio_n].bus_n,
                CxsdDbGetStr(cxsd_hw_cur_db, cxsd_hw_cur_db->liolist[lio_n].lyrinfo_ofs));

    for (devid = 0,  dev_p = cxsd_hw_devices + devid;
         devid < cxsd_hw_numdevs;
         devid++,    dev_p++)
    {
        fprintf(fp, "dev[%d] \"%s\" <%s>@%d:<%s>; [%d,+%d/%d) <%s>\n",
                devid, 
                CxsdDbGetStr(cxsd_hw_cur_db, dev_p->db_ref->instname_ofs),
                CxsdDbGetStr(cxsd_hw_cur_db, dev_p->db_ref->typename_ofs),
                dev_p->lyrid, 
                (lyrname = CxsdDbGetStr(cxsd_hw_cur_db,
                                        cxsd_hw_layers[-dev_p->lyrid].lyrname_ofs)) != NULL? lyrname : "",
                dev_p->first, dev_p->count, dev_p->wauxcount,
                CxsdDbGetStr(cxsd_hw_cur_db, dev_p->db_ref->auxinfo_ofs));
    }
}

static int chan_evproc_remover(cxsd_hw_chan_cbrec_t *p, void *privptr)
{
    /*!!! should call RlsChanCbSlot() somehow, but it requires "id" instead of "p" */
    p->evmask = 0;

    return 0;
}
int  CxsdHwSetDb   (CxsdDb db)
{
  int                lyr_n;
  int                devid;
  CxsdDbDevLine_t   *hw_d;

  int                stage;
  int                g;
  int                x;

  const char        *dev_lyrname;
  const char        *lyr_lyrname;

  cxsd_hw_lyr_t     *lyr_p;
  cxsd_hw_dev_t     *dev_p;
  CxsdChanInfoRec   *grp_p;
  cxsd_hw_chan_t    *chn_p;
  int                nchans;

#if CXSD_HW_ALLOC_ALL
  size_t             layers_bufsize;
  size_t             devices_bufsize;
  size_t             channels_bufsize;
#endif
  size_t             current_val_bufsize;
  size_t             next_wr_val_bufsize;
  size_t             usize;                // Size of data-units
  size_t             csize;                // Channel data size

    db->is_readonly = 1;
    cxsd_hw_cur_db  = db;

    /* II. Free old data */
    safe_free(cxsd_hw_current_val_buf); cxsd_hw_current_val_buf = NULL;
    safe_free(cxsd_hw_next_wr_val_buf); cxsd_hw_next_wr_val_buf = NULL;

    /* III. Clear info */
#if CXSD_HW_ALLOC_ALL
#else
    cxsd_hw_numlyrs  = 0;
    cxsd_hw_numdevs  = 0;
    cxsd_hw_numchans = 0;
    bzero(&cxsd_hw_layers,   sizeof(cxsd_hw_layers));
    bzero(&cxsd_hw_devices,  sizeof(cxsd_hw_devices));
    bzero(&cxsd_hw_channels, sizeof(cxsd_hw_channels));
    for (devid = 0;  devid < countof(cxsd_hw_devices);  devid++)
        cxsd_hw_devices[devid].state = DEVSTATE_OFFLINE;
#endif
    
    /* IV. Fill devices' and channels' properties according to now-current DB,
           adding devices one-by-one */

    /* Put aside a zero-id layer */
    cxsd_hw_numlyrs = 1;
    cxsd_hw_layers[0].lyrname_ofs = -1;
    
    /* On stage 0 we just count sizes, and
       fill properties on stage 1 */
    for (stage = 0;  stage <= 1;  stage++)
    {
        current_val_bufsize = 0;
        next_wr_val_bufsize = 0;

        /* Enumerate devices */
        for (devid = 0, hw_d = cxsd_hw_cur_db->devlist, nchans = 0;
             devid < cxsd_hw_cur_db->numdevs;
             devid++,   hw_d++)
        {
            dev_p = cxsd_hw_devices + devid;

            dev_p->db_ref = hw_d;

            /* Remember 1st (0th!) channel number */
            dev_p->first = nchans;
            
            /* Go through channel groups */
            for (g = 0,  grp_p = hw_d->changroups;
                 g < hw_d->changrpcount;
                 g++,    grp_p++)
            {
                usize = sizeof_cxdtype(grp_p->dtype);
                csize = usize * grp_p->max_nelems;
                /* Perform padding for alignment, if required */
                if (csize > 0  &&
                    /* Is it a power of 2?  I.e., does alignment have sense? */
                    (/* NO == 1 */    usize == 2   ||  
                     usize == 4   ||  usize == 8   ||  
                     usize == 16  ||  usize == 32  ||  
                     usize == 64  ||  usize == 128))
                {
                    // a. current_val
                    current_val_bufsize     = (current_val_bufsize + usize-1)
                                              & (~(usize - 1));
                    // b. next_wr_val only if rw
                    if (grp_p->rw)
                        next_wr_val_bufsize = (next_wr_val_bufsize + usize-1)
                                              & (~(usize - 1));
                }
////if (stage) fprintf(stderr, "\tdev#%d.g#%d: usize=%zd,csize=%zd v=%zd w=%zd\n", devid, g, usize, csize, current_val_bufsize, next_wr_val_bufsize);

                /* Iterate individual channels */
                for (x = 0,  chn_p = cxsd_hw_channels + nchans;
                     x < grp_p->count;
                     x++,    chn_p++,  nchans++)
                {
                    chn_p->rw             = grp_p->rw;
                    chn_p->devid          = devid;
                    chn_p->boss           = -1; /*!!!*/
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
                    chn_p->dtype          = chn_p->current_dtype = grp_p->dtype;
                    chn_p->max_nelems     = grp_p->max_nelems;
                    chn_p->usize          = chn_p->current_usize = usize;
#else
                    chn_p->dtype          = grp_p->dtype;
                    chn_p->max_nelems     = grp_p->max_nelems;
                    chn_p->usize          = usize;
#endif
                    chn_p->timestamp.sec  = INITIAL_TIMESTAMP_SECS;
                    chn_p->timestamp.nsec = 0;
                    if (chn_p->rw == 0)
                        chn_p->fresh_age  = (cx_time_t){5,0}; /*!!!*/
                    else
                        chn_p->fresh_age  = (cx_time_t){0,0}; /* No-fresh-aging */

                    chn_p->q_dtype        = chn_p->dtype;

                    if (stage)
                    {
                        chn_p->current_val = cxsd_hw_current_val_buf + current_val_bufsize;
                        if (chn_p->max_nelems == 1) /* Pre-set 1 for scalar channels */
                            chn_p->current_nelems = 1;
                        if (grp_p->rw)
                            chn_p->next_wr_val = cxsd_hw_next_wr_val_buf + next_wr_val_bufsize;
                    }

                    current_val_bufsize += csize;
                    if (grp_p->rw) next_wr_val_bufsize += csize;
                }
            }

            /* Fill in # of channels */
            dev_p->count = nchans - dev_p->first;

            /*!!! _devstate: For now, simply duplicate that code */
            /* 1. _devstate */
            chn_p = cxsd_hw_channels + nchans + CXSD_DB_CHAN_DEVSTATE_OFS;
            chn_p->rw             = 0;
            chn_p->is_autoupdated = 1;
            chn_p->is_internal    = 1;
            chn_p->devid          = devid;
            chn_p->boss           = -1; /*!!!*/
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
            chn_p->dtype          = chn_p->current_dtype = CXDTYPE_INT32;
            chn_p->max_nelems     = 1;
            chn_p->usize          = chn_p->current_usize = sizeof_cxdtype(chn_p->dtype);
#else
            chn_p->dtype          = CXDTYPE_INT32;
            chn_p->max_nelems     = 1;
            chn_p->usize          = sizeof_cxdtype(chn_p->dtype);
#endif
            chn_p->timestamp.sec  = INITIAL_TIMESTAMP_SECS;
            chn_p->timestamp.nsec = 0;
            chn_p->fresh_age      = (cx_time_t){0,0}; /*Infinite*/
            /* ...alignment */
            current_val_bufsize     = (current_val_bufsize + chn_p->usize-1)
                                      & (~(chn_p->usize - 1));
            if (stage)
            {
                chn_p->current_val = cxsd_hw_current_val_buf + current_val_bufsize;
                chn_p->current_nelems = 1;
            }
            current_val_bufsize += chn_p->usize * chn_p->max_nelems;
            /* 2. _devstate_description */
            chn_p = cxsd_hw_channels + nchans + CXSD_DB_CHAN_DEVSTATE_DESCRIPTION_OFS;
            chn_p->rw             = 0;
            chn_p->is_autoupdated = 1;
            chn_p->is_internal    = 1;
            chn_p->devid          = devid;
            chn_p->boss           = -1; /*!!!*/
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
            chn_p->dtype          = chn_p->current_dtype = CXDTYPE_TEXT;
            chn_p->max_nelems     = _DEVSTATE_DESCRIPTION_MAX_NELEMS;
            chn_p->usize          = chn_p->current_usize = sizeof_cxdtype(chn_p->dtype);
#else
            chn_p->dtype          = CXDTYPE_TEXT;
            chn_p->max_nelems     = _DEVSTATE_DESCRIPTION_MAX_NELEMS;
            chn_p->usize          = sizeof_cxdtype(chn_p->dtype);
#endif
            chn_p->timestamp.sec  = INITIAL_TIMESTAMP_SECS;
            chn_p->timestamp.nsec = 0;
            chn_p->fresh_age      = (cx_time_t){0,0}; /*Infinite*/
            if (stage)
            {
                chn_p->current_val = cxsd_hw_current_val_buf + current_val_bufsize;
                chn_p->current_nelems = 0;
            }
            current_val_bufsize += chn_p->usize * chn_p->max_nelems;
            /* count them */
            nchans += CXSD_DB_AUX_CHANCOUNT;
            dev_p->wauxcount = dev_p->count + CXSD_DB_AUX_CHANCOUNT;

            /* On stage 1 perform layer allocation if required */
            if (stage  &&
                (dev_lyrname = CxsdDbGetStr(cxsd_hw_cur_db, hw_d->lyrname_ofs)) != NULL)
            {
                for (lyr_n = 1,  lyr_p = cxsd_hw_layers + 1;
                     lyr_n < cxsd_hw_numlyrs;
                     lyr_n++,    lyr_p++)
                    if ((lyr_lyrname = CxsdDbGetStr(cxsd_hw_cur_db, lyr_p->lyrname_ofs)) != NULL  &&
                        strcasecmp(dev_lyrname, lyr_lyrname) == 0) break;

                /* Didn't we find it? */
                if (lyr_n == cxsd_hw_numlyrs) /*!!! Check for overflow! */
                {
                    /* Allocate a layer... */
                    cxsd_hw_numlyrs++;
                    lyr_p->lyrname_ofs = hw_d->lyrname_ofs;
                }

                dev_p->lyrid = -lyr_n;
            }
        }

        /* Allocate buffers */
        if (stage == 0)
        {
            if (current_val_bufsize > 0)
            {
                if ((cxsd_hw_current_val_buf = malloc(current_val_bufsize))
                    == NULL) goto CLEANUP;
                bzero(cxsd_hw_current_val_buf, current_val_bufsize);
            }
            if (next_wr_val_bufsize > 0  &&
                (cxsd_hw_next_wr_val_buf = malloc(next_wr_val_bufsize)) == NULL)
                goto CLEANUP;
        }
    }
    cxsd_hw_numdevs  = devid;
    cxsd_hw_numchans = cxsd_hw_cur_db->numchans = nchans;

    if (1) CxsdHwList(stderr);
    
    return 0;

CLEANUP:
    return -1;
}


static void InitDevice (int devid);

static int  CheckDevice(int devid);

static void SetDevRflags    (int devid, rflags_t rflags_to_set);
static void RstDevRflags    (int devid);
static void RstDevTimestamps(int devid);
static void RstDevUpdCycles (int devid);

static void TerminDev(int devid, rflags_t rflags_to_set, const char *description);
static void FreezeDev(int devid, rflags_t rflags_to_set, const char *description);
static void ReviveDev(int devid);


int  CxsdHwActivate(const char *argv0)
{
  int                lyr_n;
  int                devid;
  cxsd_hw_lyr_t     *lyr_p;
  cxsd_hw_dev_t     *dev_p;
  int                s_devid;

    /* Load layers first */
    for (lyr_n = 1,  lyr_p = cxsd_hw_layers + 1;
         lyr_n < cxsd_hw_numlyrs;
         lyr_n++,    lyr_p++)
    {
        if (CxsdLoadLayer(argv0,
                          CxsdDbGetStr(cxsd_hw_cur_db, lyr_p->lyrname_ofs),
                          &(lyr_p->metric)) == 0)
        {
            lyr_p->logmask = cxsd_hw_defdrvlog_mask; /*!!! Should use what specified in [:OPTIONS] */
            ENTER_DRIVER_S(-lyr_n, s_devid);
            if (MustSimulateHardware             ||
                lyr_p->metric->init_lyr == NULL  ||
                lyr_p->metric->init_lyr(-lyr_n) == 0)
            {
                lyr_p->active = 1;
            }
            LEAVE_DRIVER_S(s_devid);
        }
    }

    /* And than load drivers */
    for (devid = 1,  dev_p = cxsd_hw_devices + 1;
         devid < cxsd_hw_numdevs;
         devid++,    dev_p++)
    {
        if (MustSimulateHardware
            ||
            (
             (dev_p->lyrid == 0  ||  cxsd_hw_layers[-dev_p->lyrid].active)
             &&
             CxsdLoadDriver(argv0,
                            CxsdDbGetStr(cxsd_hw_cur_db, dev_p->db_ref->typename_ofs),
                            CxsdDbGetStr(cxsd_hw_cur_db, dev_p->db_ref->drvname_ofs),
                            &(dev_p->metric)) == 0
             &&
             CheckDevice(devid) == 0
            )
           )
        {
            InitDevice (devid);
        }
        else
            TerminDev  (devid, CXRF_OFFLINE | CXRF_NO_DRV, "load/init problem");
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////

static int  CheckDevice(int devid)
{
  cxsd_hw_dev_t    *dev_p;
  cxsd_hw_lyr_t    *lyr_p;
  CxsdDbDevLine_t  *db_ref;
  CxsdDriverModRec *metric;
  int               lyr_rq;

    dev_p  = cxsd_hw_devices + devid;
    db_ref = dev_p->db_ref;
    metric = dev_p->metric;
    lyr_rq = metric->layer != NULL  &&  metric->layer[0] != '\0';

    /* 1. If there's a layer -- check version compatibility. */
    if      ( lyr_rq  &&  dev_p->lyrid == 0)
    {
        logline(LOGF_MODULES, 0,
                "%s[%d] requires a layer of type \"%s\", but none specified",
                CxsdDbGetStr(cxsd_hw_cur_db, dev_p->db_ref->typename_ofs),
                devid,
                metric->layer);
                
        return -1;
    }
    else if (!lyr_rq  &&  dev_p->lyrid != 0)
    {
        logline(LOGF_MODULES, 0,
                "%s[%d] doesn't require a layer, but \"%s\" is specified",
                CxsdDbGetStr(cxsd_hw_cur_db, dev_p->db_ref->typename_ofs),
                devid,
                CxsdDbGetStr(cxsd_hw_cur_db,
                             cxsd_hw_layers[-dev_p->lyrid].lyrname_ofs));
                
        return -1;
    }
    else if ( lyr_rq  &&  dev_p->lyrid != 0)
    {
        lyr_p = cxsd_hw_layers + -dev_p->lyrid;
        if (lyr_p->metric->api_name == NULL  ||
            strcasecmp(lyr_p->metric->api_name, metric->layer) != 0)
        {
            logline(LOGF_MODULES, 0,
                    "%s[%d] requires a layer of type \"%s\", but specified \"%s\" is of type \"%s\"",
                    CxsdDbGetStr(cxsd_hw_cur_db, dev_p->db_ref->typename_ofs),
                    devid,
                    metric->layer,
                    CxsdDbGetStr(cxsd_hw_cur_db, lyr_p->lyrname_ofs),
                    lyr_p->metric->api_name);
            
            return -1;
        }
        if (!CX_VERSION_IS_COMPATIBLE(metric->layerver, lyr_p->metric->api_version))
        {
            logline(LOGF_MODULES, 0,
                    "%s[%d] requires a layer of type \"%s\".version=%d, incompatible with specified \"%s\".version=%d",
                    CxsdDbGetStr(cxsd_hw_cur_db, dev_p->db_ref->typename_ofs),
                    devid,
                    metric->layer,  metric->layerver,
                    CxsdDbGetStr(cxsd_hw_cur_db, lyr_p->lyrname_ofs),
                    lyr_p->metric->api_version);
            
            return -1;
        }
    }
    /* else=(!lyr_rq  &&  dev_p->lyrid == 0) -- do nothing */

    /* 2. Check conformance of channels */
    /*!!! Note: should compare r/w+type+nelems CHANNEL-BY-CHANNEL, not by groups */
    
    return 0;
}

static void InitDevice (int devid)
{
  cxsd_hw_dev_t    *dev;
  CxsdDbDevLine_t  *db_ref;
  CxsdDriverModRec *metric;
  const char       *auxinfo;
  int               state;
  int               s_devid;
  
    /*!!! Check validity of devid!*/

    dev     = cxsd_hw_devices + devid;
    db_ref  = dev->db_ref;
    metric  = dev->metric;
    auxinfo = CxsdDbGetStr(cxsd_hw_cur_db, db_ref->auxinfo_ofs);
    
    dev->logmask = cxsd_hw_defdrvlog_mask; /*!!! Should use what specified in [:OPTIONS] */

    RstDevTimestamps(devid);

    if (MustSimulateHardware)
        ReviveDev(devid);
    else
    {
        if (db_ref->businfocount < metric->min_businfo_n  ||
            db_ref->businfocount > metric->max_businfo_n)
        {
            DoDriverLog(devid, DRIVERLOG_ERR, "businfocount=%d, out_of[%d,%d]",
                        db_ref->businfocount,
                        metric->min_businfo_n, metric->max_businfo_n);
            /*!!! Bark -- how? */
            TerminDev(devid, CXRF_CFG_PROBL, "businfocount out of range");
            return;
        }

        /* Allocate privrecsize bytes */
        if (metric->privrecsize != 0)
        {
            dev->devptr = malloc(metric->privrecsize);
            if (dev->devptr == NULL)
            {
                TerminDev(devid, CXRF_DRV_PROBL, "malloc(privrecsize) error");
                return;
            }
            bzero(dev->devptr, metric->privrecsize);

            /* Do psp_parse() if required */
            if (metric->paramtable != NULL)
            {
                if (psp_parse(auxinfo, NULL,
                              dev->devptr,
                              '=', " \t", "",
                              metric->paramtable) != PSP_R_OK)
                {
                    /*!!! Should do a better-style logging... */
                    DoDriverLog(devid, DRIVERLOG_ERR,
                                "psp_parse(auxinfo)@InitDevice: %s",
                                psp_error());
                    TerminDev(devid, CXRF_CFG_PROBL, "psp_parse(auxinfo)@InitDevice error");
                    return;
                }
            }
        }

        dev->state = DEVSTATE_OPERATING;
        ENTER_DRIVER_S(devid, s_devid);
        state     = dev->state;
        if (metric->init_dev != NULL)
            state = metric->init_dev(devid, dev->devptr,
                                     db_ref->businfocount, db_ref->businfo,
                                     auxinfo);
        LEAVE_DRIVER_S(s_devid);
        gettimeofday(&(dev->stattime), NULL);

        /*!!! Check state!!! */
        if      (state < 0)
        {
            logline(LOGF_MODULES, LOGL_WARNING,
                    "%s: %s[%d].init_dev()=%d -- refusal",
                    __FUNCTION__, metric->mr.name, devid, state);
            TerminDev(devid, state == DEVSTATE_OFFLINE? 0 : -state, NULL/* So that if device made SetDevState(,OFFLINE) itself, the description will remain */);
        }
        else if (state == DEVSTATE_NOTREADY)
            FreezeDev(devid, 0, NULL);
        else  /*(state == DEVSTATE_OPERATING)*/
        {
            ReviveDev(devid);
        }


        /*!!! ...and write to dev->state  */
        dev->state = state;
    }
}

////////////////////////

static void HandleSimulatedHardware(void)
{
  int            devid;
  cxsd_hw_dev_t *dev;
  int            chan;

    return; /* No need now, when SendChanRequest() supports on-request simulation */
    if (!MustSimulateHardware) return;

    for (devid = 1,  dev = cxsd_hw_devices + devid;
         devid < cxsd_hw_numdevs;
         devid++,    dev++)
        for (chan = 0;  chan < dev->count;  chan++)
            if (cxsd_hw_channels[dev->first + chan].rw == 0)
                StdSimulated_rw_p(devid, dev->devptr,
                                  DRVA_READ,
                                  1, &chan,
                                  NULL, NULL, NULL);
}

////////////////////////

int  CxsdHwSetSimulate(int state)
{
    MustSimulateHardware = (state != 0);

    return 0;
}

int  CxsdHwSetCacheRFW(int state)
{
    MustCacheRFW         = (state != 0);

    return 0;
}

int  CxsdHwSetDefDrvLogMask(int mask)
{
    cxsd_hw_defdrvlog_mask = mask;

    return 0;
}


static int cycle_evproc_caller(cycle_cbrec_t *p, void *privptr)
{
  int  reason = ptr2lint(privptr);

    p->evproc(p->uniq, p->privptr1, reason, p->privptr2);

    return 0;
}

static void BeginOfCycle(void)
{
    //!!! HandleSubscriptions(); ???
    //!!! Call frontends' "begin_cycle" methods
    CxsdCallFrontendsBegCy();
    ////fprintf(stderr, "%s %s\n", strcurtime_msc(), __FUNCTION__);

    ForeachCycleCBSlot(cycle_evproc_caller, lint2ptr(CXSD_HW_CYCLE_R_BEGIN));
}

static void EndOfCycle  (void)
{
    HandleSimulatedHardware();
    //!!! Call frontends' "end_cycle" methods
    CxsdCallFrontendsEndCy();
    ////fprintf(stderr, "%s %s\n", strcurtime_msc(), __FUNCTION__);

    ForeachCycleCBSlot(cycle_evproc_caller, lint2ptr(CXSD_HW_CYCLE_R_END));

    ++current_cycle;
}

static void ResetCycles(void)
{
  struct timeval  now;

    gettimeofday(&now, NULL);
    cycle_pass_count = 0;
    cycle_start = now;
    timeval_add_usecs(&cycle_end, &cycle_start, basecyclesize);
}

static void CycleCallback(int       uniq     __attribute__((unused)),
                          void     *privptr1 __attribute__((unused)),
                          sl_tid_t  tid      __attribute__((unused)),
                          void     *privptr2 __attribute__((unused)))
{
  struct timeval  now;

    cycle_tid = -1;

    if (current_cycle == INITIAL_CURRENT_CYCLE) BeginOfCycle();
    EndOfCycle();

    cycle_start = cycle_end;
    timeval_add_usecs(&cycle_end, &cycle_start, basecyclesize);

    ///write(2, "zzz\n", 4);
    gettimeofday(&now, NULL);
    /* Adapt to time change -- check if desired time had already passed */
    if (TV_IS_AFTER(now, cycle_end))
    {
        /* Have we reached the limit? */
        if (cycle_pass_count < 0  /* For init at 1st time */ ||
            cycle_pass_count >= cycle_pass_count_lim)
        {
            ///fprintf(stderr, "\t%s :=0\n", strcurtime_msc());
            ResetCycles();
        }
        /* No, try more */
        else
        {
            cycle_pass_count++;
            ///fprintf(stderr, "\t%s pass_count=%d\n", strcurtime_msc(), cycle_pass_count);
        }
    }
    else
    {
        cycle_pass_count = 0;

        timeval_add_usecs(&now, &now, basecyclesize * 2);
    }

    cycle_tid = sl_enq_tout_at(0, NULL,
                               &cycle_end, CycleCallback, NULL);

    BeginOfCycle();
}

int  CxsdHwSetCycleDur(int cyclesize_us)
{
    if (cycle_tid >= 0)
    {
        sl_deq_tout(cycle_tid);
        cycle_tid = -1;
    }

    basecyclesize = cyclesize_us;
    bzero(&cycle_end, sizeof(cycle_end));
    CycleCallback(0, NULL, cycle_tid, NULL);

    return 0;
}

int  CxsdHwGetCurCycle(void)
{
    return current_cycle;
}

int  CxsdHwTimeChgBack(void)
{
    if (cycle_tid >= 0)
    {
        sl_deq_tout(cycle_tid);
        cycle_tid = -1;
    }
    ResetCycles();
    CycleCallback(0, NULL, cycle_tid, NULL);

    return -1;
}

int  CxsdHwSetCleanup (cxsd_hw_cleanup_proc proc)
{
    cleanup_proc = proc;

    return 0;
}

//--------------------------------------------------------------------

static int cycle_evproc_checker(cycle_cbrec_t *p, void *privptr)
{
  cycle_cbrec_t *model = privptr;

    return
        p->uniq     == model->uniq      &&
        p->privptr1 == model->privptr1  &&
        p->evproc   == model->evproc    &&
        p->privptr2 == model->privptr2;
}

int  CxsdHwAddCycleEvproc(int  uniq, void *privptr1,
                          cxsd_hw_cycle_evproc_t  evproc,
                          void                   *privptr2)
{
  cycle_cbrec_t *p;
  int            n;
  cycle_cbrec_t  rec;

    if (evproc == NULL) return 0;

    /* Check if it is already in the list */
    rec.uniq     = uniq;
    rec.privptr1 = privptr1;
    rec.evproc   = evproc;
    rec.privptr2 = privptr2;
    if (ForeachCycleCBSlot(cycle_evproc_checker, &rec) >= 0) return 0;

    n = GetCycleCBSlot();
    if (n < 0) return -1;
    p = AccessCycleCBSlot(n);

    p->uniq     = uniq;
    p->privptr1 = privptr1;
    p->evproc   = evproc;
    p->privptr2 = privptr2;

    return 0;
}

int  CxsdHwDelCycleEvproc(int  uniq, void *privptr1,
                          cxsd_hw_cycle_evproc_t  evproc,
                          void                   *privptr2)
{
  int            n;
  cycle_cbrec_t  rec;

    if (evproc == NULL) return 0;

    /* Find requested callback */
    rec.uniq     = uniq;
    rec.privptr1 = privptr1;
    rec.evproc   = evproc;
    rec.privptr2 = privptr2;
    n = ForeachCycleCBSlot(cycle_evproc_checker, &rec);
    if (n < 0)
    {
        /* Not found! */
        errno = ENOENT;
        return -1;
    }

    RlsCycleCBSlot(n);

    return 0;
}


//////////////////////////////////////////////////////////////////////

typedef struct
{
    uint8 in_use;
} idinfo_t;

static idinfo_t *clientids_list        = NULL;
static int       clientids_list_allocd = 0;

static int       client_id_ctr         = 1;

// GetClientIDSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, ClientID, idinfo_t,
                                 clientids, in_use, 0, 1,
                                 1, 100, 0,
                                 , , void)

static void RlsClientIDSlot(int cell_n)
{
  idinfo_t *p = AccessClientIDSlot(cell_n);

    p->in_use = 0;
}

int32  CxsdHwCreateClientID(void)
{
  int       cell_n;

    cell_n = GetClientIDSlot();
    if (cell_n < 0) return -1;

    client_id_ctr++;
    if (client_id_ctr > 32767) client_id_ctr = 1;

    return cell_n | (client_id_ctr << 16);
}

int    CxsdHwDeleteClientID(int ID)
{
  int       cell_n = ID & 0xFFFF;
  idinfo_t *p      = AccessClientIDSlot(cell_n);

    if (ID < 65536  ||  cell_n >= clientids_list_allocd  ||  p->in_use == 0)
    {
        errno = EBADF;
        return -1;
    }
    RlsClientIDSlot(cell_n);

    return 0;
}

//--------------------------------------------------------------------

static int only_digits(const char *s)
{
    for (;  *s != '\0';  s++)
        if (!isdigit(*s)) return 0;

    return 1;
}

static void FillPropsOfChan(cxsd_cpntid_t  cpid,
                            cxsd_gchnid_t *gcid_p,
                            int           *phys_count_p,
                            double        *rds_buf,
                            int            rds_buf_cap,
                            char         **ident_p,
                            char         **label_p,
                            char         **tip_p,
                            char         **comment_p,
                            char         **geoinfo_p,
                            char         **rsrvd6_p,
                            char         **units_p,
                            char         **dpyfmt_p)
{
  CxsdDbCpntInfo_t *cp;
  int               rds_buf_used = 0;
  int               n;
  double           *p1;
  double           *p2;
  double            t;

  int               ident_ofs    = -1;
  int               label_ofs    = -1;
  int               tip_ofs      = -1;
  int               comment_ofs  = -1;
  int               geoinfo_ofs  = -1;
  int               rsrvd6_ofs   = -1;
  int               units_ofs    = -1;
  int               dpyfmt_ofs   = -1;

    while ((cpid & CXSD_DB_CPOINT_DIFF_MASK) != 0)
    {
        cp  = cxsd_hw_cur_db->cpnts + (cpid & CXSD_DB_CHN_CPT_IDN_MASK);
////fprintf(stderr, "   %d/%d -> %d\n", cpid, cpid & CXSD_DB_CHN_CPT_IDN_MASK, cp->ref_n);
        cpid = cp->ref_n;
        if ((cpid & CXSD_DB_CPOINT_DIFF_MASK) == 0)
            cpid += cxsd_hw_devices[cp->devid].first;

        if (cp->phys_rd_specified  &&
            rds_buf_cap > 0  &&  rds_buf_used < rds_buf_cap)
        {
            rds_buf[rds_buf_used * 2 + 0] = cp->phys_rds[0];
            rds_buf[rds_buf_used * 2 + 1] = cp->phys_rds[1];
            rds_buf_used++;
        }

        if (ident_ofs   < 0) ident_ofs   = cp->ident_ofs;
        if (label_ofs   < 0) label_ofs   = cp->label_ofs;
        if (tip_ofs     < 0) tip_ofs     = cp->tip_ofs;
        if (comment_ofs < 0) comment_ofs = cp->comment_ofs;
        if (geoinfo_ofs < 0) geoinfo_ofs = cp->geoinfo_ofs;
        if (rsrvd6_ofs  < 0) rsrvd6_ofs  = cp->rsrvd6_ofs;
        if (units_ofs   < 0) units_ofs   = cp->units_ofs;
        if (dpyfmt_ofs  < 0) dpyfmt_ofs  = cp->dpyfmt_ofs;
    }
    /* Note: below this point "cpid" is actually a "gcid",
             since it had been unwound to a globalchan in the loop above. */

    *gcid_p        = cpid;

    if (cxsd_hw_channels[cpid].phys_rd_specified  &&
        rds_buf_cap > 0  &&  rds_buf_used < rds_buf_cap)
    {
        rds_buf[rds_buf_used * 2 + 0] = cxsd_hw_channels[cpid].phys_rds[0];
        rds_buf[rds_buf_used * 2 + 1] = cxsd_hw_channels[cpid].phys_rds[1];
        rds_buf_used++;
    }

    if (ident_p   != NULL) *ident_p   = CxsdDbGetStr(cxsd_hw_cur_db, ident_ofs);
    if (label_p   != NULL) *label_p   = CxsdDbGetStr(cxsd_hw_cur_db, label_ofs);
    if (tip_p     != NULL) *tip_p     = CxsdDbGetStr(cxsd_hw_cur_db, tip_ofs);
    if (comment_p != NULL) *comment_p = CxsdDbGetStr(cxsd_hw_cur_db, comment_ofs);
    if (geoinfo_p != NULL) *geoinfo_p = CxsdDbGetStr(cxsd_hw_cur_db, geoinfo_ofs);
    if (rsrvd6_p  != NULL) *rsrvd6_p  = CxsdDbGetStr(cxsd_hw_cur_db, rsrvd6_ofs);
    if (units_p   != NULL) *units_p   = CxsdDbGetStr(cxsd_hw_cur_db, units_ofs);
    if (dpyfmt_p  != NULL) *dpyfmt_p  = CxsdDbGetStr(cxsd_hw_cur_db, dpyfmt_ofs);

    if (phys_count_p != NULL) *phys_count_p = rds_buf_used;
    if (rds_buf_used > 1)
        for (n = rds_buf_used / 2, p1 = rds_buf, p2 = rds_buf + rds_buf_used*2 - 2;
             n > 0;
             n--,                  p1 += 2,      p2 -= 2)
        {
            t = p1[0]; p1[0] = p2[0]; p2[0] = t;
            t = p1[1]; p1[1] = p2[1]; p2[1] = t;
        }

////fprintf(stderr, "%s used=%d\n", __FUNCTION__, rds_buf_used);
}

cxsd_cpntid_t  CxsdHwResolveChan(const char    *name,
                                 cxsd_gchnid_t *gcid_p,
                                 int           *phys_count_p,
                                 double        *rds_buf,
                                 int            rds_buf_cap,
                                 char         **ident_p,
                                 char         **label_p,
                                 char         **tip_p,
                                 char         **comment_p,
                                 char         **geoinfo_p,
                                 char         **rsrvd6_p,
                                 char         **units_p,
                                 char         **dpyfmt_p)
{
  cxsd_cpntid_t          cpid;
  int                    chan;
  int                    devid;

  int                    res_res;  // RESolve RESult

    res_res = CxsdDbResolveName(cxsd_hw_cur_db, name, &devid, &chan);
////fprintf(stderr, "  res_res=%d devid=%d chan=%d &=%d\n", res_res, devid, chan, chan & CXSD_DB_CHN_CPT_IDN_MASK);
    if      (res_res == CXSD_DB_RESOLVE_DEVCHN)
    {
        cpid = cxsd_hw_devices[devid].first + chan;
    }
    else if (res_res == CXSD_DB_RESOLVE_GLOBAL  ||
             res_res == CXSD_DB_RESOLVE_CPOINT)
    {
        cpid = chan;
    }
    else return -1;

    FillPropsOfChan(cpid, gcid_p,
                    phys_count_p, rds_buf, rds_buf_cap,
                    ident_p,   label_p,  tip_p,   comment_p,
                    geoinfo_p, rsrvd6_p, units_p, dpyfmt_p);
////fprintf(stderr, "returning %d *gcid_p=%d\n", cpid, *gcid_p);

    return cpid;
}

int            CxsdHwGetCpnProps(cxsd_cpntid_t  cpid,
                                 cxsd_gchnid_t *gcid_p,
                                 int           *phys_count_p,
                                 double        *rds_buf,
                                 int            rds_buf_cap,
                                 char         **ident_p,
                                 char         **label_p,
                                 char         **tip_p,
                                 char         **comment_p,
                                 char         **geoinfo_p,
                                 char         **rsrvd6_p,
                                 char         **units_p,
                                 char         **dpyfmt_p)
{
    if (cpid <= 0
        ||
        ((cpid & CXSD_DB_CPOINT_DIFF_MASK) == 0  &&
          cpid >= cxsd_hw_numchans)
        ||
        ((cpid & CXSD_DB_CPOINT_DIFF_MASK) != 0  &&
         (cpid & CXSD_DB_CHN_CPT_IDN_MASK) >= cxsd_hw_cur_db->cpnts_used))
        return -1;
    
    FillPropsOfChan(cpid, gcid_p,
                    phys_count_p, rds_buf, rds_buf_cap,
                    ident_p,   label_p,  tip_p,   comment_p,
                    geoinfo_p, rsrvd6_p, units_p, dpyfmt_p);
    
    return 0;
}


enum {DRVA_IGNORE = -999};

static inline int IsCompatible(cxsd_hw_chan_t *chn_p,
                               cxdtype_t dtype, int nels)
{
  int             srpr;
  int             repr;

    /* Check nelems */
    if (nels < 0  ||  (chn_p->max_nelems == 1  &&  nels != 1)) return 0;

    /* Check dtype */
    srpr = reprof_cxdtype(dtype);
    repr = reprof_cxdtype(chn_p->dtype);
    if (
        /* a. Type is the same */
        /*!!! NOTE: this presumes that inside a representation
           any size conversion is possible, which in fact isn't a case
           between TEXTs */
        srpr == repr
#if MAY_USE_FLOAT
        ||
        /* b. float->int, int->float */
        ((srpr == CXDTYPE_REPR_FLOAT  ||  srpr == CXDTYPE_REPR_INT)  &&
         (repr == CXDTYPE_REPR_FLOAT  ||  repr == CXDTYPE_REPR_INT))
#endif
       )
        return 1;

    return 0;
}

static void StoreForSending(cxsd_gchnid_t gcid,
                            cxdtype_t *dtype_p, int *nelems_p, void **values_p,
                            int force)
{
  cxsd_hw_chan_t *chn_p      = cxsd_hw_channels + gcid;
  int             nels;
  uint8          *src;
  uint8          *dst;

  int             srpr;
  size_t          ssiz;
  int             repr;
  size_t          size;

  int32           iv32;
#if MAY_USE_INT64
  int64           iv64;
#endif
#if MAY_USE_FLOAT
  float64         fv64;
#endif

    /* Note: no need to check compatibility, since it was done by caller */

    if (*nelems_p > chn_p->max_nelems)
        *nelems_p = chn_p->max_nelems;
    nels = *nelems_p;

    srpr = reprof_cxdtype(*dtype_p);
    ssiz = sizeof_cxdtype(*dtype_p);
    repr = reprof_cxdtype(chn_p->dtype);
    size = sizeof_cxdtype(chn_p->dtype);

    /* a. Identical */
    if      (srpr == repr  &&  ssiz == size)
    {
        if (0  &&  !force) return;
        chn_p->next_wr_nelems = nels;
        if (nels > 0)
            memcpy(chn_p->next_wr_val, *values_p, nels * chn_p->usize);
    }
    /* b. Integer */
    else if (srpr == CXDTYPE_REPR_INT  &&  repr == CXDTYPE_REPR_INT)
    {
        chn_p->next_wr_nelems = nels;
        src = *values_p;
        dst = chn_p->next_wr_val;
#if MAY_USE_INT64
        if (ssiz == sizeof(int64)  ||  size == sizeof(int64))
            while (nels > 0)
            {
                // Read datum, converting to int64
                switch (*dtype_p)
                {
                    case CXDTYPE_INT32:  iv64 = *((  int32*)src);     break;
                    case CXDTYPE_UINT32: iv64 = *(( uint32*)src);     break;
                    case CXDTYPE_INT16:  iv64 = *((  int16*)src);     break;
                    case CXDTYPE_UINT16: iv64 = *(( uint16*)src);     break;
                    case CXDTYPE_INT64:
                    case CXDTYPE_UINT64: iv64 = *(( uint64*)src);     break;
                    case CXDTYPE_INT8:   iv64 = *((  int8 *)src);     break;
                    default:/*   UINT8*/ iv64 = *(( uint8 *)src);     break;
                }
                src += ssiz;

                // Store datum, converting from int64
                switch (chn_p->dtype)
                {
                    case CXDTYPE_INT32:
                    case CXDTYPE_UINT32:      *((  int32*)dst) = iv64; break;
                    case CXDTYPE_INT16:
                    case CXDTYPE_UINT16:      *((  int16*)dst) = iv64; break;
                    case CXDTYPE_INT64:
                    case CXDTYPE_UINT64:      *(( uint64*)dst) = iv64; break;
                    default:/*   *INT8*/      *((  int8 *)dst) = iv64; break;
                }
                dst += size;

                nels--;
            }
        else
#endif
        while (nels > 0)
        {
            // Read datum, converting to int32
            switch (*dtype_p)
            {
                case CXDTYPE_INT32:
                case CXDTYPE_UINT32: iv32 = *((  int32*)src);     break;
                case CXDTYPE_INT16:  iv32 = *((  int16*)src);     break;
                case CXDTYPE_UINT16: iv32 = *(( uint16*)src);     break;
                case CXDTYPE_INT8:   iv32 = *((  int8 *)src);     break;
                default:/*   UINT8*/ iv32 = *(( uint8 *)src);     break;
            }
            src += ssiz;

            // Store datum, converting from int32
            switch (chn_p->dtype)
            {
                case CXDTYPE_INT32:
                case CXDTYPE_UINT32:      *((  int32*)dst) = iv32; break;
                case CXDTYPE_INT16:
                case CXDTYPE_UINT16:      *((  int16*)dst) = iv32; break;
                default:/*   *INT8*/      *((  int8 *)dst) = iv32; break;
            }
            dst += size;

            nels--;
        }
    }
#if MAY_USE_FLOAT
    /* c. Float: float->float, float->int, int->float */
    else if ((srpr == CXDTYPE_REPR_FLOAT  ||  srpr == CXDTYPE_REPR_INT)  &&
             (repr == CXDTYPE_REPR_FLOAT  ||  repr == CXDTYPE_REPR_INT))
    {
        chn_p->next_wr_nelems = nels;
        src = *values_p;
        dst = chn_p->next_wr_val;
        while (nels > 0)
        {
            // Read datum, converting to float64 (double)
            switch (*dtype_p)
            {
                case CXDTYPE_INT32:  fv64 = *((  int32*)src);     break;
                case CXDTYPE_UINT32: fv64 = *(( uint32*)src);     break;
                case CXDTYPE_INT16:  fv64 = *((  int16*)src);     break;
                case CXDTYPE_UINT16: fv64 = *(( uint16*)src);     break;
                case CXDTYPE_DOUBLE: fv64 = *((float64*)src);     break;
                case CXDTYPE_SINGLE: fv64 = *((float32*)src);     break;
                case CXDTYPE_INT64:  fv64 = *((  int64*)src);     break;
                case CXDTYPE_UINT64: fv64 = *(( uint64*)src);     break;
                case CXDTYPE_INT8:   fv64 = *((  int8 *)src);     break;
                default:/*   UINT8*/ fv64 = *(( uint8 *)src);     break;
            }
            src += ssiz;

            // Store datum, converting from float64 (double)
            switch (chn_p->dtype)
            {
                case CXDTYPE_INT32:       *((  int32*)dst) = fv64; break;
                case CXDTYPE_UINT32:      *(( uint32*)dst) = fv64; break;
                case CXDTYPE_INT16:       *((  int16*)dst) = fv64; break;
                case CXDTYPE_UINT16:      *(( uint16*)dst) = fv64; break;
                case CXDTYPE_DOUBLE:      *((float64*)dst) = fv64; break;
                case CXDTYPE_SINGLE:      *((float32*)dst) = fv64; break;
                case CXDTYPE_INT64:       *((  int64*)dst) = fv64; break;
                case CXDTYPE_UINT64:      *(( uint64*)dst) = fv64; break;
                case CXDTYPE_INT8:        *((  int8 *)dst) = fv64; break;
                default:/*   UINT8*/      *((  int8 *)dst) = fv64; break;
            }
            dst += size;

            nels--;
        }
    }
#endif
    /* Since we are here, storing HAS happened */
    *dtype_p  = chn_p->dtype;
    *nelems_p = chn_p->next_wr_nelems;
    *values_p = chn_p->next_wr_val;
}

/* Note:
       This code supposes that ALL channels belong to a same single devid. */
static void SendChanRequest(int            requester,
                            int            action,
                            int            offset,
                            int            length,
                            cxsd_gchnid_t *gcids,
                            cxdtype_t *dtypes, int *nelems, void **values)
{
  int              devid;
  int              s_devid;
  cxsd_hw_dev_t   *dev;

  CxsdDevChanProc  do_rw;

  int              n;
  enum            {SEGLEN_MAX = 1000};
  int              seglen;
  int              addrs [SEGLEN_MAX];
  rflags_t         rflags[SEGLEN_MAX];
  int              x;

    gcids += offset;
    if (action == DRVA_WRITE)
    {
        dtypes += offset;
        nelems += offset;
        values += offset;
    }
    else
    {
        dtypes = NULL;
        nelems = NULL;
        values = NULL;
    }

    devid = cxsd_hw_channels[*gcids].devid;
    dev   = cxsd_hw_devices + devid;
    do_rw = dev->metric != NULL? dev->metric->do_rw : NULL;
    if (MustSimulateHardware) do_rw = StdSimulated_rw_p;

//fprintf(stderr, "%s/%d(ofs=%d,count=%d)", __FUNCTION__, action, offset, length);
//for (n = 0;  n < length;  n++) fprintf(stderr, " %d", gcids[n]);
//fprintf(stderr, "\n");
    for (n = 0;  n < length;  n += seglen)
    {
        seglen = length - n;
        if (seglen > SEGLEN_MAX) seglen = SEGLEN_MAX;

        for (x = 0;  x < seglen;  x++)
            addrs[x] = gcids[x] - dev->first;

        //ENTER_DRIVER_S(devid, s_devid);
        {
            if (do_rw != NULL  &&  dev->state == DEVSTATE_OPERATING)
                do_rw(devid, dev->devptr,
                      action,
                      seglen,
                      addrs,
                      dtypes, nelems, values);
        }
        //LEAVE_DRIVER_S(s_devid);

        if (action == DRVA_WRITE)
        {
            dtypes += seglen;
            nelems += seglen;
            values += seglen;
        }
    }
}

static int  ConsiderRequest(int            requester,
                            int            action,
                            int            offset,
                            cxsd_gchnid_t *gcids,
                            cxdtype_t *dtypes, int *nelems, void **values,
                            int            may_act,
                            int            f_act,
                            int            ignore_upd_cycle)
{
  cxsd_gchnid_t   gcid  = gcids[offset];
  cxsd_hw_chan_t *chn_p = cxsd_hw_channels + gcid;

//fprintf(stderr, " %s(%d)\n", __FUNCTION__, gcid);

    /* Is it a readonly channel? */
    if (chn_p->rw == 0)
    {
        if (chn_p->rd_req  ||  
            (chn_p->upd_cycle == current_cycle  &&  ignore_upd_cycle == 0)  ||
            chn_p->is_autoupdated)
        {
            return DRVA_IGNORE;
        }
        else
        {
            if (may_act  &&  f_act == DRVA_READ) chn_p->rd_req = 1;
            return DRVA_READ;
        }
    }

    /* Okay, it is a write channel...  What do we need? */
    if (action == DRVA_WRITE)
    {
        if (chn_p->locker != 0  &&  chn_p->locker != requester)   return DRVA_IGNORE;
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
        if (reprof_cxdtype(chn_p->dtype) == CXDTYPE_REPR_UNKNOWN) return DRVA_IGNORE;
#endif
        if (!IsCompatible(chn_p, dtypes[offset], nelems[offset])) return DRVA_IGNORE;

        if (chn_p->wr_req)
        {
            if (may_act  &&  f_act == DRVA_IGNORE)
            {
                /*!!! Store data to next_wr_val,next_wr_nelems */
                StoreForSending(gcid,
                                dtypes+offset, nelems+offset, values+offset,
                                1);
                chn_p->next_wr_val_pnd = 1;
            }
            return DRVA_IGNORE;
        }
        else
        {
            if (may_act  &&  f_act == DRVA_WRITE)
            {
                StoreForSending(gcid,
                                dtypes+offset, nelems+offset, values+offset,
                                0);
                chn_p->wr_req = 1;
            }
            return DRVA_WRITE;
        }
    }
    else
    {
        /* Note:
                   Treating !IsReqRofWrChsIng identically to MustCacheRFW,
               so that it is affected by upd_cycle==current_cycle check,
               isn't entirely correct: a device can change state several
               times a cycle, and should perform initial readout EACH time.
                   But the problem is resolved by ReviveDev() calling
               RstDevUpdCycles() first, so that upd_cycle becomes =0
               (infinitely old).
         */
        if ((MustCacheRFW  &&  !IsReqRofWrChsIng)  ||
            chn_p->rd_req  ||  chn_p->wr_req       ||
            chn_p->upd_cycle == current_cycle)
            return DRVA_IGNORE;
        else
        {
            if (may_act  &&  f_act == DRVA_READ) chn_p->rd_req = 1;
            return DRVA_READ;
        }
    }
}

int  CxsdHwDoIO         (int  requester,
                         int  action,
                         int  count, cxsd_gchnid_t *gcids,
                         cxdtype_t *dtypes, int *nelems, void **values)
{
  int             n;
  cxsd_gchnid_t   first;
  int             length;
  int             devid;
  int             f_act;
  int             ignore_upd_cycle;

#if 0
fprintf(stderr, "%s %d count=%d:", __FUNCTION__, action, count);
for (n = 0;  n < count;  n++) fprintf(stderr, " %d", gcids[n]);
fprintf(stderr, "\n");
#endif
    ignore_upd_cycle = (action & CXSD_HW_DRVA_IGNORE_UPD_CYCLE_FLAG) != 0;
    action &=~ CXSD_HW_DRVA_IGNORE_UPD_CYCLE_FLAG;

    for (n = 0;  n < count;  /*NO-OP*/)
    {
        /* Skip "invalid" references */
        while (n < count  &&
               (gcids[n] <= 0  ||  gcids[n] > cxsd_hw_numchans))
            n++;
        if (n >= count) break;

        /* Get "model" parameters */
        first = n;
        devid = cxsd_hw_channels[gcids[n]].devid;
        f_act = ConsiderRequest(requester, action, first,
                                gcids, dtypes, nelems, values,
                                0, DRVA_IGNORE, ignore_upd_cycle);
        //n++;

//fprintf(stderr, "  f_act(%d)=%d\n", gcids[first], f_act);
        /* Find out how many channels can be packed */
        while (n < count
               &&
               (gcids[n] > 0  &&  gcids[n] < cxsd_hw_numchans)
               &&
               cxsd_hw_channels[gcids[n]].devid == devid
               &&
               ConsiderRequest(requester, action, n,
                               gcids, dtypes, nelems, values,
                               1, f_act, ignore_upd_cycle) == f_act)
            n++;

        length = n - first;

//fprintf(stderr, "\t%d,+%d act=%d\n", first, length, f_act);
        switch (f_act)
        {
            case DRVA_IGNORE: break;
            case DRVA_READ:
            case DRVA_WRITE:
                SendChanRequest(requester, f_act,
                                first, length,
                                gcids, dtypes, nelems, values);
                break;
        }
    }

    return 0;
}

int  CxsdHwLockChannels (int  requester,
                         int  count, cxsd_gchnid_t *gcids,
                         int  operation)
{
    return -1;
}

static void ReqRofWrChsOf(int devid)
{
  int              rrowc;
  cxsd_hw_dev_t   *dev;
  int              n;
  int              last;
  enum            {SEGLEN_MAX = 1000};
  int              seglen;
  int              addrs [SEGLEN_MAX];
  int              x;

    ////fprintf(stderr, "%s(%d)\n", __FUNCTION__, devid);
    if (!MustCacheRFW) return; // No need to do anything with uncacheable channels

    /* Inform ConsiderRequest() that we are REALLY reading write-channels */
    rrowc = IsReqRofWrChsIng;
    IsReqRofWrChsIng = 1;

    dev   = cxsd_hw_devices + devid;

    for (n = 0;
         n < dev->count  &&  dev->state == DEVSTATE_OPERATING;
         n += seglen)
    {
        /* Filter-out non-rw channels: */
        /* 1. Find first rw channel */
        while (n    < dev->count  &&
               cxsd_hw_channels[dev->first + n   ].rw == 0) n++;
        /* 2. Find last rw channel */
        last = n;
        while (last < dev->count  &&
               cxsd_hw_channels[dev->first + last].rw != 0) last++;

        seglen = last - n;
        if (seglen == 0) break;
        //seglen = dev->count - n;
        if (seglen > SEGLEN_MAX) seglen = SEGLEN_MAX;

        for (x = 0;  x < seglen;  x++)
            addrs[x] = dev->first + n + x;

        CxsdHwDoIO(0, DRVA_READ, seglen, addrs, NULL, NULL, NULL);
    }

    IsReqRofWrChsIng = rrowc;
}

//--------------------------------------------------------------------

static void SetDevRflags    (int devid, rflags_t rflags_to_set)
{
  cxsd_hw_dev_t  *dev_p = cxsd_hw_devices + devid;
  int             x;
  cxsd_gchnid_t   gcid;
  cxsd_hw_chan_t *chn_p;

  CxsdHwChanEvCallInfo_t  call_info;

    call_info.reason = CXSD_HW_CHAN_R_STATCHG;
    call_info.evmask = 1 << call_info.reason;

    for (x = dev_p->count, gcid = dev_p->first, chn_p = cxsd_hw_channels + dev_p->first;
         x > 0;
         x--,              gcid++,              chn_p++)
    {
        chn_p->rflags  |= rflags_to_set;
        chn_p->crflags |= rflags_to_set;

        CxsdHwCallChanEvprocs(gcid, &call_info);
    }
}

static void RstDevRflags    (int devid)
{
  cxsd_hw_dev_t  *dev_p = cxsd_hw_devices + devid;
  int             x;
  cxsd_hw_chan_t *chn_p;

    for (x = dev_p->count, chn_p = cxsd_hw_channels + dev_p->first;
         x > 0;
         x--, chn_p++)
    {
        chn_p->rflags   = 0;
        chn_p->crflags  = 0;
    }
}

static void RstDevTimestamps(int devid)
{
  cxsd_hw_dev_t  *dev_p = cxsd_hw_devices + devid;
  int             x;
  cxsd_hw_chan_t *chn_p;

    for (x = dev_p->count, chn_p = cxsd_hw_channels + dev_p->first;
         x > 0;
         x--, chn_p++)
    {
        chn_p->timestamp.sec  = INITIAL_TIMESTAMP_SECS;
        chn_p->timestamp.nsec = 0;
        chn_p->upd_cycle      = 0;
    }
}

static void RstDevUpdCycles (int devid)
{
  cxsd_hw_dev_t  *dev_p = cxsd_hw_devices + devid;
  int             x;
  cxsd_hw_chan_t *chn_p;

    for (x = dev_p->count, chn_p = cxsd_hw_channels + dev_p->first;
         x > 0;
         x--, chn_p++)
    {
        chn_p->upd_cycle      = 0;
    }
}

static void report_devstate(int devid, const char *description)
{
  cxsd_hw_dev_t *dev_p = cxsd_hw_devices + devid;

  int32          _devstate = dev_p->state;
  int            len;
  int            count;
  int            addrs [2];
  cxdtype_t      dtypes[2];
  int            nelems[2];
  void          *values[2];
  rflags_t       rflags[2];

    count = 1;
    addrs [0] = dev_p->count + CXSD_DB_CHAN_DEVSTATE_OFS;
    dtypes[0] = CXDTYPE_INT32;
    nelems[0] = 1;
    values[0] = &_devstate;
    rflags[0] = 0;
    if (description != NULL)
    {
        len = strlen(description);
        if (len > _DEVSTATE_DESCRIPTION_MAX_NELEMS)
            len = _DEVSTATE_DESCRIPTION_MAX_NELEMS;

        count++;
        addrs [1] = dev_p->count + CXSD_DB_CHAN_DEVSTATE_DESCRIPTION_OFS;
        dtypes[1] = CXDTYPE_TEXT;
        nelems[1] = len;
        values[1] = description;
        rflags[1] = 0;
    }
    ReturningInternal = 1;
    ReturnDataSet(devid, count, addrs, dtypes, nelems, values, rflags, NULL);
}

static void TerminDev(int devid, rflags_t rflags_to_set, const char *description)
{
  cxsd_hw_dev_t *dev_p = cxsd_hw_devices + devid;
  cxsd_hw_lyr_t *lyr_p;
  int            s_devid;

    rflags_to_set |= CXRF_OFFLINE;

    /* I. Call its termination function */
    if (dev_p->state != DEVSTATE_OFFLINE  &&  !MustSimulateHardware)
    {
        ENTER_DRIVER_S(devid, s_devid);
        if (dev_p->metric           != NULL  &&
            dev_p->metric->term_dev != NULL)
            dev_p->metric->term_dev(devid, dev_p->devptr);
        if (dev_p->lyrid != 0                                           &&
            (lyr_p = cxsd_hw_layers + (-dev_p->lyrid))->metric != NULL  &&
            lyr_p->metric->disconnect != NULL)
            lyr_p->metric->disconnect(devid);
        LEAVE_DRIVER_S(s_devid);
    }

    dev_p->state      = DEVSTATE_OFFLINE;
    gettimeofday(&(dev_p->stattime), NULL);
    dev_p->statrflags = rflags_to_set;
    SetDevRflags(devid, rflags_to_set);

    /* II. Free all of its resources which are still left */

    /* Private pointer */
    if (dev_p->metric           != NULL  &&
        dev_p->metric->privrecsize != 0  &&  dev_p->devptr != NULL)
    {
        if (dev_p->metric->paramtable != NULL)
            psp_free(dev_p->devptr, dev_p->metric->paramtable);
        free(dev_p->devptr);
    }
    dev_p->devptr = NULL;

    /* Other resources */
    /*!!! cda? cxlib? */
    if (cleanup_proc != NULL)
        cleanup_proc(devid);
    fdio_do_cleanup (devid);
    sl_do_cleanup   (devid);

    /*!!! Manage _devstate channel */
    report_devstate(devid, description);
}

static void FreezeDev(int devid, rflags_t rflags_to_set, const char *description)
{
  cxsd_hw_dev_t *dev_p = cxsd_hw_devices + devid;

    rflags_to_set |= CXRF_OFFLINE;

    dev_p->state      = DEVSTATE_NOTREADY;
    gettimeofday(&(dev_p->stattime), NULL);
    dev_p->statrflags = rflags_to_set;
    SetDevRflags(devid, rflags_to_set);

    /*!!! Manage _devstate channel */
    report_devstate(devid, description);
}

static void ReviveDev(int devid)
{
  cxsd_hw_dev_t *dev_p = cxsd_hw_devices + devid;

    ////fprintf(stderr, "%s(%d)\n", __FUNCTION__, devid);
    dev_p->state      = DEVSTATE_OPERATING;
    gettimeofday(&(dev_p->stattime), NULL);
    dev_p->statrflags = 0;
    //RstDevRflags    (devid);
    RstDevUpdCycles (devid);

    ReRequestDevData(devid);
    ReqRofWrChsOf   (devid);

    /* Are we still alive, nothing had intervened? */
    if (dev_p->state == DEVSTATE_OPERATING)
        /*!!! Manage _devstate channel */
        report_devstate(devid, "");
}

void        SetDevState      (int devid, int state,
                              rflags_t rflags_to_set, const char *description)
{
    CHECK_SANITY_OF_DEVID_WO_STATE();

    /* Is `state' sane? */
    if (state != DEVSTATE_OFFLINE  &&  state != DEVSTATE_NOTREADY  &&  state != DEVSTATE_OPERATING)
    {
        logline(LOGF_SYSTEM, 0, "%s: (devid=%d/active=%d) request for non-existent state %d",
                __FUNCTION__, devid, active_devid, state);
        return;
    }
    
    /* Escaping from OFFLINE is forbidden */
    if (cxsd_hw_devices[devid].state == DEVSTATE_OFFLINE)
    {
        logline(LOGF_SYSTEM, 0, "%s: (devid=%d/active=%d) attempt to move from DEVSTATE_OFFLINE to %d",
                __FUNCTION__, devid, active_devid, state);
        return;
    }
    /* No-op? */
    //if (state == cxsd_hw_devices[devid].state) return;

    if      (state == DEVSTATE_OFFLINE)   TerminDev(devid, rflags_to_set, description);
    else if (state == DEVSTATE_NOTREADY)  FreezeDev(devid, rflags_to_set, description);
    else if (state == DEVSTATE_OPERATING) ReviveDev(devid);
}

static int  ShouldReRequestChan(cxsd_gchnid_t  gcid)
{
    if      (cxsd_hw_channels[gcid].wr_req) return DRVA_WRITE;
    else if (cxsd_hw_channels[gcid].rd_req) return DRVA_READ;
    else                                    return DRVA_IGNORE;
}
void        ReRequestDevData (int devid)
{
  cxsd_hw_dev_t *dev;
  cxsd_gchnid_t  barrier;
  cxsd_gchnid_t  gcid;
  cxsd_gchnid_t  first;
  int            length;
  int            f_act;

  int            n;
  enum          {SEGLEN_MAX = 1000};
  int            seglen;
  int            addrs [SEGLEN_MAX];
  cxdtype_t      dtypes[SEGLEN_MAX];
  int            nelems[SEGLEN_MAX];
  void          *values[SEGLEN_MAX];
  int            x;

    CHECK_SANITY_OF_DEVID();

    dev   = cxsd_hw_devices + devid;

    barrier = dev->first + dev->count;

    for (gcid = dev->first;
         gcid < barrier  &&  dev->state == DEVSTATE_OPERATING;
         /*NO-OP*/)
    {
        /* Get "model" parameters */
        first = gcid;
        f_act = ShouldReRequestChan(gcid);
        gcid++;

        /* Find out how many channels can be packed */
        while (gcid < barrier  &&  ShouldReRequestChan(gcid) == f_act)
            gcid++;

        length = gcid - first;

        if (f_act == DRVA_IGNORE) goto NEXT_GROUP;

        for (n = 0;
             n < length  &&  dev->state == DEVSTATE_OPERATING;
             n += seglen)
        {
            seglen = length - n;
            if (seglen > SEGLEN_MAX) seglen = SEGLEN_MAX;
            
            for (x = 0;  x < seglen;  x++)
            {
                addrs[x] = first + x/* - dev->first*/;
                if (f_act == DRVA_WRITE)
                {
                    dtypes[x] = cxsd_hw_channels[first + x].dtype;
                    nelems[x] = cxsd_hw_channels[first + x].next_wr_nelems;
                    values[x] = cxsd_hw_channels[first + x].next_wr_val;
                }
            }
            if (f_act == DRVA_WRITE)
                SendChanRequest(0/*!!!*/,
                                f_act,
                                0,
                                seglen,
                                addrs,
                                dtypes, nelems, values);
            else
                SendChanRequest(0/*!!!*/,
                                f_act,
                                0,
                                seglen,
                                addrs,
                                NULL, NULL, NULL);
                    
        }

 NEXT_GROUP:;
    }
}


//--------------------------------------------------------------------

static inline int ShouldWrNext(cxsd_hw_dev_t  *dev, int chan)
{
    return chan >= 0  &&  chan < dev->count  &&
        cxsd_hw_channels[dev->first + chan].next_wr_val_pnd;
}
/* Note:
       This code supposes that ALL channels belong to a same single devid. */
static void TryWrNext (int devid,
                       int count,
                       int *addrs)
{
#if 1
  cxsd_hw_dev_t  *dev = cxsd_hw_devices + devid;

  int             first; /*!!! In "flush" rename "count" to seglen */
  int             length;
  int             should_wr;
  int             z;

  int             n;
  enum           {SEGLEN_MAX = 1000};
  int             seglen;
  int             gchans[SEGLEN_MAX];
  cxdtype_t       dtypes[SEGLEN_MAX];
  int             nelems[SEGLEN_MAX];
  void           *values[SEGLEN_MAX];
  int             x;
  cxsd_gchnid_t   gcid;
  cxsd_hw_chan_t *chn_p;

    for (n = 0;
         n < count  &&  dev->state == DEVSTATE_OPERATING;
         /* NO-OP */)
    {
        /* Get "model" parameters */
        first = n;
        should_wr = ShouldWrNext(dev, addrs[first]);

        /* Find out how many channels can be packed */
        while (n < count  &&  ShouldWrNext(dev, addrs[n]) == should_wr)
            n++;

        length = n - first;

////if (should_wr) fprintf(stderr, "should_wr [%d]=%d %d\n", first, addrs[first], length);
        if (should_wr)
            for (z = 0;
                 z < length  &&  dev->state == DEVSTATE_OPERATING;
                 z += seglen)
            {
                seglen = length - z;
                if (seglen > SEGLEN_MAX) seglen = SEGLEN_MAX;
                
                for (x = 0;  x < seglen;  x++)
                {
                    gcid = dev->first + addrs[first + z];
////fprintf(stderr, " gcid=%d\n", gcid);
                    chn_p = cxsd_hw_channels + gcid;
                    gchans[x] = gcid;
                    dtypes[x] = chn_p->dtype;
                    nelems[x] = chn_p->next_wr_nelems;
                    values[x] = chn_p->next_wr_val;

                    chn_p->next_wr_val_pnd = 0;
                }
                SendChanRequest(0/*!!!*/,
                                DRVA_WRITE,
                                0,
                                seglen,
                                gchans,
                                dtypes, nelems, values);
            }

        n += length;
    }
#endif
}

void ReturnDataSet    (int devid,
                       int count,
                       int *addrs, cxdtype_t *dtypes, int *nelems,
                       void **values, rflags_t *rflags, cx_time_t *timestamps)
{
  cxsd_hw_dev_t  *dev = cxsd_hw_devices + devid;

  struct timeval  timenow;
  cx_time_t       timestamp;

  int             internal;

  int             x;
  int             chan;
  cxsd_gchnid_t   gcid;
  cxsd_hw_chan_t *chn_p;
  int             nels;
  uint8          *src;
  uint8          *dst;

  int             srpr;
  size_t          ssiz;
  int             repr;
  size_t          size;

  int32           iv32;
#if MAY_USE_INT64
  int64           iv64;
#endif
#if MAY_USE_FLOAT
  float64         fv64;
#endif

  CxsdHwChanEvCallInfo_t  call_info;

    internal = ReturningInternal;
    ReturningInternal = 0;

    if (count == RETURNDATA_COUNT_PONG) return; // In server that's just a NOP

    if (!internal)
    {
        CHECK_SANITY_OF_DEVID();

        /* Check the 'count' */
        if (count == 0) return;
        if (count < 0  ||  count > dev->count)
        {
            logline(LOGF_MODULES, LOGL_WARNING,
                    "%s(devid=%d/active=%d): count=%d, out_of[1...dev->count=%d]",
                    __FUNCTION__, devid, active_devid, count, dev->count);
            return;
        }
    }

    gettimeofday(&timenow, NULL);
    timestamp.sec  = timenow.tv_sec;
    timestamp.nsec = timenow.tv_usec * 1000;

    /* I. Update */
    for (x = 0;  x < count;  x++)
    {
        /* Get pointer to the channel in question */
        chan = addrs[x];
        if (!internal  &&
            (chan < 0  ||  chan >= dev->count))
        {
            logline(LOGF_MODULES, LOGL_WARNING,
                    "%s(devid=%d/active=%d): addrs[%d]=%d, out_of[0...dev->count=%d)",
                    __FUNCTION__, devid, active_devid, x, chan, dev->count);
            goto NEXT_TO_UPDATE;
        }
        gcid  = chan + dev->first;
        chn_p = cxsd_hw_channels + gcid;

        /* Check nelems */
        nels = nelems[x];
        if      (nels < 0)
        {
            logline(LOGF_MODULES, LOGL_WARNING,
                    "%s(devid=%d/active=%d): negative nelems[%d:%d]=%d",
                    __FUNCTION__, devid, active_devid, x, chan, nels);
            goto NEXT_TO_UPDATE;
        }
        else if (chn_p->max_nelems == 1  &&  nels != 1)
        {
            logline(LOGF_MODULES, LOGL_WARNING,
                    "%s(devid=%d/active=%d): nelems[%d:%d]=%d, should be =1 for scalar",
                    __FUNCTION__, devid, active_devid, x, chan, nels);
            goto NEXT_TO_UPDATE;
        }
        else if (nels > chn_p->max_nelems)
        {
            /* Too many?  Okay, just limit */
            logline(LOGF_MODULES, LOGL_INFO,
                    "%s(devid=%d/active=%d): nelems[%d:%d]=%d, truncating to limit=%d",
                    __FUNCTION__, devid, active_devid, x, chan, nels, chn_p->max_nelems);
            nels = chn_p->max_nelems;
        }

        /* Store data */
#if 1
        src = values[x];
        dst = chn_p->current_val;

#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
        if (chn_p->dtype == CXDTYPE_UNKNOWN)
        {
            chn_p->current_dtype = dtypes[x];
            chn_p->current_usize = sizeof_cxdtype(chn_p->current_dtype);
            /* Additional check for nelems, to prevent buffer overflow */
            /* Note:
                   1. We compare with just max_nelems, without *usize,
                      since usize==1.
                   2. We compare "nelems > max_nelems/current_usize"
                      instead of "nelems*current_usize > max_nelems"
                      because latter can result in integer overflow
                      (if nelems>MAX_SIZE_T/current_usize)
             */
            if (nels > chn_p->max_nelems / chn_p->current_usize)
                nels = chn_p->max_nelems / chn_p->current_usize;

            if (nels < 0) goto NEXT_TO_UPDATE;
        }
#endif

        srpr = reprof_cxdtype(dtypes[x]);
        ssiz = sizeof_cxdtype(dtypes[x]);
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
        repr = reprof_cxdtype(chn_p->current_dtype);
        size = sizeof_cxdtype(chn_p->current_dtype);
#else
        repr = reprof_cxdtype(chn_p->dtype);
        size = sizeof_cxdtype(chn_p->dtype);
#endif

        /* a. Identical */
        if      (srpr == repr  &&  ssiz == size)
        {
            chn_p->current_nelems = nels;
            if (nels > 0)
                memcpy(dst, src, size * nels);
        }
        /* b. Integer */
        else if (srpr == CXDTYPE_REPR_INT  &&  repr == CXDTYPE_REPR_INT)
        {
            chn_p->current_nelems = nels;
#if MAY_USE_INT64
            if (ssiz == sizeof(int64)  ||  size == sizeof(int64))
                while (nels > 0)
                {
                    // Read datum, converting to int64
                    switch (dtypes[x])
                    {
                        case CXDTYPE_INT32:  iv64 = *((  int32*)src);     break;
                        case CXDTYPE_UINT32: iv64 = *(( uint32*)src);     break;
                        case CXDTYPE_INT16:  iv64 = *((  int16*)src);     break;
                        case CXDTYPE_UINT16: iv64 = *(( uint16*)src);     break;
                        case CXDTYPE_INT64:
                        case CXDTYPE_UINT64: iv64 = *(( uint64*)src);     break;
                        case CXDTYPE_INT8:   iv64 = *((  int8 *)src);     break;
                        default:/*   UINT8*/ iv64 = *(( uint8 *)src);     break;
                    }
                    src += ssiz;

                    // Store datum, converting from int64
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
                    switch (chn_p->current_dtype)
#else
                    switch (chn_p->dtype)
#endif
                    {
                        case CXDTYPE_INT32:
                        case CXDTYPE_UINT32:      *((  int32*)dst) = iv64; break;
                        case CXDTYPE_INT16:
                        case CXDTYPE_UINT16:      *((  int16*)dst) = iv64; break;
                        case CXDTYPE_INT64:
                        case CXDTYPE_UINT64:      *(( uint64*)dst) = iv64; break;
                        default:/*   *INT8*/      *((  int8 *)dst) = iv64; break;
                    }
                    dst += size;

                    nels--;
                }
            else
#endif
            while (nels > 0)
            {
                // Read datum, converting to int32
                switch (dtypes[x])
                {
                    case CXDTYPE_INT32:
                    case CXDTYPE_UINT32: iv32 = *((  int32*)src);     break;
                    case CXDTYPE_INT16:  iv32 = *((  int16*)src);     break;
                    case CXDTYPE_UINT16: iv32 = *(( uint16*)src);     break;
                    case CXDTYPE_INT8:   iv32 = *((  int8 *)src);     break;
                    default:/*   UINT8*/ iv32 = *(( uint8 *)src);     break;
                }
                src += ssiz;

                // Store datum, converting from int32
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
                switch (chn_p->current_dtype)
#else
                switch (chn_p->dtype)
#endif
                {
                    case CXDTYPE_INT32:
                    case CXDTYPE_UINT32:      *((  int32*)dst) = iv32; break;
                    case CXDTYPE_INT16:
                    case CXDTYPE_UINT16:      *((  int16*)dst) = iv32; break;
                    default:/*   *INT8*/      *((  int8 *)dst) = iv32; break;
                }
                dst += size;

                nels--;
            }
        }
#if MAY_USE_FLOAT
        /* c. Float: float->float, float->int, int->float */
        else if ((srpr == CXDTYPE_REPR_FLOAT  ||  srpr == CXDTYPE_REPR_INT)  &&
                 (repr == CXDTYPE_REPR_FLOAT  ||  repr == CXDTYPE_REPR_INT))
        {
            chn_p->current_nelems = nels;
            while (nels > 0)
            {
                // Read datum, converting to float64 (double)
                switch (dtypes[x])
                {
                    case CXDTYPE_INT32:  fv64 = *((  int32*)src);     break;
                    case CXDTYPE_UINT32: fv64 = *(( uint32*)src);     break;
                    case CXDTYPE_INT16:  fv64 = *((  int16*)src);     break;
                    case CXDTYPE_UINT16: fv64 = *(( uint16*)src);     break;
                    case CXDTYPE_DOUBLE: fv64 = *((float64*)src);     break;
                    case CXDTYPE_SINGLE: fv64 = *((float32*)src);     break;
                    case CXDTYPE_INT64:  fv64 = *((  int64*)src);     break;
                    case CXDTYPE_UINT64: fv64 = *(( uint64*)src);     break;
                    case CXDTYPE_INT8:   fv64 = *((  int8 *)src);     break;
                    default:/*   UINT8*/ fv64 = *(( uint8 *)src);     break;
                }
                src += ssiz;

                // Store datum, converting from float64 (double)
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
                switch (chn_p->current_dtype)
#else
                switch (chn_p->dtype)
#endif
                {
                    case CXDTYPE_INT32:       *((  int32*)dst) = fv64; break;
                    case CXDTYPE_UINT32:      *(( uint32*)dst) = fv64; break;
                    case CXDTYPE_INT16:       *((  int16*)dst) = fv64; break;
                    case CXDTYPE_UINT16:      *(( uint16*)dst) = fv64; break;
                    case CXDTYPE_DOUBLE:      *((float64*)dst) = fv64; break;
                    case CXDTYPE_SINGLE:      *((float32*)dst) = fv64; break;
                    case CXDTYPE_INT64:       *((  int64*)dst) = fv64; break;
                    case CXDTYPE_UINT64:      *(( uint64*)dst) = fv64; break;
                    case CXDTYPE_INT8:        *((  int8 *)dst) = fv64; break;
                    default:/*   UINT8*/      *((  int8 *)dst) = fv64; break;
                }
                dst += size;

                nels--;
            }
        }
#endif
        /* d. Incompatible */
        else
        {
            chn_p->current_nelems = chn_p->max_nelems;
            /* Note "usize", NOT "current_usize"! */
            bzero(dst, chn_p->max_nelems * chn_p->usize);
        }
#else
        /*!!!DATACONV*/
        if (dtypes[x] != chn_p->dtype)
        {
            /* No data conversion for now... */
            chn_p->current_nelems = chn_p->nelems;
            bzero(chn_p->current_val, chn_p->nelems * chn_p->usize);
        }
        else
        {
            chn_p->current_nelems = nels;
            memcpy(chn_p->current_val, values[x], nels * chn_p->usize);
        }
#endif

        chn_p->crflags |= (chn_p->rflags = rflags[x]);
        /* Timestamp (with appropriate copying, of requested) */
        if (timestamps != NULL  /*&&  timestamps[x].sec != 0 OBSOLETED 02.03.2015 */  &&
            chn_p->timestamp_cn <= 0)
            chn_p->timestamp = timestamps[x];
        else
            chn_p->timestamp =
                (chn_p->timestamp_cn <= 0)? timestamp
                                          : cxsd_hw_channels[chn_p->timestamp_cn].timestamp;

        chn_p->upd_cycle = current_cycle;

        /* Drop "request sent" flags */
        chn_p->rd_req = chn_p->wr_req = 0;

 NEXT_TO_UPDATE:;
    }

    /*!!! Do "TryWrNext" */
    TryWrNext(devid, count, addrs);

    /* II. Call who requested */
    call_info.reason = CXSD_HW_CHAN_R_UPDATE;
    call_info.evmask = 1 << call_info.reason;

    for (x = 0;  x < count;  x++)
    {
        /* Get pointer to the channel in question */
        chan = addrs[x];
        if (!internal  &&
            (chan < 0  ||  chan >= dev->count))
            goto NEXT_TO_CALL;
        gcid = chan + dev->first;

        CxsdHwCallChanEvprocs(gcid, &call_info);
 NEXT_TO_CALL:;
    }
}

void ReturnInt32Datum (int devid, int chan, int32 v, rflags_t rflags)
{
  static cxdtype_t  dt_int32    = CXDTYPE_INT32;
  static int        nels_1      = 1;

  void             *vp          = &v;

    ReturnDataSet(devid, 1,
                  &chan, &dt_int32, &nels_1,
                  &vp, &rflags, NULL);
}

void ReturnInt32DatumTimed(int devid, int chan, int32 v,
                           rflags_t rflags, cx_time_t timestamp)
{
  static cxdtype_t  dt_int32    = CXDTYPE_INT32;
  static int        nels_1      = 1;

  void             *vp          = &v;

    ReturnDataSet(devid, 1,
                  &chan, &dt_int32, &nels_1,
                  &vp, &rflags, &timestamp);
}

void SetChanRDs       (int devid,
                       int first, int count,
                       double phys_r, double phys_d)
{
  cxsd_hw_dev_t  *dev = cxsd_hw_devices + devid;

  int             x;
  cxsd_hw_chan_t *chn_p;

  CxsdHwChanEvCallInfo_t  call_info;

    CHECK_SANITY_OF_DEVID();

    /* Check the `first' */
    if (first < 0  ||  first >= dev->count)
    {
        logline(LOGF_MODULES, LOGL_WARNING,
                "%s(devid=%d/active=%d): first=%d, out_of[0...dev->count=%d)",
                __FUNCTION__, devid, active_devid, first, dev->count);
        return;
    }

    /* Now check the `count' */
    if (count < 1  ||  count > dev->count - first)
    {
        logline(LOGF_MODULES, LOGL_WARNING,
                "%s:(devid=%d/active=%d) count=%d, out_of[1..%d] (first=%d, dev->count=%d)",
                __FUNCTION__, devid, active_devid, count, dev->count - first, first, dev->count);
        return;
    }

    for (x = 0, chn_p = cxsd_hw_channels + dev->first + first;
         x < count;
         x++, chn_p++)
    {
        chn_p->phys_rds[0]       = phys_r;
        chn_p->phys_rds[1]       = phys_d;
        chn_p->phys_rd_specified = 1;
    }

    /* II. Call who requested */
    call_info.reason = CXSD_HW_CHAN_R_RDSCHG;
    call_info.evmask = 1 << call_info.reason;

    for (x = 0;  x < count;  x++)
    {
        CxsdHwCallChanEvprocs(dev->first + first + x, &call_info);
    }
}

void SetChanFreshAge  (int devid,
                       int first, int count,
                       cx_time_t fresh_age)
{
  cxsd_hw_dev_t  *dev = cxsd_hw_devices + devid;

  int             x;
  cxsd_hw_chan_t *chn_p;

  CxsdHwChanEvCallInfo_t  call_info;

    CHECK_SANITY_OF_DEVID();

    /* Check the `first' */
    if (first < 0  ||  first >= dev->count)
    {
        logline(LOGF_MODULES, LOGL_WARNING,
                "%s(devid=%d/active=%d): first=%d, out_of[0...dev->count=%d)",
                __FUNCTION__, devid, active_devid, first, dev->count);
        return;
    }

    /* Now check the `count' */
    if (count < 1  ||  count > dev->count - first)
    {
        logline(LOGF_MODULES, LOGL_WARNING,
                "%s:(devid=%d/active=%d) count=%d, out_of[1..%d] (first=%d, dev->count=%d)",
                __FUNCTION__, devid, active_devid, count, dev->count - first, first, dev->count);
        return;
    }

    for (x = 0, chn_p = cxsd_hw_channels + dev->first + first;
         x < count;
         x++, chn_p++)
    {
        chn_p->fresh_age = fresh_age;
    }

    /* II. Call who requested */
    call_info.reason = CXSD_HW_CHAN_R_FRESHCHG;
    call_info.evmask = 1 << call_info.reason;

    for (x = 0;  x < count;  x++)
    {
        CxsdHwCallChanEvprocs(dev->first + first + x, &call_info);
    }
}

void SetChanQuant     (int devid,
                       int first, int count,
                       CxAnyVal_t q, cxdtype_t q_dtype)
{
  cxsd_hw_dev_t  *dev = cxsd_hw_devices + devid;

  int             x;
  cxsd_hw_chan_t *chn_p;

  CxsdHwChanEvCallInfo_t  call_info;

    CHECK_SANITY_OF_DEVID();

    /* Check the `first' */
    if (first < 0  ||  first >= dev->count)
    {
        logline(LOGF_MODULES, LOGL_WARNING,
                "%s(devid=%d/active=%d): first=%d, out_of[0...dev->count=%d)",
                __FUNCTION__, devid, active_devid, first, dev->count);
        return;
    }

    /* Now check the `count' */
    if (count < 1  ||  count > dev->count - first)
    {
        logline(LOGF_MODULES, LOGL_WARNING,
                "%s:(devid=%d/active=%d) count=%d, out_of[1..%d] (first=%d, dev->count=%d)",
                __FUNCTION__, devid, active_devid, count, dev->count - first, first, dev->count);
        return;
    }

    for (x = 0, chn_p = cxsd_hw_channels + dev->first + first;
         x < count;
         x++, chn_p++)
    {
        chn_p->q       = q;
        chn_p->q_dtype = q_dtype;
    }

    /* II. Call who requested */
    call_info.reason = CXSD_HW_CHAN_R_QUANTCHG;
    call_info.evmask = 1 << call_info.reason;

    for (x = 0;  x < count;  x++)
    {
        CxsdHwCallChanEvprocs(dev->first + first + x, &call_info);
    }
}

void SetChanRange     (int devid,
                       int first, int count,
                       CxAnyVal_t minv, CxAnyVal_t maxv, cxdtype_t range_dtype)
{
  cxsd_hw_dev_t  *dev = cxsd_hw_devices + devid;

  int             x;
  cxsd_hw_chan_t *chn_p;

  CxsdHwChanEvCallInfo_t  call_info;

    CHECK_SANITY_OF_DEVID();

    /* Check the `first' */
    if (first < 0  ||  first >= dev->count)
    {
        logline(LOGF_MODULES, LOGL_WARNING,
                "%s(devid=%d/active=%d): first=%d, out_of[0...dev->count=%d)",
                __FUNCTION__, devid, active_devid, first, dev->count);
        return;
    }

    /* Now check the `count' */
    if (count < 1  ||  count > dev->count - first)
    {
        logline(LOGF_MODULES, LOGL_WARNING,
                "%s:(devid=%d/active=%d) count=%d, out_of[1..%d] (first=%d, dev->count=%d)",
                __FUNCTION__, devid, active_devid, count, dev->count - first, first, dev->count);
        return;
    }

    for (x = 0, chn_p = cxsd_hw_channels + dev->first + first;
         x < count;
         x++, chn_p++)
    {
        chn_p->range[0]    = minv;
        chn_p->range[1]    = maxv;
        chn_p->range_dtype = range_dtype;
    }

    /* II. Call who requested */
    call_info.reason = CXSD_HW_CHAN_R_RANGECHG;
    call_info.evmask = 1 << call_info.reason;

    for (x = 0;  x < count;  x++)
    {
        CxsdHwCallChanEvprocs(dev->first + first + x, &call_info);
    }
}

void SetChanReturnType(int devid,
                       int first, int count,
                       int is_autoupdated)
{
  cxsd_hw_dev_t  *dev = cxsd_hw_devices + devid;

  int             x;
  cxsd_hw_chan_t *chn_p;

  CxsdHwChanEvCallInfo_t  call_info;

    CHECK_SANITY_OF_DEVID();

    /* Check the `first' */
    if (first < 0  ||  first >= dev->count)
    {
        logline(LOGF_MODULES, LOGL_WARNING,
                "%s(devid=%d/active=%d): first=%d, out_of[0...dev->count=%d)",
                __FUNCTION__, devid, active_devid, first, dev->count);
        return;
    }

    /* Now check the `count' */
    if (count < 1  ||  count > dev->count - first)
    {
        logline(LOGF_MODULES, LOGL_WARNING,
                "%s:(devid=%d/active=%d) count=%d, out_of[1..%d] (first=%d, dev->count=%d)",
                __FUNCTION__, devid, active_devid, count, dev->count - first, first, dev->count);
        return;
    }

    for (x = 0, chn_p = cxsd_hw_channels + dev->first + first;
         x < count;
         x++, chn_p++)
    {
        chn_p->is_autoupdated = is_autoupdated != 0;
        if (is_autoupdated == IS_AUTOUPDATED_TRUSTED)
            chn_p->fresh_age  = (cx_time_t){0,0};
    }

    if (is_autoupdated != IS_AUTOUPDATED_TRUSTED) return;
    /* II. Call who requested */
    call_info.reason = CXSD_HW_CHAN_R_FRESHCHG;
    call_info.evmask = 1 << call_info.reason;

    for (x = 0;  x < count;  x++)
    {
        CxsdHwCallChanEvprocs(dev->first + first + x, &call_info);
    }
}

//--------------------------------------------------------------------

static int chan_evproc_checker(cxsd_hw_chan_cbrec_t *p, void *privptr)
{
  cxsd_hw_chan_cbrec_t *model = privptr;

    return
        p->evmask   == model->evmask    &&
        p->uniq     == model->uniq      &&
        p->privptr1 == model->privptr1  &&
        p->evproc   == model->evproc    &&
        p->privptr2 == model->privptr2;
}

int  CxsdHwAddChanEvproc(int  uniq, void *privptr1,
                         cxsd_gchnid_t          gcid,
                         int                    evmask,
                         cxsd_hw_chan_evproc_t  evproc,
                         void                  *privptr2)
{
  cxsd_hw_chan_t       *chn_p = cxsd_hw_channels + gcid;
  cxsd_hw_chan_cbrec_t *p;
  int                   n;
  cxsd_hw_chan_cbrec_t  rec;

    if (CheckGcid(gcid) != 0) return -1;
    if (evmask == 0  ||
        evproc == NULL)       return 0;

    /* Check if it is already in the list */
    rec.evmask   = evmask;
    rec.uniq     = uniq;
    rec.privptr1 = privptr1;
    rec.evproc   = evproc;
    rec.privptr2 = privptr2;
    if (ForeachChanCbSlot(chan_evproc_checker, &rec, chn_p) >= 0) return 0;

    n = GetChanCbSlot(chn_p);
    if (n < 0) return -1;
    p = AccessChanCbSlot(n, chn_p);

    p->evmask   = evmask;
    p->uniq     = uniq;
    p->privptr1 = privptr1;
    p->evproc   = evproc;
    p->privptr2 = privptr2;

    return 0;
}

int  CxsdHwDelChanEvproc(int  uniq, void *privptr1,
                         cxsd_gchnid_t          gcid,
                         int                    evmask,
                         cxsd_hw_chan_evproc_t  evproc,
                         void                  *privptr2)
{
  cxsd_hw_chan_t       *chn_p = cxsd_hw_channels + gcid;
  int                   n;
  cxsd_hw_chan_cbrec_t  rec;

    if (CheckGcid(gcid) != 0) return -1;
    if (evmask == 0)          return 0;

    /* Find requested callback */
    rec.evmask   = evmask;
    rec.uniq     = uniq;
    rec.privptr1 = privptr1;
    rec.evproc   = evproc;
    rec.privptr2 = privptr2;
    n = ForeachChanCbSlot(chan_evproc_checker, &rec, chn_p);
    if (n < 0)
    {
        /* Not found! */
        errno = ENOENT;
        return -1;
    }

    RlsChanCbSlot(n, chn_p);

    return 0;
}


int  cxsd_uniq_checker(const char *func_name, int uniq)
{
  int                  devid = uniq;

    if (uniq == 0  ||  uniq == DEVID_NOT_IN_DRIVER) return 0;

    //DO_CHECK_SANITY_OF_DEVID(func_name, -1);

    return 0;
}


static int chan_evproc_cleanuper(cxsd_hw_chan_cbrec_t *p, void *privptr)
{
  int  uniq = ptr2lint(privptr);

    if (p->uniq == uniq)
        /*!!! should call RlsChanCbSlot() somehow, but it requires "id" instead of "p" */
        p->evmask = 0;

    return 0;
}
static int cycle_evproc_cleanuper(cycle_cbrec_t *p, void *privptr)
{
  int  uniq = ptr2lint(privptr);

    if (p->uniq == uniq)
        /*!!! should call RlsCycleCBSlot() somehow, but it requires "n" instead of "p" */
        p->in_use = 0;

    return 0;
}
void cxsd_hw_do_cleanup(int uniq)
{
  cxsd_gchnid_t  gcid;

    for (gcid = 0;  gcid < cxsd_hw_numchans;  gcid++)
        ForeachChanCbSlot(chan_evproc_cleanuper,
                          lint2ptr(uniq),
                          cxsd_hw_channels + gcid);

    ForeachCycleCBSlot(cycle_evproc_cleanuper, lint2ptr(uniq));
}
