#ifndef __CHLP_H
#define __CHLP_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <stdarg.h>


/* Error descriptions setting API */

void ChlClearErr(void);
void ChlSetErr  (const char *format, ...)
    __attribute__((format (printf, 1, 2)));
void ChlVSetErr (const char *format, va_list ap);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CHLP_H */
