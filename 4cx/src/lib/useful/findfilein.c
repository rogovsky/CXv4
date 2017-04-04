#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <sys/param.h> // For PATH_MAX, at least on ARM@MOXA

#include "misc_macros.h"
#include "findfilein.h"


static void *builtin_fopen_checker(const char *name,
                                   const char *path,
                                   void       *privptr)
{
  char  buf[PATH_MAX];
  char  plen = strlen(path);
  
    check_snprintf(buf, sizeof(buf), "%s%s%s%s",
                   path,
                   plen == 0  ||  path[plen-1] == '/'? "" : "/",
                   name,
                   privptr == NULL? "" : (char *)privptr);
    return fopen(buf, "r");
}

void *findfilein(const char            *name,
                 const char            *argv0,
                 findfilein_check_proc  checker, void *privptr,
                 const char            *search_path)
{
  void       *ret = NULL;
  const char *cur_p;
  const char *sep_p;
  const char *s;
  size_t      slen;  // Note:  slen can never be negative, due to the way it is calculated
  size_t      plen;  // ...and plen too (its max is sizeof(buf)-1-slen, slen's max is sizeof(buf)-1, so plen's min is 0
  const char *p1;
  char        buf[PATH_MAX];
  char        a0d[PATH_MAX];  // Argv0-Dir

    if (search_path == NULL) return NULL;

    if (checker == NULL) checker = builtin_fopen_checker;

    cur_p = search_path;
    while (ret == NULL)
    {
        if (*cur_p == '\0') break;
        /* Find a separator... */
        sep_p = strchr(cur_p, FINDFILEIN_SEP_CHAR);
        /* ...or EOL */
        if (sep_p == NULL) sep_p = cur_p + strlen(cur_p);

        /* Skip empty strings */
        if (cur_p == sep_p) goto NEXT_STEP;

        /* Is it something special? */
        s = NULL;
        if      (cur_p[0] == '~'  &&  (cur_p + 1 == sep_p  ||  cur_p[1] == '/'))
        {
            s = getenv("HOME");
            if (s == NULL) goto NEXT_STEP;
        }
        else if (cur_p[0] == '!'  &&  (cur_p + 1 == sep_p  ||  cur_p[1] == '/'))
        {
            s = argv0;
            if (s == NULL) goto NEXT_STEP;
            p1 = strrchr(s, '/');
            if (p1 == NULL) goto NEXT_STEP;
            slen = p1 - s;
            if (slen > sizeof(a0d) - 1) slen = sizeof(a0d) - 1;
            if (slen > 0) memcpy(a0d, s, slen);
            a0d[slen] = '\0';

            s = a0d;
        }
        else if (cur_p[0] == '$')
        {
            s = cur_p + 1;
            p1 = memchr(s, '/', sep_p - s);
            if (p1 == NULL) p1 = sep_p;
            slen = p1 - s;
            if (slen > sizeof(buf) - 1) slen = sizeof(buf) - 1;
            if (slen > 0) memcpy(buf, s, slen);
            buf[slen] = '\0';

            s = getenv(buf);
            if (s == NULL) goto NEXT_STEP;
        }

        /* ...is it? */
        if (s != NULL)
        {
            p1 = memchr(cur_p, '/', sep_p - cur_p);
            if (p1 == NULL) p1 = sep_p; // Just "something", not "something/..."?
            plen = sep_p - p1;          // Size of segment after special-prefix; this size includes [optional] separating '/'

            strzcpy(buf, s, sizeof(buf));

            slen = strlen(buf);
            if (plen > sizeof(buf) - 1 - slen)
                plen = sizeof(buf) - 1 - slen;
            if (plen > 0) memcpy(buf + slen, p1, plen);
            buf[slen + plen] = '\0';
        }
        else
        {
            plen = sep_p - cur_p;
            if (plen > sizeof(buf) - 1) plen = sizeof(buf) - 1;
            memcpy(buf, cur_p, plen);
            buf[plen] = '\0';
        }

        ret = checker(name, buf, privptr);
 NEXT_STEP:
        if (*sep_p == '\0') break;
        cur_p = sep_p + 1;
    }

    return ret;
}
