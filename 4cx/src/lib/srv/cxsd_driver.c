#include "cxsd_lib_includes.h"


#ifndef MAY_USE_INT64
  #define MAY_USE_INT64 1
#endif
#ifndef MAY_USE_FLOAT
  #define MAY_USE_FLOAT 1
#endif


//////////////////////////////////////////////////////////////////////

void        DoDriverLog      (int devid, int level,
                              const char *format, ...)
{
  va_list        ap;
  
    va_start(ap, format);
    vDoDriverLog(devid, level, format, ap);
    va_end(ap);
}

void        vDoDriverLog     (int devid, int level,
                              const char *format, va_list ap)
{
#if 1
  int         log_type = LOGF_HARDWARE;
  char        subaddress[200];
  int         category = level & DRIVERLOG_C_mask;
  const char *catname;
  cxsd_hw_dev_t    *dev_p;
  cxsd_hw_lyr_t    *lyr_p;

    if (level & DRIVERLOG_ERRNO) log_type |= LOGF_ERRNO;

    if      (devid > 0  &&   devid < cxsd_hw_numdevs)
    {
        dev_p = cxsd_hw_devices + devid;
        if (category != DRIVERLOG_C_DEFAULT        &&
            category <= DRIVERLOG_C__max_allowed_  &&
            !DRIVERLOG_m_C_ISIN_CHECKMASK(category, dev_p->logmask))
            return;

        catname = GetDrvlogCatName(category);
        snprintf(subaddress, sizeof(subaddress),
                 "%s[%d]%s%s%s",
                 CxsdDbGetStr(cxsd_hw_cur_db, dev_p->db_ref->typename_ofs), 
                 devid, 
                 CxsdDbGetStr(cxsd_hw_cur_db, dev_p->db_ref->instname_ofs),
                 catname[0] != '\0'? "/" : "", catname);
    }
    else if (devid < 0  &&  -devid < cxsd_hw_numlyrs)
    {
        lyr_p = cxsd_hw_layers + -devid;
        if (category != DRIVERLOG_C_DEFAULT        &&
            category <= DRIVERLOG_C__max_allowed_  &&
            !DRIVERLOG_m_C_ISIN_CHECKMASK(category, lyr_p->logmask))
            return;

        catname = GetDrvlogCatName(category);
        snprintf(subaddress, sizeof(subaddress),
                 "lyr:%s[%d]%s%s",
                 CxsdDbGetStr(cxsd_hw_cur_db, lyr_p->lyrname_ofs),
                 devid,
                 catname[0] != '\0'? "/" : "", catname);
    }
    else /* DEVID_NOT_IN_DRIVER or some overflow/illegal... */
    {
        if (category != DRIVERLOG_C_DEFAULT        &&
            category <= DRIVERLOG_C__max_allowed_  &&
            !DRIVERLOG_m_C_ISIN_CHECKMASK(category, cxsd_hw_defdrvlog_mask))
            return;

        catname = GetDrvlogCatName(category);
        snprintf(subaddress, sizeof(subaddress),
                 "NOT-IN-DRIVER=%d%s%s",
                 devid, catname[0] != '\0'? "/" : "", catname);
    }

    vloglineX(log_type, level & DRIVERLOG_LEVEL_mask, subaddress, format, ap);
#else
    fprintf (stderr, "%s: ", strcurtime());
    vfprintf(stderr, format, ap);
    fprintf (stderr, "\n");
#endif
}


int         RegisterDevPtr   (int devid, void *devptr)
{
    CHECK_SANITY_OF_DEVID(-1);

    /*!!! Should we forbid replacing devptr when privrecsize>0?  */
    cxsd_hw_devices[devid].devptr = devptr;
    
    return 0;
}


