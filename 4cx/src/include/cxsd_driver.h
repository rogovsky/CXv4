#ifndef __CXSD_DRIVER_H
#define __CXSD_DRIVER_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <sys/types.h>
#include <sys/socket.h>

#include "cx.h"
#include "cx_version.h"
#include "cx_module.h"

#include "misc_macros.h"
#include "misclib.h"
#include "paramstr_parser.h"
#include "cxscheduler.h"
#include "fdiolib.h"


/*** General definitions ********************************************/

#define CXSD_DRIVER_MODREC_SUFFIX     _driver_modrec
#define CXSD_DRIVER_MODREC_SUFFIX_STR __CX_STRINGIZE(CXSD_DRIVER_MODREC_SUFFIX)

enum {CXSD_DRIVER_MODREC_MAGIC   = 0x12345678};
enum {
    CXSD_DRIVER_MODREC_VERSION_MAJOR = 10,
    CXSD_DRIVER_MODREC_VERSION_MINOR = 0,
    CXSD_DRIVER_MODREC_VERSION = CX_ENCODE_VERSION(CXSD_DRIVER_MODREC_VERSION_MAJOR,
                                                   CXSD_DRIVER_MODREC_VERSION_MINOR)
};


#define CXSD_LAYER_MODREC_SUFFIX     _layer_modrec
#define CXSD_LAYER_MODREC_SUFFIX_STR __CX_STRINGIZE(CXSD_LAYER_MODREC_SUFFIX)

enum {CXSD_LAYER_MODREC_MAGIC   = 0x7279614c}; /* Little-endian 'Layr' */
enum
{
    CXSD_LAYER_MODREC_VERSION_MAJOR = 2,
    CXSD_LAYER_MODREC_VERSION_MINOR = 0,
    CXSD_LAYER_MODREC_VERSION = CX_ENCODE_VERSION(CXSD_LAYER_MODREC_VERSION_MAJOR,
                                                  CXSD_LAYER_MODREC_VERSION_MINOR)
};


enum {DEVID_NOT_IN_DRIVER = 0};

enum
{
    DEVSTATE_OFFLINE   = -1,
    DEVSTATE_NOTREADY  =  0,
    DEVSTATE_OPERATING = +1
};

enum
{
    DRVA_READ  = 1,
    DRVA_WRITE = 2
};

enum
{
    RETURNDATA_COUNT_PONG = 0
};

enum
{
    IS_AUTOUPDATED_NOT     = 0,
    IS_AUTOUPDATED_YES     = 1,
    IS_AUTOUPDATED_TRUSTED = 2,
    IS_AUTOUPDATED_unused3 = 3,
    DO_IGNORE_UPD_CYCLE    = 4,
};


/*** Drivers'-provided interface definition *************************/

/*** a. Layers ******************************************************/

typedef int  (*CxsdLayerInitProc)(int lyrid);
typedef void (*CxsdLayerTermProc)(void);
typedef void (*CxsdLayerDiscProc)(int devid);

typedef struct
{
    cx_module_rec_t    mr;

    const char        *api_name;
    int                api_version;

    CxsdLayerInitProc  init_lyr;
    CxsdLayerTermProc  term_lyr;
    CxsdLayerDiscProc  disconnect;
    void              *vmtlink;
} CxsdLayerModRec;

#define CXSD_LAYER_MODREC_NAME(name) \
    __CX_CONCATENATE(name,CXSD_LAYER_MODREC_SUFFIX)

#define DECLARE_CXSD_LAYER(name) \
    CxsdLayerModRec CXSD_LAYER_MODREC_NAME(name)

#define DEFINE_CXSD_LAYER(name, comment,                             \
                          init_m, term_m,                            \
                          api_name, api_version,                     \
                          init_l, term_l,                            \
                          disconnect_p,                              \
                          vmt)                                       \
    CxsdLayerModRec CXSD_LAYER_MODREC_NAME(name) =                   \
    {                                                                \
        {                                                            \
            CXSD_LAYER_MODREC_MAGIC, CXSD_LAYER_MODREC_VERSION,      \
            __CX_STRINGIZE(name), comment,                           \
            init_m, term_m                                           \
        },                                                           \
        api_name, api_version,                                       \
        init_l, term_l,                                              \
        disconnect_p,                                                \
        vmt                                                          \
    }


