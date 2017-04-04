#ifndef __CDRP_H
#define __CDRP_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <stdarg.h>

#include "misc_macros.h"
#include "cx_version.h"
#include "cx_module.h"

#include "datatree.h"
#include "datatreeP.h"


/* General structure-management API */
data_section_t *CdrCreateSection(DataSubsys sys,
                                 const char *type,
                                 char       *name, size_t namelen,
                                 void       *data, size_t datasize);


/* Getter/converter-plugins management */

#define CDR_SUBSYS_LDR_MODREC_SUFFIX     _subsys_ldr_modrec
#define CDR_SUBSYS_LDR_MODREC_SUFFIX_STR __CX_STRINGIZE(CDR_SUBSYS_LDR_MODREC_SUFFIX)

enum {CDR_SUBSYS_LDR_MODREC_MAGIC = 0x4c726443 }; /* Little-endian 'CdrL' */
enum
{
    CDR_SUBSYS_LDR_MODREC_VERSION_MAJOR = 1,
    CDR_SUBSYS_LDR_MODREC_VERSION_MINOR = 0,
    CDR_SUBSYS_LDR_MODREC_VERSION = CX_ENCODE_VERSION(CDR_SUBSYS_LDR_MODREC_VERSION_MAJOR,
                                                      CDR_SUBSYS_LDR_MODREC_VERSION_MINOR)
};


typedef DataSubsys (*Cdr_subsys_ldr_t)(const char *reference,
                                       const char *argv0);

typedef struct CdrSubsysLdrModRec_struct
{
    cx_module_rec_t   mr;

    Cdr_subsys_ldr_t  ldr;
    
    struct CdrSubsysLdrModRec_struct *next;
} CdrSubsysLdrModRec;

#define CDR_SUBSYS_LDR_MODREC_NAME(name) \
    __CX_CONCATENATE(name, CDR_SUBSYS_LDR_MODREC_SUFFIX)

#define DECLARE_CDR_SUBSYS_LDR(name) \
    CdrSubsysLdrModRec CDR_SUBSYS_LDR_MODREC_NAME(name)

#define DEFINE_CDR_SUBSYS_LDR(name, comment,                           \
                              init_m, term_m,                          \
                              ldr_f)                                   \
    CdrSubsysLdrModRec CDR_SUBSYS_LDR_MODREC_NAME(name) =              \
    {                                                                  \
        {                                                              \
            CDR_SUBSYS_LDR_MODREC_MAGIC, CDR_SUBSYS_LDR_MODREC_VERSION,\
            __CX_STRINGIZE(name), comment,                             \
            init_m, term_m                                             \
        },                                                             \
        ldr_f,                                                         \
        NULL                                                           \
    }


int  CdrRegisterSubsysLdr  (CdrSubsysLdrModRec *metric);
int  CdrDeregisterSubsysLdr(CdrSubsysLdrModRec *metric);


/* Standard from-text/via-ppf4td API */

DataSubsys  CdrLoadSubsystemViaPpf4td(const char *scheme,
                                      const char *reference);

DataSubsys  CdrLoadSubsystemFileViaPpf4td(const char *scheme,
                                          const char *reference,
                                          const char *argv0);

/* Error descriptions setting API */

void CdrClearErr(void);
void CdrSetErr  (const char *format, ...)
    __attribute__((format (printf, 1, 2)));
void CdrVSetErr (const char *format, va_list ap);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CDRP_H */
