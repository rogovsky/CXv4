//#include "cxsd_includes.h"

#include "cxsd_logger.h"
#include "cxsd_modmgr.h"

#include "cxldr.h"


static int  CxsdModuleChecker_cm(const char *name, void *symptr, void *privptr)
{
  cx_module_rec_t  *mr   = symptr;
  cx_module_desc_t *desc = privptr;

    if (mr->magicnumber != desc->magicnumber)
    {
        logline(LOGF_MODULES, 0,
                "%s (%s).magicnumber=0x%x, mismatch with 0x%x",
                desc->kindname, name, mr->magicnumber, desc->magicnumber);
        errno = -1; /* errno=-1 means "Do-nothing, error is already logged" for DoLoadModule() */
        return -1;
    }
    if (!CX_VERSION_IS_COMPATIBLE(mr->version, desc->version))
    {
        logline(LOGF_MODULES, 0,
                "%s (%s).version=%d.%d.%d.%d, incompatible with %d.%d.%d.%d",
                desc->kindname, name,
                CX_MAJOR_OF(mr->version),
                CX_MINOR_OF(mr->version),
                CX_PATCH_OF(mr->version),
                CX_SNAP_OF (mr->version),
                CX_MAJOR_OF(desc->version),
                CX_MINOR_OF(desc->version),
                CX_PATCH_OF(desc->version),
                CX_SNAP_OF (desc->version));
        errno = -1; /* errno=-1 means "Do-nothing, error is already logged" for DoLoadModule() */
        return -1;
    }
    
    return 0;
}

static int  CxsdModuleInit_cm   (const char *name, void *symptr)
{
  cx_module_rec_t  *mr   = symptr;

    if (mr->init_mod == NULL  ||
        mr->init_mod() == 0)
        return 0;
    else
    {
        errno = -1;
        return -1;
    }
}

static void CxsdModuleFini_cm   (const char *name, void *symptr)
{
  cx_module_rec_t  *mr   = symptr;

    if (mr->term_mod != NULL)
        mr->term_mod();
}

//////////////////////////////////////////////////////////////////////

static cx_module_desc_t drv_desc =
{
    "driver",
    CXSD_DRIVER_MODREC_MAGIC,
    CXSD_DRIVER_MODREC_VERSION
};

static cx_module_desc_t lyr_desc =
{
    "layer",
    CXSD_LAYER_MODREC_MAGIC,
    CXSD_LAYER_MODREC_VERSION
};

static cx_module_desc_t fnd_desc =
{
    "frontend",
    CXSD_FRONTEND_MODREC_MAGIC,
    CXSD_FRONTEND_MODREC_VERSION
};

static cx_module_desc_t ext_desc =
{
    "ext",
    CXSD_EXT_MODREC_MAGIC,
    CXSD_EXT_MODREC_VERSION
};

static cx_module_desc_t lib_desc =
{
    "lib",
    CXSD_LIB_MODREC_MAGIC,
    CXSD_LIB_MODREC_VERSION
};


static CXLDR_DEFINE_CONTEXT(drv_ldr_context,
                            CXLDR_FLAG_NONE,
                            NULL,
                            "", "_drv.so",
                            "", CXSD_DRIVER_MODREC_SUFFIX_STR,
                            CxsdModuleChecker_cm,
                            CxsdModuleInit_cm, CxsdModuleFini_cm,
                            NULL,
                            NULL, 0);

static CXLDR_DEFINE_CONTEXT(lyr_ldr_context,
                            CXLDR_FLAG_NONE,
                            NULL,
                            "", "_lyr.so",
                            "", CXSD_LAYER_MODREC_SUFFIX_STR,
                            CxsdModuleChecker_cm,
                            CxsdModuleInit_cm, CxsdModuleFini_cm,
                            NULL,
                            NULL, 0);

static CXLDR_DEFINE_CONTEXT(fnd_ldr_context,
                            CXLDR_FLAG_NONE,
                            NULL,
                            "cxsd_fe_", ".so",
                            "", CXSD_FRONTEND_MODREC_SUFFIX_STR,
                            CxsdModuleChecker_cm,
                            CxsdModuleInit_cm, CxsdModuleFini_cm,
                            NULL,
                            NULL, 0);


static CXLDR_DEFINE_CONTEXT(ext_ldr_context,
                            CXLDR_FLAG_NONE,
                            NULL,
                            "", "_ext.so",
                            "", CXSD_EXT_MODREC_SUFFIX_STR,
                            CxsdModuleChecker_cm,
                            CxsdModuleInit_cm, CxsdModuleFini_cm,
                            NULL,
                            NULL, 0);