/*** b. Drivers *****************************************************/

typedef int  (*CxsdDevInitFunc)(int devid, void *devptr,
                                int businfocount, int businfo[],
                                const char *auxinfo);
typedef void (*CxsdDevTermProc)(int devid, void *devptr);

typedef void (*CxsdDevChanProc)(int devid, void *devptr,
                                int action,
                                int count, int *addrs,
                                cxdtype_t *dtypes, int *nelems, void **values);

typedef struct
{
    int        count;       // # of channels in this segment
    int        rw;          // their kind (0-ro, 1-rw)
    cxdtype_t  dtype;       // data type
    int        max_nelems;  // Max # of units
} CxsdChanInfoRec;

typedef struct
{
    const char *name;
    int         n;
} CxsdChanNameRec;

// Dynamic-conmmands-related stuff
typedef enum
{
    CXSD_DEV_PARAM_TYPE_STRING = 1,
    CXSD_DEV_PARAM_TYPE_INT    = 2,
    CXSD_DEV_PARAM_TYPE_DOUBLE = 3,
} CxsdDevCmdParamType;

typedef struct
{
    CxsdDevCmdParamType  type;
    union {
        const char *s;
        int         i;
        double      d;
    }                    v;
} CxsdDevCmdParam;

typedef int  (*CxsdDevCmdFunc) (int devid, void *devptr,
                                const char *command,
                                int param_count, CxsdDevCmdParam params[]);

typedef struct
{
    const char     *cmdname;
    const char     *argsdescr;
    CxsdDevCmdFunc  processer;
} CxsdDevCmdDescr;

//
typedef struct
{
    const char *type;
    void       *data;
} CxsdDevExtRef;

typedef struct
{
    cx_module_rec_t   mr;

    size_t            privrecsize;
    psp_paramdescr_t *paramtable;

    int               min_businfo_n;
    int               max_businfo_n;

    const char       *layer;
    int               layerver;

    CxsdDevExtRef    *extensions;

    int               chan_nsegs;
    CxsdChanInfoRec  *chan_info;
    CxsdChanNameRec  *chan_namespace;

    //CxsdDevCmdDescr  *cmd_table;

    CxsdDevInitFunc   init_dev;
    CxsdDevTermProc   term_dev;
    CxsdDevChanProc   do_rw;
} CxsdDriverModRec;


#define CXSD_DRIVER_MODREC_NAME(name) \
    __CX_CONCATENATE(name,CXSD_DRIVER_MODREC_SUFFIX)

#define DECLARE_CXSD_DRIVER(name) \
    CxsdDriverModRec CXSD_DRIVER_MODREC_NAME(name)

#ifdef TRUE_DEFINE_DRIVER_H_FILE

#include TRUE_DEFINE_DRIVER_H_FILE

#else

#define DEFINE_CXSD_DRIVER(name, comment,                            \
                           init_mod, term_mod,                       \
                           privrecsize, paramtable,                  \
                           min_businfo_n, max_businfo_n,             \
                           layer, layerver,                          \
                           extensions,                               \
                           chan_nsegs, chan_info, chan_namespace,    \
                           init_dev, term_dev, rdwr_p)               \
    CxsdDriverModRec CXSD_DRIVER_MODREC_NAME(name) =                 \
    {                                                                \
        {                                                            \
            CXSD_DRIVER_MODREC_MAGIC, CXSD_DRIVER_MODREC_VERSION,    \
            __CX_STRINGIZE(name), comment,                           \
            init_mod, term_mod                                       \
        },                                                           \
        privrecsize, paramtable,                                     \
        min_businfo_n, max_businfo_n,                                \
        layer, layerver,                                             \
        extensions,                                                  \
        chan_nsegs, chan_info, chan_namespace,                       \
        init_dev, term_dev,                                          \
        rdwr_p                                                       \
    }

#endif


/*** Server API for drivers *****************************************/

/* State */

void        SetDevState      (int devid, int state,
                              rflags_t rflags_to_set, const char *description);
void        ReRequestDevData (int devid);


/* OS-like functionality */

enum
{
    DRIVERLOG_LEVEL_mask = (1 << 14) - 1,
    DRIVERLOG_ERRNO      = (1 << 14),

