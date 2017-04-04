#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>

#include "cx_sysdeps.h"
#include "misc_macros.h"
#include "misclib.h"
#include "findfilein.h"

#include "datatreeP.h"
#include "CdrP.h"
#include "Cdr.h"


typedef struct
{
  char  path[PATH_MAX];
} chk_rec_t;

static void *glue_checker(const char *name,
                          const char *path,
                          void       *privptr)
{
  char       buf[PATH_MAX];
  char       plen = strlen(path);
  FILE      *fp;
  chk_rec_t *crp  = privptr;
  
    check_snprintf(buf, sizeof(buf), "%s%s%s.subsys",
                   path,
                   plen == 0  ||  path[plen-1] == '/'? "" : "/",
                   name);
    fp = fopen(buf, "r");
    if (fp == NULL) return NULL;

    fclose(fp);
    strzcpy(crp->path, buf, sizeof(crp->path));
    return privptr;
}

DataSubsys  CdrLoadSubsystemFileViaPpf4td(const char *scheme,
                                          const char *reference,
                                          const char *argv0)
{
  chk_rec_t     rec;
  void         *ff;
  FILE         *fp;

    /* Is it a direct reference? */
    if ((    strchr(reference, '.')  != NULL  ||
             strchr(reference, '/')  != NULL)  &&
        (fp = fopen(reference, "r")) != NULL)
    {
        fclose(fp);
        strzcpy(rec.path, reference, sizeof(rec.path));
        ff = &rec;
    }
    else
    /* Find the file */
        ff = findfilein(reference,
#if OPTION_HAS_PROGRAM_INVOCATION_NAME
                        program_invocation_name,
#else
                        argv0,
#endif /* OPTION_HAS_PROGRAM_INVOCATION_NAME */
                        glue_checker, &rec,
                        "./"
                            FINDFILEIN_SEP "!/"
                            FINDFILEIN_SEP "!/../screens"
                            FINDFILEIN_SEP "$PULT_ROOT/screens"
                            FINDFILEIN_SEP "~/4pult/screens");
    if (ff == NULL)
    {
        CdrSetErr("fopen(...\"%s.subsys\"): %s", reference, strerror(errno));
        return NULL;
    }

    /* Load it */
    return CdrLoadSubsystemViaPpf4td(scheme, rec.path);
}
