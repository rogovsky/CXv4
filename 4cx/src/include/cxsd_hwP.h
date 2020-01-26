#ifndef __CXSD_HWP_H
#define __CXSD_HWP_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "cxsd_driver.h"
#include "cxsd_db.h"
#include "cxsd_dbP.h"
#include "cxsd_hw.h"


#if defined(D) || defined(V)
  #error Sorry, unable to compile when D or V defined
  #error (see warning(s) below for details).
#endif

#ifdef __CXSD_HW_C
  #define D
  #define V(value...) = value
#else
  #define D extern
  #define V(value...)
#endif /* __CXSD_HW_C */


/* Types */

typedef struct
{
    int                  state;         // One of DEVSTATE_NNN
    int                  logmask;       // Mask for logging of various classes of events
    int                  is_simulated;  // Per-device simulation flag
    struct timeval       stattime;      // Time of last state change
    rflags_t             statrflags;    // Flags of current state/error

    /* Properties */
    CxsdDbDevLine_t     *db_ref;        // Its line in cxsd_hw_cur_db[]
    CxsdDriverModRec    *metric;        // Driver's metric
    int                  lyrid;         // ID of layer serving this driver
    cxsd_gchnid_t        first;         // # of first channel
    int                  count;         // Total # of channels
    int                  wauxcount;     // =count+CXSD_DB_AUX_CHANCOUNT

    /* Driver-support data */
    void                *devptr;        // Device's private pointer
} cxsd_hw_dev_t;

typedef struct
{
    int                  active;
    int                  logmask;       // Mask for logging of various classes of events

    int                  lyrname_ofs;
    CxsdLayerModRec     *metric;
} cxsd_hw_lyr_t;

typedef void (*cxsd_hw_chan_evproc_t)(int            uniq,
                                      void          *privptr1,
                                      cxsd_gchnid_t  gcid,
                                      int            reason,
                                      void          *privptr2);
typedef struct
{
    int                    evmask;
    int                    uniq;
    void                  *privptr1;
    cxsd_hw_chan_evproc_t  evproc;
    void                  *privptr2;
} cxsd_hw_chan_cbrec_t;
typedef struct
{
    int8            rw;                // 0 -- readonly, 1 -- read/write
    int8            rd_req;            // Read request was sent but yet unserved
    int8            wr_req;            // Write request was sent but yet unserved
    int8            next_wr_val_pnd;   // Value for next write is pending

    int8            is_autoupdated;
    int8            is_internal;
    int8            do_ignore_upd_cycle;
    int8            res3;

    int             devid;             // "Backreference" -- device this channel belongs to
    cxsd_gchnid_t   boss;              // if >= 0
    cxdtype_t       dtype;             // Data type
    int             max_nelems;        // Max # of units
    size_t          usize;             // Size of 1 unit, =sizeof_cxdtype(dtype)

    int             dcpr_id;

    int             chancol;           // !!!"Color" of this channel (default=0)

    void           *current_val;       // Pointer to current-value...
    int             current_nelems;    // ...and # of units in it
    void           *next_wr_val;       // Pointer to next-value-to-write
    int             next_wr_nelems;    // ...and # of units in it
    rflags_t        rflags;            // Result flags
    rflags_t        crflags;           // Accumulated/cumulative result flags
    cx_time_t       timestamp;         // !!!
    cxsd_gchnid_t   timestamp_cn;
    int             upd_cycle;         // Value of current_cycle when this channel was last updated
#ifndef CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
  #define CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN 1
#endif
#if CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN
    cxdtype_t       current_dtype;     // Current data type (is useful if .dtype==CXDTYPE_UNKNOWN)
    size_t          current_usize;     // =sizeof_cxdtype(current_dtype)
#endif /* CXSD_HW_SUPPORTS_CXDTYPE_UNKNOWN */

    int             locker;

    cx_time_t       fresh_age;
    int             phys_rd_specified;
    double          phys_rds[2];

    CxAnyVal_t      q;
    cxdtype_t       q_dtype;

    CxAnyVal_t      range[2];
    cxdtype_t       range_dtype;

    cxsd_hw_chan_cbrec_t *cb_list;
    int             cb_list_allocd;
} cxsd_hw_chan_t;


