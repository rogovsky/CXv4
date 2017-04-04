#ifndef __MISCLIB_H
#define __MISCLIB_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <stddef.h>

#include "misc_macros.h"
#include "misc_sepchars.h"


/* printf-style format parsing */
enum
{
    FMT_ALTFORM    = 1 << 0,
    FMT_ZEROPAD    = 1 << 1,
    FMT_LEFTADJUST = 1 << 2,
    FMT_SPACE      = 1 << 3,
    FMT_SIGN       = 1 << 4,
    FMT_GROUPTHNDS = 1 << 5
};

int ParseDoubleFormat (const char *fmt,
                       int  *flags_p,
                       int  *width_p,
                       int  *precision_p,
                       char *conv_c_p);
int CreateDoubleFormat(char *buf, size_t bufsize,
                       int   max_width, int max_precision,
                       int   flags,
                       int   width,
                       int   precision,
                       char  conv_c);
int         GetTextColumns(const char *dpyfmt, const char *units);
const char *GetTextFormat (const char *dpyfmt);

int ParseIntFormat    (const char *fmt,
                       int  *flags_p,
                       int  *width_p,
                       int  *precision_p,
                       char *conv_c_p);
int CreateIntFormat   (char *buf, size_t bufsize,
                       int   max_width, int max_precision,
                       int   flags,
                       int   width,
                       int   precision,
                       char  conv_c);
int         GetIntColumns (const char *dpyfmt, const char *units);
const char *GetIntFormat  (const char *dpyfmt);

char *printffmt_lasterr(void);


/* Multistring management */
enum
{
    MULTISTRING_SEPARATOR        = CX_US_C,
    MULTISTRING_OPTION_SEPARATOR = CX_SS_C,
};

int extractstring(const char *list, int n, char *buf, size_t bufsize);
int countstrings (const char *list);


/* "[SCHEME:]LOC" URLs parsing */
int split_url(const char  *def_scheme, const char *sep_str,
              const char  *url,
              char        *scheme_buf, size_t scheme_buf_size,
              const char **loc_p);


/* FD state management */
int check_fd_state(int fd, int rw);
int set_fd_flags  (int fd, int mask, int onoff);


/* Uninterruptible I/O */
ssize_t n_read (int fd, void       *buf, size_t count);
ssize_t n_write(int fd, const void *buf, size_t count);

/*
 *  uintr_read
 *      A read(2) substitute, which ignores EINTR.
 *      It doesn't try to read all the data requested (as n_read does),
 *      only as much as available.
 *
 *      uintr_read(fd) doesn't block if is called after select() which
 *      indicates that fd is ready for reading.
 *
 *      Is called from GenericReadFromClient() to read the next available
 *      portion of data.
 */

static inline ssize_t uintr_read(int s, void        *buf, size_t count)
{
  int  r;
  
    do {
        r = read(s, buf, count);
    } while (r < 0  &&  SHOULD_RESTART_SYSCALL());

    return r;
}

static inline ssize_t uintr_write(int s, const void *buf, size_t count)
{
  int  r;
  
    do {
        r = write(s, buf, count);
    } while (r < 0  &&  SHOULD_RESTART_SYSCALL());

    return r;
}


/* Portable replacement for signal() with a clearly-defined semantics */
int set_signal(int signum, void (*handler)(int));


/* ISO 8601 time presentation */
char *stroftime     (time_t when, char *dts);
char *stroftime_msc (struct timeval *when, char *dts);
char *strcurtime    (void);
char *strcurtime_msc(void);


/* Portable sleep() with microsecond precision */
void SleepBySelect(int usecs);


/* Buffer management */
int GrowBuf     (void **bufptr, size_t *sizeptr,    size_t newsize);
int GrowUnitsBuf(void **bufptr, int    *allocd_ptr, int    grow_to, size_t unit_size);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __MISCLIB_H */
