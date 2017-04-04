#include <string.h>

#include "cda.h"
#include "cdaP.h"
#include "cda_plugmgr.h"

#include "cda_d_cx.h"
#include "cda_d_dircn.h"
#include "cda_d_local.h"
#include "cda_d_vcas.h"
#include "cda_d_epics.h"
#include "cda_d_v2cx.h"
#include "cda_f_fla.h"


static int              cda_plugmgr_initialized = 0;
static cda_dat_p_rec_t *first_dat_p_metric      = NULL;
static cda_fla_p_rec_t *first_fla_p_metric      = NULL;


static int DoRegisterDat_p(cda_dat_p_rec_t *metric)
{
    if (cda_register_dat_plugin(metric) < 0) return -1;
    if (metric->mr.init_mod != NULL  &&
        metric->mr.init_mod() < 0)           return -1;

    return 0;
}

static int DoRegisterFla_p(cda_fla_p_rec_t *metric)
{
    if (cda_register_fla_plugin(metric) < 0) return -1;
    if (metric->mr.init_mod != NULL  &&
        metric->mr.init_mod() < 0)           return -1;

    return 0;
}

static int initialize_cda_plugmgr(void)
{
    if (DoRegisterDat_p(&CDA_DAT_P_MODREC_NAME(cx))    < 0  ||
        DoRegisterDat_p(&CDA_DAT_P_MODREC_NAME(dircn)) < 0  ||
        DoRegisterDat_p(&CDA_DAT_P_MODREC_NAME(local)) < 0  ||
        DoRegisterDat_p(&CDA_DAT_P_MODREC_NAME(vcas))  < 0  ||
        DoRegisterDat_p(&CDA_DAT_P_MODREC_NAME(epics)) < 0  ||
#ifndef NOV2CX
        DoRegisterDat_p(&CDA_DAT_P_MODREC_NAME(v2cx))  < 0  ||
#endif
        DoRegisterFla_p(&CDA_FLA_P_MODREC_NAME(fla))   < 0) return -1;

    cda_plugmgr_initialized = 1;

    return 0;
}


/* Public registration/deregistration API */

int  cda_register_dat_plugin  (cda_dat_p_rec_t *metric)
{
  cda_dat_p_rec_t *mp;

    /* First, check plugin credentials */
    if (metric->mr.magicnumber != CDA_DAT_P_MODREC_MAGIC)
    {
        cda_set_err("dat-plugin \"%s\".magicnumber mismatch", metric->mr.name);
        return -1;
    }
    if (!CX_VERSION_IS_COMPATIBLE(metric->mr.version, CDA_DAT_P_MODREC_VERSION))
    {
        cda_set_err("dat-plugin \"%s\".version=%d.%d.%d.%d, incompatible with %d.%d.%d.%d",
                    metric->mr.name,
                    CX_MAJOR_OF(metric->mr.version),
                    CX_MINOR_OF(metric->mr.version),
                    CX_PATCH_OF(metric->mr.version),
                    CX_SNAP_OF (metric->mr.version),
                    CX_MAJOR_OF(CDA_DAT_P_MODREC_VERSION),
                    CX_MINOR_OF(CDA_DAT_P_MODREC_VERSION),
                    CX_PATCH_OF(CDA_DAT_P_MODREC_VERSION),
                    CX_SNAP_OF (CDA_DAT_P_MODREC_VERSION));
        return -1;
    }

    /* ...just a guard: check if this exact plugin is already registered */
    for (mp = first_dat_p_metric;  mp != NULL;  mp = mp->next)
        if (mp == metric) return +1;
    
    /* Then, ensure that this scheme isn't already registered */
    for (mp = first_dat_p_metric;  mp != NULL;  mp = mp->next)
        if (strcasecmp(metric->mr.name, mp->mr.name) == 0)
        {
            cda_set_err("data-access scheme \"%s\" already registered", metric->mr.name);
            return -1;
        }

    /* Insert at beginning of the list */
    metric->next = first_dat_p_metric;
    first_dat_p_metric = metric;
    
    return 0;
}

int  cda_deregister_dat_plugin(cda_dat_p_rec_t *metric)
{
}

int  cda_register_fla_plugin  (cda_fla_p_rec_t *metric)
{
  cda_fla_p_rec_t *mp;

    /* First, check plugin credentials */
    if (metric->mr.magicnumber != CDA_FLA_P_MODREC_MAGIC)
    {
        cda_set_err("fla-plugin \"%s\".magicnumber mismatch", metric->mr.name);
        return -1;
    }
    if (!CX_VERSION_IS_COMPATIBLE(metric->mr.version, CDA_FLA_P_MODREC_VERSION))
    {
        cda_set_err("fla-plugin \"%s\".version=%d.%d.%d.%d, incompatible with %d.%d.%d.%d",
                    metric->mr.name,
                    CX_MAJOR_OF(metric->mr.version),
                    CX_MINOR_OF(metric->mr.version),
                    CX_PATCH_OF(metric->mr.version),
                    CX_SNAP_OF (metric->mr.version),
                    CX_MAJOR_OF(CDA_FLA_P_MODREC_VERSION),
                    CX_MINOR_OF(CDA_FLA_P_MODREC_VERSION),
                    CX_PATCH_OF(CDA_FLA_P_MODREC_VERSION),
                    CX_SNAP_OF (CDA_FLA_P_MODREC_VERSION));
        return -1;
    }

    /* ...just a guard: check if this exact plugin is already registered */
    for (mp = first_fla_p_metric;  mp != NULL;  mp = mp->next)
        if (mp == metric) return +1;
    
    /* Then, ensure that this scheme isn't already registered */
    for (mp = first_fla_p_metric;  mp != NULL;  mp = mp->next)
        if (strcasecmp(metric->mr.name, mp->mr.name) == 0)
        {
            cda_set_err("formula scheme \"%s\" already registered", metric->mr.name);
            return -1;
        }

    /* Insert at beginning of the list */
    metric->next = first_fla_p_metric;
    first_fla_p_metric = metric;
    
    return 0;
}

int  cda_deregister_fla_plugin(cda_fla_p_rec_t *metric)
{
}


/* Internal cda plug-ins-access API */

cda_dat_p_rec_t * cda_get_dat_p_rec_by_scheme(const char *scheme)
{
  cda_dat_p_rec_t *mp;
  
    if (!cda_plugmgr_initialized)
    {
        if (initialize_cda_plugmgr() < 0) return NULL;
    }
  
    for (mp = first_dat_p_metric;  mp != NULL;  mp = mp->next)
        if (strcasecmp(scheme, mp->mr.name) == 0)
            return mp;

    return NULL;
}

cda_fla_p_rec_t * cda_get_fla_p_rec_by_scheme(const char *scheme)
{
  cda_fla_p_rec_t *mp;
  
    for (mp = first_fla_p_metric;  mp != NULL;  mp = mp->next)
        if (strcasecmp(scheme, mp->mr.name) == 0)
            return mp;

    return NULL;
}