/* Current-HW-database */

D CxsdDb          cxsd_hw_cur_db;

// Counts
D int             cxsd_hw_numlyrs  V(0);
D int             cxsd_hw_numdevs  V(0);
D int             cxsd_hw_numchans V(0);

// Buffers
D uint8          *cxsd_hw_buffers  V(NULL);
D size_t          cxsd_hw_buf_size V(0);
D cxsd_hw_lyr_t  *cxsd_hw_layers   V(0);
D cxsd_hw_dev_t  *cxsd_hw_devices  V(0);
D cxsd_hw_chan_t *cxsd_hw_channels V(0);
D uint8          *cxsd_hw_current_val_buf V(NULL);
D uint8          *cxsd_hw_next_wr_val_buf V(NULL);

D int             cxsd_hw_defdrvlog_mask V(0);


enum
{
    CXSD_HW_CYCLE_R_BEGIN = 0,
    CXSD_HW_CYCLE_R_END   = 1,
};

typedef void (*cxsd_hw_cycle_evproc_t)(int   uniq,
                                       void *privptr1,
                                       int   reason,
                                       void *privptr2);

int  CxsdHwAddCycleEvproc(int  uniq, void *privptr1,
                          cxsd_hw_cycle_evproc_t  evproc,
                          void                   *privptr2);
int  CxsdHwDelCycleEvproc(int  uniq, void *privptr1,
                          cxsd_hw_cycle_evproc_t  evproc,
                          void                   *privptr2);


/* Inserver API */

enum
{
    CXSD_HW_CHAN_R_UPDATE   = 0,  CXSD_HW_CHAN_EVMASK_UPDATE   = 1 << CXSD_HW_CHAN_R_UPDATE,
    CXSD_HW_CHAN_R_STATCHG  = 1,  CXSD_HW_CHAN_EVMASK_STATCHG  = 1 << CXSD_HW_CHAN_R_STATCHG,
    CXSD_HW_CHAN_R_STRSCHG  = 2,  CXSD_HW_CHAN_EVMASK_STRSCHG  = 1 << CXSD_HW_CHAN_R_STRSCHG,
    CXSD_HW_CHAN_R_RDSCHG   = 3,  CXSD_HW_CHAN_EVMASK_RDSCHG   = 1 << CXSD_HW_CHAN_R_RDSCHG,
    CXSD_HW_CHAN_R_FRESHCHG = 4,  CXSD_HW_CHAN_EVMASK_FRESHCHG = 1 << CXSD_HW_CHAN_R_FRESHCHG,
    CXSD_HW_CHAN_R_QUANTCHG = 5,  CXSD_HW_CHAN_EVMASK_QUANTCHG = 1 << CXSD_HW_CHAN_R_QUANTCHG,
    CXSD_HW_CHAN_R_RANGECHG = 6,  CXSD_HW_CHAN_EVMASK_RANGECHG = 1 << CXSD_HW_CHAN_R_RANGECHG,
};

enum {CXSD_HW_DRVA_IGNORE_UPD_CYCLE_FLAG = 1 << 16};


int32  CxsdHwCreateClientID(void);
int    CxsdHwDeleteClientID(int ID);


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
                                 char         **dpyfmt_p);
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
                                 char         **dpyfmt_p);
int            CxsdHwGetChanType(cxsd_gchnid_t  gcid,
                                 int           *is_rw_p,
                                 cxdtype_t     *dtype_p,
                                 int           *max_nelems_p);
int            CxsdHwGetDevPlace(int            devid,
                                 int           *first_p,
                                 int           *count_p);

/* Note: CxsdHwDoIO MAY modify contents of dtypes[], nelems[] and values[] */
int  CxsdHwDoIO         (int  requester,
                         int  action,
                         int  count, cxsd_gchnid_t *gcids,
                         cxdtype_t *dtypes, int *nelems, void **values);
int  CxsdHwLockChannels (int  requester,
                         int  count, cxsd_gchnid_t *gcids,
                         int  operation);

