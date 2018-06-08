#ifndef __CX_MEMCASECMP_H
#define __CX_MEMCASECMP_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


/* Note: we use "CX" prefix in define to ensure no-clash for
 a case of a system-wide include with such functionality and naming */


#include <string.h>


int cx_memcasecmp (const void *vs1, const void *vs2, size_t n);

int cx_strmemcmp    (const char *str, const void *mem, size_t memlen);
int cx_strmemcasecmp(const char *str, const void *mem, size_t memlen);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CX_MEMCASECMP_H */