    /* Note: these are unified with LOGL_*, which are in turn unified with syslog()'s  LOG_* */
    DRIVERLOG_EMERG   = 0,
    DRIVERLOG_ALERT   = 1,
    DRIVERLOG_CRIT    = 2,
    DRIVERLOG_ERR     = 3,
    DRIVERLOG_WARNING = 4,
    DRIVERLOG_NOTICE  = 5,
    DRIVERLOG_INFO    = 6,
    DRIVERLOG_DEBUG   = 7,

    DRIVERLOG_C_shift              = 16, DRIVERLOG_C_mask              = 0xFFFF                         << DRIVERLOG_C_shift,
    /* Note: all _CN_ enums must be kept in sync with cxsd_drvmgr.c::catlist[] */
    DRIVERLOG_CN_default           = 0,  DRIVERLOG_C_DEFAULT           = DRIVERLOG_CN_default           << DRIVERLOG_C_shift,
    DRIVERLOG_CN_entrypoint        = 1,  DRIVERLOG_C_ENTRYPOINT        = DRIVERLOG_CN_entrypoint        << DRIVERLOG_C_shift,
    DRIVERLOG_CN_pktdump           = 2,  DRIVERLOG_C_PKTDUMP           = DRIVERLOG_CN_pktdump           << DRIVERLOG_C_shift,
    DRIVERLOG_CN_pktinfo           = 3,  DRIVERLOG_C_PKTINFO           = DRIVERLOG_CN_pktinfo           << DRIVERLOG_C_shift,
    DRIVERLOG_CN_dataconv          = 4,  DRIVERLOG_C_DATACONV          = DRIVERLOG_CN_dataconv          << DRIVERLOG_C_shift,
    DRIVERLOG_CN_remdrv_pktdump    = 5,  DRIVERLOG_C_REMDRV_PKTDUMP    = DRIVERLOG_CN_remdrv_pktdump    << DRIVERLOG_C_shift,
    DRIVERLOG_CN_remdrv_pktinfo    = 6,  DRIVERLOG_C_REMDRV_PKTINFO    = DRIVERLOG_CN_remdrv_pktinfo    << DRIVERLOG_C_shift,
    DRIVERLOG_CN__max_allowed_     = 15, DRIVERLOG_C__max_allowed_     = DRIVERLOG_CN__max_allowed_     << DRIVERLOG_C_shift,
};

#define DRIVERLOG_m_C_TO_CHECKMASK(  C)       (1 << ((C) >> DRIVERLOG_C_shift))
#define DRIVERLOG_m_C_ISIN_CHECKMASK(C, mask) ((mask & DRIVERLOG_m_C_TO_CHECKMASK(C)) != 0)
#define DRIVERLOG_m_CHECKMASK_ALL             -1


void        DoDriverLog      (int devid, int level,
                              const char *format, ...)
                              __attribute__ ((format (printf, 3, 4)));
void        vDoDriverLog     (int devid, int level,
                              const char *format, va_list ap);

int         RegisterDevPtr   (int devid, void *devptr);


/* Data */

void ReturnDataSet    (int devid,
                       int count,
                       int *addrs, cxdtype_t *dtypes, int *nelems,
                       void **values, rflags_t *rflags, cx_time_t *timestamps);
void ReturnInt32Datum (int devid, int chan, int32 v, rflags_t rflags);
void ReturnInt32DatumTimed(int devid, int chan, int32 v,
                           rflags_t rflags, cx_time_t timestamp);


void SetChanRDs       (int devid,
                       int first, int count,
                       double phys_r, double phys_d);
void SetChanFreshAge  (int devid,
                       int first, int count,
                       cx_time_t fresh_age);
void SetChanQuant     (int devid,
                       int first, int count,
                       CxAnyVal_t q, cxdtype_t q_dtype);
void SetChanRange     (int devid,
                       int first, int count,
                       CxAnyVal_t minv, CxAnyVal_t maxv, cxdtype_t range_dtype);
void SetChanReturnType(int devid,
                       int first, int count,
                       int return_type);


void StdSimulated_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values);


const char * GetDevTypename(int devid);
void         GetDevLogPrms (int devid, int *curlevel_p, int *curmask_p);

void       * GetLayerVMT   (int devid);
const char * GetLayerInfo  (const char *lyrname, int bus_n);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CXSD_DRIVER_H */
