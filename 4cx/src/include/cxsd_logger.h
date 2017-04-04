#ifndef __CXSD_LOGGER_H
#define __CXSD_LOGGER_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <stdarg.h>


#if defined(D) || defined(V)
  #error Sorry, unable to compile when D or V defined
  #error (see warning(s) below for details).
#endif

#ifdef __CXSD_LOGGER_C
  #define D
  #define V(value...) = value
#else
  #define D extern
  #define V(value...)
#endif /* __CXSD_LOGGER_C */


/* Values for logline.type, the log-category nambers */
typedef enum {
    /* Logfiles */
    LOGF_SYSTEM   = 0,           /* System messages */
    LOGF_ACCESS   = 1,           /* Access log */
    LOGF_DEBUG    = 2,           /* Debug log */
    LOGF_MODULES  = 3,           /* Driver-API debug log (concerns modules' state and operation) */
    LOGF_HARDWARE = 4,           /* Drivers'/layers' debug log */

    LOGF_count    = 5,

    LOGF_DEFAULT  = LOGF_SYSTEM, /* Uncategorized log */

    /* Logging options */
    LOGF_ERRNO   = 1 << 16       /* Add the ": "+strerror(errno) at the end */
} logtype_t;

/* Values for logline.level (yes, these are unified with syslog()'s LOG_*) */
enum
{
    LOGL_EMERG   = 0,
    LOGL_ALERT   = 1,
    LOGL_CRIT    = 2,
    LOGL_ERR     = 3,
    LOGL_WARNING = 4,
    LOGL_NOTICE  = 5,
    LOGL_INFO    = 6,
    LOGL_DEBUG   = 7,
};


D int             cxsd_logger_verbosity  V(9);


int   cxsd_log_init(const char *argv0, int verbosity, int consolity);
char *cxsd_log_setf(int cat_mask, const char *path);

void  logline (logtype_t type, int level, const char *format, ...)
               __attribute__ ((format (printf, 3, 4)));
void vloglineX(logtype_t type, int level,
               const char *subaddress, const char *format, va_list ap);


#undef D
#undef V


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CXSD_LOGGER_H */
