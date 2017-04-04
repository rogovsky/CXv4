#include "Chl_includes.h"

chl_privrec_t *_ChlPrivOf(XhWindow win)
{
  chl_privrec_t *p = XhGetHigherPrivate(win);
  
    if (p == NULL)
    {
        p = malloc(sizeof(*p));
        if (p != NULL)
        {
            bzero(p, sizeof(*p));
            XhSetHigherPrivate(win, p);
        }
    }

    return p;
}