static CXLDR_DEFINE_CONTEXT(lib_ldr_context,
                            CXLDR_FLAG_GLOBAL,
                            NULL,
                            "", "_lib.so",
                            "", CXSD_LIB_MODREC_SUFFIX_STR,
                            CxsdModuleChecker_cm,
                            CxsdModuleInit_cm, CxsdModuleFini_cm,
                            NULL,
                            NULL, 0);
/////////////////


int  CxsdSetDrvLyrPath    (const char *path)
{
    return
        cxldr_set_path(&lyr_ldr_context, path) == 0
        &&
        cxldr_set_path(&drv_ldr_context, path) == 0
        ? 0 : -1;
}

int  CxsdSetFndPath       (const char *path)
{
    return cxldr_set_path(&fnd_ldr_context, path) == 0? 0 : -1;
}

int  CxsdSetExtPath       (const char *path)
{
    return cxldr_set_path(&ext_ldr_context, path) == 0? 0 : -1;
}

int  CxsdSetLibPath       (const char *path)
{
    return cxldr_set_path(&lib_ldr_context, path) == 0? 0 : -1;
}


static int DoLoadModule(const char        *argv0, 
                        const char        *modname,
                        void             **metric_p,
                        cxldr_context_t   *ctx,
                        cx_module_desc_t  *mod_desc)
{
  int                 r;
  char               *errstr;

    r = cxldr_get_module(ctx,
                         modname,
                         argv0,
                         metric_p,
                         &errstr,
                         mod_desc);

    if (r != 0)
    {
        if      (errno < 0)
            /* Do-nothing, error is already logged */;
        else if (errno == 0)
            logline(LOGF_MODULES, 0, "%s %s: symbol \"%s%s%s\" not found",
                    mod_desc->kindname, modname,
                    ctx->symprefix, modname, ctx->symsuffix);
        else
            logline(LOGF_MODULES, 0, "error loading %s %s: %s",
                    mod_desc->kindname, modname,
                    errstr != NULL? errstr : strerror(errno));
    }
    
    return r;
}

int  CxsdLoadDriver  (const char          *argv0,
                      const char          *typename, const char *drvname,
                      CxsdDriverModRec   **metric_p)
{
  const char         *modname;

    modname = drvname != NULL  &&  drvname[0] != '\0'? drvname
                                                     : typename;
    return DoLoadModule(argv0, modname, (void **)metric_p,
                        &drv_ldr_context, &drv_desc);
}

int  CxsdLoadLayer   (const char          *argv0,
                      const char          *lyrname,
                      CxsdLayerModRec    **metric_p)
{
    return DoLoadModule(argv0, lyrname, (void **)metric_p,
                        &lyr_ldr_context, &lyr_desc);
}

int  CxsdLoadFrontend(const char          *argv0,
                      const char          *name,
                      CxsdFrontendModRec **metric_p)
{
    return DoLoadModule(argv0, name, (void **)metric_p,
                        &fnd_ldr_context, &fnd_desc);
}

int  CxsdLoadExt     (const char *argv0, const char *name)
{
    return DoLoadModule(argv0, name, NULL,
                        &ext_ldr_context, &ext_desc);
}

int  CxsdLoadLib     (const char *argv0, const char *name)
{
    return DoLoadModule(argv0, name, NULL,
                        &lib_ldr_context, &lib_desc);
}

//////////////////////////////////////////////////////////////////////

static CxsdFrontendModRec *first_frontend_metric = NULL;

int  CxsdActivateFrontends (int instance)
{
  CxsdFrontendModRec *mp;

    for (mp = first_frontend_metric;  mp != NULL;  mp = mp->next)
        if (mp->init_fe != NULL  &&
            mp->init_fe(instance) != 0) return -1;

    return 0;
}

void CxsdCallFrontendsBegCy(void)
{
  CxsdFrontendModRec *mp;

    for (mp = first_frontend_metric;  mp != NULL;  mp = mp->next)
        if (mp->begin_cycle != NULL)
            mp->begin_cycle();
}

void CxsdCallFrontendsEndCy(void)
{
  CxsdFrontendModRec *mp;

    for (mp = first_frontend_metric;  mp != NULL;  mp = mp->next)
        if (mp->end_cycle != NULL)
            mp->end_cycle();
}

int  CxsdRegisterFrontend  (CxsdFrontendModRec *metric)
{
  CxsdFrontendModRec *mp;

    if (metric == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    /* Check if it is already registered */
    for (mp = first_frontend_metric;  mp != NULL;  mp = mp->next)
        if (mp == metric) return +1;
    
    /* Insert at beginning of the list */
    metric->next = first_frontend_metric;
    first_frontend_metric = metric;

    return 0;
}

int  CxsdDeregisterFrontend(CxsdFrontendModRec *metric)
{
}
