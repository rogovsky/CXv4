#include "cxsd_db.h"
#include "cxsd_dbP.h"


CxsdDbLdrModRec *first_dbldr_metric = NULL;


CxsdDb  CxsdDbLoadDb(const char *argv0,
                     const char *def_scheme, const char *reference)
{
  CxsdDb           ret;
  char             scheme[20];
  const char      *location;
  CxsdDbLdrModRec *mp;

  int              devid;
  int              nsp_id;

    /* Decide with scheme */
    if (def_scheme != NULL  &&  *def_scheme == '!')
    {
        strzcpy(scheme, def_scheme + 1, sizeof(scheme));
        location = reference;
    }
    else
        split_url(def_scheme, "::",
                  reference, scheme, sizeof(scheme),
                  &location);

    /* Find appropriate loader */
    for (mp = first_dbldr_metric;  mp != NULL;  mp = mp->next)
        if (strcasecmp(scheme, mp->mr.name) == 0)
            break;
    if (mp == NULL) return NULL;

    /* Do load! */
    ret = mp->ldr(argv0, location);

    if (ret == NULL) return NULL;

    /* Perform final tuning */

    /* Linking */
    for (devid = 0;  devid < ret->numdevs;  devid++)
        if (ret->devlist[devid].type_nsp_id <= 0)
        {
            nsp_id = CxsdDbFindNsp(ret, 
                                   CxsdDbGetStr(ret, ret->devlist[devid].typename_ofs));
            if (nsp_id > 0)
                ret->devlist[devid].type_nsp_id = nsp_id;
        }
    
    return ret;
}


int  CxsdDbRegisterDbLdr  (CxsdDbLdrModRec *metric)
{
  CxsdDbLdrModRec *mp;

    if (metric == NULL)
        return -1;

    /* Check if it is already registered */
    for (mp = first_dbldr_metric;  mp != NULL;  mp = mp->next)
        if (mp == metric) return +1;

    /* Insert at beginning of the list */
    metric->next = first_dbldr_metric;
    first_dbldr_metric = metric;

    return 0;
}

int  CxsdDbDeregisterDbLdr(CxsdDbLdrModRec *metric)
{
}
