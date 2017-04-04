#include <string.h>
#include <ctype.h>

#include "misclib.h"


int split_url(const char  *def_scheme, const char *sep_str,
              const char  *url,
              char        *scheme_buf, size_t scheme_buf_size,
              const char **loc_p)
{
  const char *p;
  size_t      sl;
    
    if (def_scheme == NULL) def_scheme = "";
    if (url        == NULL) url        = "";

    p = url;

    /* Skip all alphanumerics first */
    while (*p != '\0'  &&  isalnum(*p)) p++;

    /* Check... */
    if (memcmp(p, sep_str, strlen(sep_str)) == 0)
    {
        sl = p - url;
        if (sl > scheme_buf_size - 1)
            sl = scheme_buf_size - 1;
        memcpy(scheme_buf, url, p - url);
        scheme_buf[sl] = '\0';
        *loc_p = p + strlen(sep_str);
    }
    else
    {
        strzcpy(scheme_buf, def_scheme, scheme_buf_size);
        *loc_p = url;
    }
    
    return 0;
}
