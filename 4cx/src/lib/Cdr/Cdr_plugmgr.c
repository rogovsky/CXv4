#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "misc_macros.h"

#include "datatreeP.h"
#include "CdrP.h"

#include "Cdr_plugmgr.h"
#include "Cdr_file_ldr.h"
#include "Cdr_plaintext_ldr.h"


static int                 Cdr_plugmgr_initialized = 0;
static CdrSubsysLdrModRec *first_subsys_ldr_metric = NULL;


static int initialize_Cdr_plugmgr(void)
{
    if (//CdrRegisterSubsysLdr(&(CDR_SUBSYS_LDR_MODREC_NAME(string)))    < 0  ||
        CdrRegisterSubsysLdr(&(CDR_SUBSYS_LDR_MODREC_NAME(file)))      < 0  ||
        CdrRegisterSubsysLdr(&(CDR_SUBSYS_LDR_MODREC_NAME(plaintext))) < 0)
        return -1;

    Cdr_plugmgr_initialized = 1;

    return 0;
}


int  CdrRegisterSubsysLdr  (CdrSubsysLdrModRec *metric)
{
  CdrSubsysLdrModRec *mp;

    /* First, check plugin credentials */
    if (metric->mr.magicnumber != CDR_SUBSYS_LDR_MODREC_MAGIC)
    {
        CdrSetErr("subsys_ldr-plugin \"%s\".magicnumber mismatch", metric->mr.name);
        return -1;
    }
    if (!CX_VERSION_IS_COMPATIBLE(metric->mr.version, CDR_SUBSYS_LDR_MODREC_VERSION))
    {
        CdrSetErr("subsys_ldr-plugin \"%s\".version=%d.%d.%d.%d, incompatible with %d.%d.%d.%d",
                  metric->mr.name,
                  CX_MAJOR_OF(metric->mr.version),
                  CX_MINOR_OF(metric->mr.version),
                  CX_PATCH_OF(metric->mr.version),
                  CX_SNAP_OF (metric->mr.version),
                  CX_MAJOR_OF(CDR_SUBSYS_LDR_MODREC_VERSION),
                  CX_MINOR_OF(CDR_SUBSYS_LDR_MODREC_VERSION),
                  CX_PATCH_OF(CDR_SUBSYS_LDR_MODREC_VERSION),
                  CX_SNAP_OF (CDR_SUBSYS_LDR_MODREC_VERSION));
        return -1;
    }

    /* ...just a guard: check if this exact plugin is already registered */
    for (mp = first_subsys_ldr_metric;  mp != NULL;  mp = mp->next)
        if (mp == metric) return +1;
    
    /* Then, ensure that this scheme isn't already registered */
    for (mp = first_subsys_ldr_metric;  mp != NULL;  mp = mp->next)
        if (strcasecmp(metric->mr.name, mp->mr.name) == 0)
        {
            CdrSetErr("subsys_ldr scheme \"%s\" already registered", metric->mr.name);
            return -1;
        }

    /* Insert at beginning of the list */
    metric->next = first_subsys_ldr_metric;
    first_subsys_ldr_metric = metric;
    
    return 0;
}

int  CdrDeregisterSubsysLdr(CdrSubsysLdrModRec *metric)
{
}


CdrSubsysLdrModRec * CdrGetSubsysLdrByScheme(const char *scheme)
{
  CdrSubsysLdrModRec *mp;

    if (!Cdr_plugmgr_initialized)
    {
        if (initialize_Cdr_plugmgr() < 0) return NULL;
    }

    for (mp = first_subsys_ldr_metric;  mp != NULL;  mp = mp->next)
        if (strcasecmp(scheme, mp->mr.name) == 0)
            return mp;

    return NULL;
}
