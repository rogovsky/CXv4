#ifndef __CXSD_FRONTEND_H
#define __CXSD_FRONTEND_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "cx.h"
#include "cx_version.h"
#include "cx_module.h"

#include "misc_macros.h"
#include "paramstr_parser.h"


/*** General definitions ********************************************/

/* Helper macros and various definitions */

#define CXSD_FRONTEND_MODREC_SUFFIX     _frontend_modrec
#define CXSD_FRONTEND_MODREC_SUFFIX_STR __CX_STRINGIZE(CXSD_FRONTEND_MODREC_SUFFIX)

enum {CXSD_FRONTEND_MODREC_MAGIC   = 0x646e7446};  /* Little-endian 'Ftnd' */
enum
{
    CXSD_FRONTEND_MODREC_VERSION_MAJOR = 2,
    CXSD_FRONTEND_MODREC_VERSION_MINOR = 0,
    CXSD_FRONTEND_MODREC_VERSION = CX_ENCODE_VERSION(CXSD_FRONTEND_MODREC_VERSION_MAJOR,
                                                     CXSD_FRONTEND_MODREC_VERSION_MINOR)
};


typedef int  (*CxsdFrontendInitFunc)      (int server_instance);
typedef void (*CxsdFrontendTermProc)      (void);
typedef void (*CxsdFrontendCycleBeginProc)(void);
typedef void (*CxsdFrontendCycleEndProc)  (void);

typedef struct CxsdFrontendModRec_struct
{
    cx_module_rec_t             mr;

    CxsdFrontendInitFunc        init_fe;
    CxsdFrontendTermProc        term_fe;

    CxsdFrontendCycleBeginProc  begin_cycle;
    CxsdFrontendCycleEndProc    end_cycle;

    struct CxsdFrontendModRec_struct *next;
} CxsdFrontendModRec;

#define CXSD_FRONTEND_MODREC_NAME(name) \
    __CX_CONCATENATE(name, CXSD_FRONTEND_MODREC_SUFFIX)

#define DECLARE_CXSD_FRONTEND(name) \
    CxsdFrontendModRec CXSD_FRONTEND_MODREC_NAME(name)

#define DEFINE_CXSD_FRONTEND(name, comment,                          \
                            init_m, term_m,                          \
                            init_f, term_f,                          \
                            begin_c, end_c)                          \
    CxsdFrontendModRec CXSD_FRONTEND_MODREC_NAME(name) =             \
    {                                                                \
        {                                                            \
            CXSD_FRONTEND_MODREC_MAGIC, CXSD_FRONTEND_MODREC_VERSION,\
            __CX_STRINGIZE(name), comment,                           \
            init_m, term_m                                           \
        },                                                           \
        init_f, term_f,                                              \
        begin_c, end_c,                                              \
        NULL                                                         \
    }


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CXSD_FRONTEND_H */
