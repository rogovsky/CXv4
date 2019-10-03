#include <string.h>

#include <unistd.h>    // for getuid()+geteuid()
#include <sys/types.h> // for getuid()+geteuid()
              

#include "cxldr.h"
#include "findfilein.h"

#include "cda.h"
#include "cdaP.h"
#include "cda_plugmgr.h"

#include "cda_d_cx.h"
#include "cda_d_dircn.h"
#include "cda_d_local.h"
#include "cda_d_vcas.h"
#include "cda_d_v2cx.h"
#include "cda_f_fla.h"


static int              cda_plugmgr_initialized = 0;
static int              plugins_loading_allowed = 1;
static cda_dat_p_rec_t *first_dat_p_metric      = NULL;
static cda_fla_p_rec_t *first_fla_p_metric      = NULL;


//////////////////////////////////////////////////////////////////////

static int  cda_plugmgr_checker_cm(const char *name, void *symptr, void *privptr)
{
  cx_module_rec_t  *mr   = symptr;

    if (mr->magicnumber != CDA_DAT_P_MODREC_MAGIC)
    {
        cda_set_err("dat-plugin (%s).magicnumber=0x%x, mismatch with 0x%x",
                    name, mr->magicnumber, CDA_DAT_P_MODREC_MAGIC);
        errno = -1; /* errno=-1 means "cda_set_err() is already called" */
        return -1;
    }
    if (!CX_VERSION_IS_COMPATIBLE(mr->version, CDA_DAT_P_MODREC_VERSION))
    {
        cda_set_err("dat-plugin (%s).version=%d.%d.%d.%d, incompatible with %d.%d.%d.%d",
                    name,
                    CX_MAJOR_OF(mr->version),
                    CX_MINOR_OF(mr->version),
                    CX_PATCH_OF(mr->version),
                    CX_SNAP_OF (mr->version),
                    CX_MAJOR_OF(CDA_DAT_P_MODREC_VERSION),
                    CX_MINOR_OF(CDA_DAT_P_MODREC_VERSION),
                    CX_PATCH_OF(CDA_DAT_P_MODREC_VERSION),
                    CX_SNAP_OF (CDA_DAT_P_MODREC_VERSION));
        errno = -1; /* errno=-1 means "cda_set_err() is already called" */
        return -1;
    }

    return 0;
}

static int  cda_plugmgr_init_cm   (const char *name, void *symptr)
{
  cx_module_rec_t  *mr   = symptr;

    if (mr->init_mod == NULL  ||
        mr->init_mod() == 0)
        return 0;
    else
        return -1;
}

static void cda_plugmgr_fini_cm   (const char *name, void *symptr)
{
  cx_module_rec_t  *mr   = symptr;

    if (mr->term_mod != NULL)
        mr->term_mod();
}

static CXLDR_DEFINE_CONTEXT(cda_dat_p_ldr_context,
                            CXLDR_FLAG_NONE,
                            "$CX_CDA_PLUGINS_DIR"
                                FINDFILEIN_SEP "./"
                                FINDFILEIN_SEP "!/"
                                FINDFILEIN_SEP "!/../lib/cda/plugins"
                                FINDFILEIN_SEP "$PULT_ROOT/lib/cda/plugins"
                                FINDFILEIN_SEP "~/4pult/lib/cda/plugins",
                            "cda_d_", ".so",
                            "", CDA_DAT_P_MODREC_SUFFIX_STR,
                            cda_plugmgr_checker_cm,
                            cda_plugmgr_init_cm, cda_plugmgr_fini_cm,
                            NULL,
                            NULL, 0);

//////////////////////////////////////////////////////////////////////

typedef struct
{
    const char *name;
} erprtd_scheme_t;

static erprtd_scheme_t  *reported_list        = NULL;
static int               reported_list_allocd = 0;

// GetErsSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Ers, erprtd_scheme_t,
                                 reported, name, NULL, (void*)1,
                                 0, 10, 1000000,
                                 , , void)

//////////////////////////////////////////////////////////////////////

void cda_allow_plugins_loading(int allow)
{
    plugins_loading_allowed = allow;
}

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
    return -1;
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
    return -1;
}


/* Internal cda plug-ins-access API */

static int ErsCmpIterator(erprtd_scheme_t *erp, void *privptr)
{
    return strcmp(erp->name, (const char *)privptr) == 0;
}
cda_dat_p_rec_t * cda_get_dat_p_rec_by_scheme(const char *argv0, const char *scheme)
{
  cda_dat_p_rec_t *mp;

  int              r;
  void            *mr;
  char            *errstr;

  int              en;
  erprtd_scheme_t *erp;

    if (!cda_plugmgr_initialized)
    {
        if (initialize_cda_plugmgr() < 0) return NULL;
    }
  
    for (mp = first_dat_p_metric;  mp != NULL;  mp = mp->next)
        if (strcasecmp(scheme, mp->mr.name) == 0)
            return mp;

    if (!plugins_loading_allowed  ||
        /* A (paranoid) check against suid */
        getuid() != geteuid()) return NULL;

    r = cxldr_get_module(&cda_dat_p_ldr_context,
                         scheme,
                         argv0,
                         &mr,
                         &errstr,
                         NULL);
    if (r != 0)
    {
        if      (errno < 0)
            /* Do-nothing, error is already logged */;
        else if (errno == 0)
            cda_set_err("dat-plugin %s: symbol \"%s%s%s\" not found",
                        scheme,
                        cda_dat_p_ldr_context.symprefix, scheme, cda_dat_p_ldr_context.symsuffix);
        else
            cda_set_err("error loading dat-plugin %s: %s",
                        scheme,
                        errstr != NULL? errstr : strerror(errno));

        /* Check for this scheme's error already reported */
        if (ForeachErsSlot(ErsCmpIterator, (void*)scheme) < 0)
        {
            fprintf(stderr, "%s: %s\n",
                    argv0, cda_last_err());

            en = GetErsSlot();
            if (en >= 0)
            {
                erp = AccessErsSlot(en);
                erp->name = strdup(scheme);
            }
        }

        return NULL;
    }

    /* Okay, re-do search */
    for (mp = first_dat_p_metric;  mp != NULL;  mp = mp->next)
        if (strcasecmp(scheme, mp->mr.name) == 0)
            return mp;

    return NULL;
}

cda_fla_p_rec_t * cda_get_fla_p_rec_by_scheme(const char *argv0, const char *scheme)
{
  cda_fla_p_rec_t *mp;
  
    for (mp = first_fla_p_metric;  mp != NULL;  mp = mp->next)
        if (strcasecmp(scheme, mp->mr.name) == 0)
            return mp;

    return NULL;
}
