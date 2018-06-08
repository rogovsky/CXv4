#ifndef __CXLOGGER_H
#define __CXLOGGER_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <stdarg.h>


typedef struct
{
    char *name;  // Category name
    int   fref;  // Reference to filedesc #
    int   sync;  // Always do fsync() for this category
    int   is_on; // Whether this category is configured
} ll_catdesc_t;

typedef struct
{
    const char *path;  // Provided by caller
    int         fd;    // Managed by cxlogger
} ll_filedesc_t;


int   ll_init    (ll_catdesc_t *categories, int errno_mask,
                  const char *ident,
                  int verblevel, int consolity,
                  ll_filedesc_t *files);
int   ll_reopen  (int initial);
void  ll_access  (void);

void  ll_logline (int category, int level,
                  const char *subaddress, const char *format, ...)
                 __attribute__ ((format (printf, 4,5)));
void  ll_vlogline(int category, int level,
                  const char *subaddress, const char *format, va_list ap);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CXLOGGER_H */
