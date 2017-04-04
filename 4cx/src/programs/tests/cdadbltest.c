#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>

#include "misc_macros.h"
#include "misclib.h"
#include "cxscheduler.h"

#include "cx.h"
#include "cda.h"


static void NewValProc(int            uniq,
                       void          *privptr1,
                       cda_dataref_t  ref,
                       int            reason,
                       void          *info_ptr,
                       void          *privptr2)
{
  char   *name = privptr2;
  double  v;

    cda_get_dcval(ref, &v);
    printf("%s %s=%8.3f\n", strcurtime(), name, v);
}

int main(int argc, char *argv[])
{
  cda_context_t  the_cid;
  int            n;
  cda_dataref_t  ref;

    the_cid = cda_new_context(0, NULL, "v2cx::", 0, argv[0],
                              0, NULL, NULL);
    if (the_cid == CDA_CONTEXT_ERROR)
    {
        fprintf(stderr, "%s %s: cda_new_context(): %s\n",
                strcurtime(), argv[0],
                cda_last_err());
        exit(1);
    }

    for (n = 1;  n < argc;  n++)
    {
        ref = cda_add_chan(the_cid, NULL, argv[n], 0,
                           CXDTYPE_DOUBLE, 1,
                           CDA_REF_EVMASK_UPDATE, NewValProc, argv[n]);
        if (ref < 0)
            fprintf(stderr, "%s %s: cda_add_chan(\"%s\"): %s\n",
                    strcurtime(), argv[0], argv[optind], cda_last_err());
    }

    sl_main_loop();
}
