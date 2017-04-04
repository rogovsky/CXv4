#include <stdarg.h>
#include <stdio.h>
#include <errno.h>

#include "misclib.h"
#include "cxlogger.h"

#define __CXSD_LOGGER_C
#include "cxsd_logger.h"


static ll_catdesc_t   cats_info[LOGF_count+1];
static ll_filedesc_t  file_info[LOGF_count+1];

int   cxsd_log_init(const char *argv0, int verbosity, int consolity)
{
  const char *ident;
  int         fspc;
  int         cat;
  int         r;
  int         open_errno;

    if (argv0 == NULL) argv0 = "(UNKNOWN)";
    ident = strrchr(argv0, '/');
    if (ident != NULL) ident++; else ident = argv0;

    cats_info[LOGF_SYSTEM  ].name = "system";
    cats_info[LOGF_ACCESS  ].name = "access";
    cats_info[LOGF_DEBUG   ].name = "debug";
    cats_info[LOGF_MODULES ].name = "modules";
    cats_info[LOGF_HARDWARE].name = "hardware";

    /* Check specifiedness and perform mapping for those who aren't */
    /* First, try to find a SPECIFIED category... */
    for (fspc = 0;  fspc < countof(cats_info) - 1;  fspc++)
    {
        /* ...and if found -- map all unspecified to the same destination */
        if (cats_info[fspc].is_on)
        {
            for (cat = 0;  cat < countof(cats_info) - 1;  cat++)
                if (cats_info[cat].is_on == 0)
                {
                    cats_info[cat].fref  = cats_info[fspc].fref;
                    cats_info[cat].is_on = 1;
                }
            goto END_FSPC;
        }
    }
 END_FSPC:;

    /*  */
    r = ll_init(cats_info, LOGF_ERRNO, ident, verbosity, consolity, file_info);
    if (r < 0)
    {
        open_errno = errno;
        fprintf(stderr, "%s %s(): failed to open \"%s\" for writing: %s\n",
               strcurtime(), __FUNCTION__, 
               file_info[-r - 1].path, strerror(open_errno));
        return -1;
    }
    cxsd_logger_verbosity = verbosity;

    return 0;
}

char *cxsd_log_setf(int cat_mask, const char *path)
{
  int cat;
  int ref;
  int x;
  int any;

    /* First, free previously-used-and-now-unused files */
    for (cat = 0;  cat < countof(cats_info) - 1;  cat++)
        if ((cat_mask & (1 << cat)) != 0  &&
            cats_info[cat].is_on)
        {
            /* Temporarily mark this category as unconfigured (for ease of subsequent search) */
            cats_info[cat].is_on = 0;

            /* And check if now ANY category does reference that file */
            for (x = 0, any = 0, ref = cats_info[cat].fref;
                 x < countof(cats_info) - 1;
                 x++)
                any |= (cats_info[x].is_on  &&
                        cats_info[x].fref == ref);

            if (!any)
            {
                safe_free(file_info[ref].path);
                file_info[ref].path = NULL;
            }
        }

    /* Second, configure a file */
    for (ref = 0;
         ref < countof(file_info) - 1  &&  file_info[ref].path != NULL;
         ref++);
    if (ref >= countof(file_info) - 1)
        return "file_info[] table overflow";
    file_info[ref].path = strdup(path);
    if (file_info[ref].path == NULL)
        return "strdup() failure";

    /* Third, set all involved categories to point there */
    for (cat = 0;  cat < countof(cats_info) - 1;  cat++)
        if ((cat_mask & (1 << cat)) != 0)
        {
            cats_info[cat].is_on = 1;
            cats_info[cat].fref  = ref;
        }

    return NULL;
}

void  logline (logtype_t type, int level, const char *format, ...)
{
  va_list  ap;
    
    va_start(ap, format);
    vloglineX(type, level, "", format, ap);
    va_end(ap);
}

void vloglineX(logtype_t type, int level,
               const char *subaddress, const char *format, va_list ap)
{
#if 1
    ll_vlogline(type, level, subaddress, format, ap);
#else
  int      saved_errno = errno;

    fprintf(stderr, "%s: ", strcurtime_msc());
    vfprintf(stderr, format, ap);
    if (type & LOGF_ERRNO) fprintf(stderr, ": %s", strerror(saved_errno));
    fprintf(stderr, "\n");
#endif
}
