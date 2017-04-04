#ifndef __PZFRAME_DRV_H
#define __PZFRAME_DRV_H


#include "cxsd_driver.h"


enum
{
    PZFRAME_CHTYPE_UNSUPPORTED = 0,
    PZFRAME_CHTYPE_BIGC        = 1,
    PZFRAME_CHTYPE_PZFRAME_STD = 2,
    PZFRAME_CHTYPE_INDIVIDUAL  = 3,
    PZFRAME_CHTYPE_VALIDATE    = 4,
    PZFRAME_CHTYPE_STATUS      = 5,
    PZFRAME_CHTYPE_AUTOUPDATED = 6,
    PZFRAME_CHTYPE_MARKER      = 7,
};

typedef struct
{
    int  chtype;
    int  refchn;
} pzframe_chinfo_t;


enum
{
    PZFRAME_R_READY = +1,
    PZFRAME_R_DOING =  0,
    PZFRAME_R_ERROR = -1,
    PZFRAME_R_IGNOR = -1, // For now -- the same as ERROR
};


struct _pzframe_drv_t_struct;

typedef int       (*pzframe_start_measurements_t)(struct _pzframe_drv_t_struct *pdr);
typedef int       (*pzframe_trggr_measurements_t)(struct _pzframe_drv_t_struct *pdr);
typedef int       (*pzframe_abort_measurements_t)(struct _pzframe_drv_t_struct *pdr);
typedef rflags_t  (*pzframe_read_measurements_t) (struct _pzframe_drv_t_struct *pdr);
typedef void      (*pzframe_prepare_retbufs_t)   (struct _pzframe_drv_t_struct *pdr,
                                                  rflags_t add_rflags);

typedef struct
{
    int         count;
    int        *addrs;
    cxdtype_t  *dtypes;
    int        *nelems;
    void      **values;
    rflags_t   *rflags;
    cx_time_t  *timestamps;
} pzframe_retbufs_t;

typedef struct _pzframe_drv_t_struct
{
    int                           devid;

    int                           param_shot;
    int                           param_istart;
    int                           param_waittime;
    int                           param_stop;
    int                           param_elapsed;

    pzframe_start_measurements_t  start_measurements;
    pzframe_trggr_measurements_t  trggr_measurements;
    pzframe_abort_measurements_t  abort_measurements;
    pzframe_read_measurements_t   read_measurements;
    pzframe_prepare_retbufs_t     prepare_retbufs;

    pzframe_retbufs_t             retbufs;

    int                           state; // Opaque -- used by pzframe_drv internally
    int                           measuring_now;
    sl_tid_t                      tid;
    struct timeval                measurement_start;
    int32                         value_istart;
    int32                         value_waittime;
} pzframe_drv_t;


void  pzframe_drv_init(pzframe_drv_t *pdr, int devid,
                       int                           param_shot,
                       int                           param_istart,
                       int                           param_waittime,
                       int                           param_stop,
                       int                           param_elapsed,
                       pzframe_start_measurements_t  start_measurements,
                       pzframe_trggr_measurements_t  trggr_measurements,
                       pzframe_abort_measurements_t  abort_measurements,
                       pzframe_read_measurements_t   read_measurements,
                       pzframe_prepare_retbufs_t     prepare_retbufs);
void  pzframe_drv_term(pzframe_drv_t *pdr);

void  pzframe_drv_rw_p   (pzframe_drv_t *pdr,
                          int chan, int32 val, int action);
void  pzframe_drv_req_mes(pzframe_drv_t *pdr);
void  pzframe_drv_drdy_p (pzframe_drv_t *pdr, int do_return, rflags_t rflags);


#endif /* __PZFRAME_DRV_H */