int  CxsdHwAddChanEvproc(int  uniq, void *privptr1,
                         cxsd_gchnid_t          gcid,
                         int                    evmask,
                         cxsd_hw_chan_evproc_t  evproc,
                         void                  *privptr2);
int  CxsdHwDelChanEvproc(int  uniq, void *privptr1,
                         cxsd_gchnid_t          gcid,
                         int                    evmask,
                         cxsd_hw_chan_evproc_t  evproc,
                         void                  *privptr2);


/* A little bit of macros for interaction with drivers */

D int32 active_devid V(DEVID_NOT_IN_DRIVER);

#define ENTER_DRIVER_S(devid, s) do {s = active_devid; active_devid = devid;} while (0)
#define LEAVE_DRIVER_S(s)        do {active_devid = s;} while (0)


#define DO_CHECK_POSITIVE_SANITY(func_name, errret)                          \
    if      (devid == 0)                                                     \
    {                                                                        \
        logline(LOGF_MODULES, DRIVERLOG_WARNING, "%s(devid=%d/active=%d): devid==0", \
                func_name, devid, active_devid);                             \
        return errret;                                                       \
    }                                                                        \
    else if (devid > 0)                                                      \
    {                                                                        \
        if (devid >= cxsd_hw_numdevs)                                        \
        {                                                                    \
            logline(LOGF_MODULES, DRIVERLOG_WARNING, "%s(devid=%d/active=%d): devid out of [1...numdevs=%d)", \
                    func_name, devid, active_devid, cxsd_hw_numdevs);        \
            return errret;                                                   \
        }                                                                    \
        if (cxsd_hw_devices[devid].state == DEVSTATE_OFFLINE)                \
        {                                                                    \
            logline(LOGF_MODULES, DRIVERLOG_WARNING, "%s(devid=%d/active=%d): is OFFLINE", \
                func_name, devid, active_devid);                             \
            return errret;                                                   \
        }                                                                    \
    }

#define DO_CHECK_SANITY_OF_DEVID(func_name, errret)                          \
    CHECK_POSITIVE_SANITY(errret)                                            \
    else /*  devid < 0 */                                                    \
    {                                                                        \
        logline(LOGF_MODULES, DRIVERLOG_WARNING, "%s(devid=%d/active=%d): operation not supported for layers", \
                func_name, devid, active_devid);                             \
        return errret;                                                       \
    }

#define DO_CHECK_SANITY_OF_DEVID_WO_STATE(func_name, errret)                          \
    if      (devid == 0)                                                     \
    {                                                                        \
        logline(LOGF_MODULES, DRIVERLOG_WARNING, "%s(devid=%d/active=%d): devid==0", \
                func_name, devid, active_devid);                             \
        return errret;                                                       \
    }                                                                        \
    else if (devid > 0)                                                      \
    {                                                                        \
        if (devid >= cxsd_hw_numdevs)                                        \
        {                                                                    \
            logline(LOGF_MODULES, DRIVERLOG_WARNING, "%s(devid=%d/active=%d): devid out of [1...numdevs=%d)", \
                    func_name, devid, active_devid, cxsd_hw_numdevs);        \
            return errret;                                                   \
        }                                                                    \
    }                                                                        \
    else /*  devid < 0 */                                                    \
    {                                                                        \
        logline(LOGF_MODULES, DRIVERLOG_WARNING, "%s(devid=%d/active=%d): operation not supported for layers", \
                func_name, devid, active_devid);                             \
        return errret;                                                       \
    }

#define CHECK_POSITIVE_SANITY(errret) \
    DO_CHECK_POSITIVE_SANITY(__FUNCTION__, errret)
#define CHECK_SANITY_OF_DEVID(errret) \
    DO_CHECK_SANITY_OF_DEVID(__FUNCTION__, errret)
#define CHECK_SANITY_OF_DEVID_WO_STATE(errret) \
    DO_CHECK_SANITY_OF_DEVID_WO_STATE(__FUNCTION__, errret)


#undef D
#undef V


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CXSD_HWP_H */
