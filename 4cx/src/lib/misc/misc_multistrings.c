#include <string.h>

#include "misclib.h"

/*
 *  extractstring
 *      Extracts a substring from a list.
 *      The list is a null-terminated string, with substrings separated
 *      by '\t's. The last substring must *not* be terminated with '\t'.
 */

int extractstring(const char *list, int n, char *buf, size_t bufsize)
{
  const char *p = list;
  const char *b;
  size_t      len;

    /* Sanity check */
    if (n < 0  ||  bufsize == 0) return -1;
  
    /* 1. Find the beginning of a segment */
    for (; n > 0; n--)
    {
        b = strchr(p, MULTISTRING_SEPARATOR);
        if (b == NULL)
        {
            *buf = '\0';
            return -1;
        }

        p = b + 1;
    }

    /* 2. Find the end of a segment */
    b = strchr(p, MULTISTRING_SEPARATOR);
    if (b == NULL) b = p + strlen(p); /* I hesitate to use strchr(p, 0) */
    len = b - p;
    if (len > bufsize - 1) len = bufsize - 1;

    /* Do copy */
    memcpy(buf, p, len);
    buf[len] = '\0';

    return 0;
}

int countstrings (const char *list)
{
  register const char *p;
  register int         count;
  
    for (p = list, count = 1;  *p != '\0';  p++)
        if (*p == MULTISTRING_SEPARATOR) count++;
    
    return count;
}