void StdSimulated_rw_p(int devid, void *devptr __attribute__((unused)),
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  int              n;
  int              x;
  cxsd_gchnid_t    gcid;
  cxdtype_t        dt;
  void            *nvp;
  int              nel;
  int              cycle = CxsdHwGetCurCycle();
  
  struct
  {
      int8         i8;
      int16        i16;
      int32        i32;
      int64        i64;

      float        f32;
      double       f64;
  
      int8         t8;
      int32        t32;
  }                v;
  cx_time_t       *timestamp_p;

  static rflags_t  zero_rflags = 0;
  static cx_time_t never_read_timestamp = (cx_time_t){CX_TIME_SEC_NEVER_READ,0};

    if (devid < 0)
    {
        logline(LOGF_MODULES, LOGL_WARNING,
                "%s(devid=%d/active=%d): attempt to call as-if-from layer",
                __FUNCTION__, devid, active_devid);
        return;
    }
    CHECK_SANITY_OF_DEVID();

    for (n = 0;  n < count;  n++)
    {
        x = addrs[n];
        if (x < 0  ||  x >= cxsd_hw_devices[devid].count)
        {
            /*!!! Bark?*/
            continue;
        }
        gcid = x + cxsd_hw_devices[devid].first;

        bzero(&v, sizeof(v));

        
        if (action == DRVA_WRITE) dt = dtypes[n];
        else                      dt = cxsd_hw_channels[gcid].dtype;
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
        if (dt == CXDTYPE_UNKNOWN)
        {
            if      (cxsd_hw_channels[gcid].max_nelems < 2) dt = CXDTYPE_INT8;
            else if (cxsd_hw_channels[gcid].max_nelems < 4) dt = CXDTYPE_INT16;
            else                                            dt = CXDTYPE_INT32;
        }
#endif
        switch (dt)
        {
            case CXDTYPE_INT8:   case CXDTYPE_UINT8:  nvp = &v.i8;  break;
            case CXDTYPE_INT16:  case CXDTYPE_UINT16: nvp = &v.i16; break;
            case CXDTYPE_INT32:  case CXDTYPE_UINT32: nvp = &v.i32; break;
            case CXDTYPE_INT64:  case CXDTYPE_UINT64: nvp = &v.i64; break;
            case CXDTYPE_SINGLE:                      nvp = &v.f32; break;
            case CXDTYPE_DOUBLE:                      nvp = &v.f64; break;
            case CXDTYPE_TEXT:                        nvp = &v.t8;  break;
            case CXDTYPE_UCTEXT:                      nvp = &v.t32; break;
            default:
                logline(LOGF_MODULES, LOGL_ERR,
                        "%s(devid=%d/active=%d): unrecognized dtypes[n=%d/chan=%d/global=%d]=%d",
                        __FUNCTION__, devid, active_devid, n, x, gcid, dt);
                return;
        }

        timestamp_p = NULL;
        if      (action == DRVA_WRITE  &&  cxsd_hw_channels[gcid].rw)
        {
            nvp = values[n];
            nel = nelems[n];
        }
        else if (cxsd_hw_channels[gcid].rw)
        {
            /* That must be initial read-of-w-channels.
               We can't do anything with it, just init with 0. */
            nvp = cxsd_hw_channels[gcid].current_val;
            nel = cxsd_hw_channels[gcid].current_nelems;
            timestamp_p = &never_read_timestamp;
            /* 15.04.2016:
               Unfortunately, NEVER_READ presents problems when combined with
               cxsd_fe_cx.c's "send CURVAL for NEVER_READ rw channels instead
               of NEWVAL".
               Thus, for now we switch to returning just current time...  */
            timestamp_p = NULL;
        }
        else   /* Read of a readonly channel */
        {
            v.i8  = gcid        + cycle;
            v.i16 = gcid * 10   + cycle;
            v.i32 = gcid * 1000 + cycle;
#if MAY_USE_INT64
            v.i64 = gcid * 5000 + cycle;
#endif
#if MAY_USE_FLOAT
            v.f32 = gcid + cycle / 1000;
            v.f64 = gcid + cycle / 1000;
#endif
            v.t8  = 'a' + ((gcid + cycle) % 26);
            v.t32 = 'A' + ((gcid + cycle) % 26);

            nel = 1;
        }

        ReturnDataSet(devid,
                      1,
                      &x, &dt, &nel,
                      &nvp, &zero_rflags, timestamp_p);
    }
}

const char * GetDevTypename(int devid)
{
  cxsd_hw_dev_t  *dev_p = cxsd_hw_devices + devid;

    CHECK_SANITY_OF_DEVID(NULL);

    return CxsdDbGetStr(cxsd_hw_cur_db, dev_p->db_ref->typename_ofs);
}

void         GetDevLogPrms (int devid, int *curlevel_p, int *curmask_p)
{
    CHECK_SANITY_OF_DEVID();

    if (curlevel_p != NULL) *curlevel_p = cxsd_logger_verbosity;
    if (curmask_p  != NULL) *curmask_p  = cxsd_hw_devices[devid].logmask;
}

void       * GetLayerVMT   (int devid)
{
  cxsd_hw_dev_t  *dev_p = cxsd_hw_devices + devid;

    CHECK_SANITY_OF_DEVID(NULL);

    if (dev_p->lyrid == 0)
    {
        logline(LOGF_MODULES, LOGL_ERR,
                "%s(devid=%d/active=%d): request for layer-VMT from non-layered device",
                __FUNCTION__, devid, active_devid);
        return NULL;
    }

    return cxsd_hw_layers[-dev_p->lyrid].metric->vmtlink;
}

const char * GetLayerInfo  (const char *lyrname, int bus_n)
{
  const char        *ret = NULL;
  int                n;
  CxsdDbLayerinfo_t *lio;
  const char        *name;

    for (n = 0,  lio = cxsd_hw_cur_db->liolist;
         n < cxsd_hw_cur_db->numlios;
         n++,    lio++)
        if ((name = CxsdDbGetStr(cxsd_hw_cur_db, lio->lyrname_ofs)) != NULL  &&
            strcasecmp(lyrname, name) == 0)
        {
            if (bus_n == lio->bus_n
                ||
                (lio->bus_n < 0  && ret == NULL))
                ret = CxsdDbGetStr(cxsd_hw_cur_db, lio->lyrinfo_ofs);
        }

    return ret;
}
