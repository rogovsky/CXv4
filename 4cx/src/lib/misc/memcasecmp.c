#include <ctype.h>

#include "memcasecmp.h"


/* Copied from RedHat-7.3:SRPMS:fileutils-4.1/lib/memcasecmp.c */

#define TOUPPER toupper
int cx_memcasecmp (const void *vs1, const void *vs2, size_t n)
{
  unsigned int i;
  unsigned char const *s1 = (unsigned char const *) vs1;
  unsigned char const *s2 = (unsigned char const *) vs2;
  for (i = 0; i < n; i++)
    {
      unsigned char u1 = *s1++;
      unsigned char u2 = *s2++;
      if (TOUPPER (u1) != TOUPPER (u2))
        return TOUPPER (u1) - TOUPPER (u2);
    }
  return 0;
}


/* My own creatures */

/* Returns 0 upon match and !=0 otherwise */
int cx_strmemcmp    (const char *str, const void *mem, size_t memlen)
{
    return !(memcmp       (str, mem, memlen) == 0  &&  strlen(str) == memlen);
}

/* Returns 0 upon match and !=0 otherwise */
int cx_strmemcasecmp(const char *str, const void *mem, size_t memlen)
{
    return !(cx_memcasecmp(str, mem, memlen) == 0  &&  strlen(str) == memlen);
}

