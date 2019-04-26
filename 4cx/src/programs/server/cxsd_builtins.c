#include <stdio.h>

#include "misclib.h"
#include "main_builtins.h"

#include "cxsd_frontend.h"
#include "cxsd_dbP.h"
#include "cdaP.h"

//////////////////////////////////////////////////////////////////////

extern DECLARE_CXSD_FRONTEND (cx);
extern DECLARE_CXSD_DBLDR    (file);
extern CDA_DECLARE_DAT_PLUGIN(insrv);


static cx_module_rec_t *builtins_list[] =
{
    &(CXSD_FRONTEND_MODREC_NAME(cx).mr),
    &(CXSD_DBLDR_MODREC_NAME   (file).mr),
    &(CDA_DAT_P_MODREC_NAME    (insrv).mr),

};
//////////////////////////////////////////////////////////////////////

int  InitBuiltins(init_builtins_err_notifier_t err_notifier)
{
  int  n;

    for (n = 0;  n < countof(builtins_list);  n++)
    {
        if (builtins_list[n]->init_mod != NULL  &&
            builtins_list[n]->init_mod() < 0)
        {
            if (err_notifier != NULL)
            {
                if (err_notifier(builtins_list[n]->magicnumber,
                                 builtins_list[n]->name) != 0)
                    return -1;
            }
            else
                fprintf(stderr, "%s %s(): %08x.%s.init_mod() failed\n",
                        strcurtime(), __FUNCTION__,
                        builtins_list[n]->magicnumber,
                        builtins_list[n]->name);
        }
    }

    return 0;
}

void TermBuiltins(void)
{
  int  n;

    for (n = countof(builtins_list) - 1;  n >= 0;  n--)
        if (builtins_list[n]->term_mod != NULL)
            builtins_list[n]->term_mod();
}
