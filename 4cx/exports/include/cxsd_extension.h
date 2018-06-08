#ifndef __CXSD_EXTENSION_H
#define __CXSD_EXTENSION_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "cx.h"
#include "cx_version.h"
#include "cx_module.h"

#include "misc_macros.h"


/*** General definitions ********************************************/

#define CXSD_EXT_MODREC_SUFFIX     _ext_modrec
#define CXSD_EXT_MODREC_SUFFIX_STR __CX_STRINGIZE(CXSD_EXT_MODREC_SUFFIX)

enum {CXSD_EXT_MODREC_MAGIC = 0x6e747845}; /* Little-endian 'Extn' */
enum {
    CXSD_EXT_MODREC_VERSION_MAJOR = 1,
    CXSD_EXT_MODREC_VERSION_MINOR = 0,
    CXSD_EXT_MODREC_VERSION = CX_ENCODE_VERSION(CXSD_EXT_MODREC_VERSION_MAJOR,
                                                CXSD_EXT_MODREC_VERSION_MINOR)
};


#define CXSD_LIB_MODREC_SUFFIX     _lib_modrec
#define CXSD_LIB_MODREC_SUFFIX_STR __CX_STRINGIZE(CXSD_LIB_MODREC_SUFFIX)

enum {CXSD_LIB_MODREC_MAGIC = 0x7262694c}; /* Little-endian 'Libr' */
enum
{
    CXSD_LIB_MODREC_VERSION_MAJOR = 1,
    CXSD_LIB_MODREC_VERSION_MINOR = 0,
    CXSD_LIB_MODREC_VERSION = CX_ENCODE_VERSION(CXSD_LIB_MODREC_VERSION_MAJOR,
                                                CXSD_LIB_MODREC_VERSION_MINOR)
};


/*** Modules'/Libraries'-provided interface definition **************/

typedef struct
{
    cx_module_rec_t     mr;
} CxsdExtModRec;

#define CXSD_EXT_MODREC_NAME(name) \
    __CX_CONCATENATE(name,CXSD_EXT_MODREC_SUFFIX)

#define DECLARE_CXSD_EXT(name) \
    CxsdExtModRec CXSD_EXT_MODREC_NAME(name)

#define DEFINE_CXSD_EXT(name, comment,                               \
                        init_m, term_m)                              \
    CxsdExtModRec CXSD_EXT_MODREC_NAME(name) =                       \
    {                                                                \
        {                                                            \
            CXSD_EXT_MODREC_MAGIC, CXSD_EXT_MODREC_VERSION,          \
            __CX_STRINGIZE(name), comment,                           \
            init_m, term_m                                           \
        }                                                            \
    }


typedef struct
{
    cx_module_rec_t     mr;
} CxsdLibModRec;

#define CXSD_LIB_MODREC_NAME(name) \
    __CX_CONCATENATE(name,CXSD_LIB_MODREC_SUFFIX)

#define DECLARE_CXSD_LIB(name) \
    CXSD_LIB_MODREC_NAME(name)

#define DEFINE_CXSD_LIB(name, comment,                               \
                        init_m, term_m)                              \
    CxsdLibModRec CXSD_LIB_MODREC_NAME(name) =                       \
    {                                                                \
        {                                                            \
            CXSD_LIB_MODREC_MAGIC, CXSD_LIBMOD_REC_VERSION,          \
            __CX_STRINGIZE(name), comment,                           \
            init_m, term_m                                           \
        }                                                            \
    }


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CXSD_EXTENSION_H */
