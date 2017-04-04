/*********************************************************************
*                                                                    *
*  cx_sysdeps.h                                                      *
*                                                                    *
*********************************************************************/

#ifndef __CX_SYSDEPS_H
#define __CX_SYSDEPS_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


/* Set system-dependent OPTIONs */
#include "sysdeps/os_linux.h"
#include "sysdeps/os_cygwin.h"
#include "sysdeps/os_interix.h"
#include "sysdeps/os_openbsd.h"


/* For some strange reason these GNU variables aren't defined in any .h
   (as of RedHat-5.2 or 6.*)
   unless you are using glibc... */
#if OPTION_HAS_PROGRAM_INVOCATION_NAME
extern char *program_invocation_name;
extern char *program_invocation_short_name;
#endif /* OPTION_HAS_PROGRAM_INVOCATION_NAME */


/* For bzero() */
#if   defined(OS_IRIX)
  #include <bstring.h>
#elif defined(OS_SOLARIS)  ||  defined(OS_UNIXWARE)  ||  defined(OS_OSF)
  #include <strings.h>
#endif

/* For strcasecmp() to be defined later (on Unixware) */
#if OPTION_REQUIRES_STRCASECMP==1
  #include <ctype.h>
#endif

/* On Sun "unsigned long inet_addr()" instead of INADDR_NONE returns -1! */
#if OPTION_REQUIRES_INADDR_NONE
  #define INADDR_NONE 0xffffffff
#endif


/* SunOS is a so non-ANSI system, that it even forgets to declare many
   external variables in header files!
   The following declarations were copied from the man pages. */
#if OPTION_REQUIRES_OPTXXX
  extern char *optarg;
  extern int   optind, opterr;
#endif


/* SunOS is so old, that it doesn't have raise() */
#if OPTION_REQUIRES_RAISE
  #define raise(sig) kill(getpid(), sig)
#endif


/* BSDI, SUNOS & IRIX doesn't have strsignal().
   In fact, all of them have sys_siglist[NSIG], which can be used
   to obtain a signal description, but it isn't appropriately
   documented in IRIX (and is named `_sys_siglist' there). */
#if OPTION_REQUIRES_STRSIGNAL
  #define strsignal ___own_strsignal
  static inline char *strsignal(int sig)
  {
    static char description[100];

      sprintf(description, "Signal #%d", sig);
      return description;
  }
#endif

  
/* And SunOS doesn't even have a strerror() */
#if OPTION_REQUIRES_STRERROR
  #define strerror ___own_strerror
  static inline char *strerror(int errnum)
  {
    extern char *sys_errlist[];
    extern int   sys_nerr;
    static char  description[100];
      
      if (errnum > 0  &&  errnum <= sys_nerr)
          return sys_errlist[errnum];
      sprintf(description, "Undefined error: %d", errnum);
      return description;
  }
#endif


/* In Unixware strcasecmp() resides in some strange BSD compatibility
   library. The following code was taken from libc sources,
   string/strcasecmp.c */
#if OPTION_REQUIRES_STRCASECMP
  #define strcasecmp ___own_strcasecmp /* To escape conflicts with strings.h */
  static inline int strcasecmp(const char *s1, const char *s2)
  {
    register const unsigned char *p1   = (const unsigned char *) s1;
    register const unsigned char *p2   = (const unsigned char *) s2;
    register int                  ret;
    unsigned char                 c1;
      
      if (p1 == p2)
          return 0;
      
      for (; !(ret = (c1 = tolower(*p1)) - tolower(*p2)); p1++, p2++)
          if (c1 == '\0') break;
      return ret;
  }
#endif


/* Gcc versions prior to 2.7 have problems with __attribute__.
   Since we don't use any attributes except "format" and "unused",
   it is safe to ignore them. */
#if (__GNUC__ * 1000 + __GNUC_MINOR__) < 2007
  #define __attribute__(arg)
#endif
  

/* Now, the endian.h adventures... */
#ifdef OPTION_ENDIAN_H_LOCATION
  #include OPTION_ENDIAN_H_LOCATION
#elif defined(OS_SUNOS)
  /* Sorry, but this stupid system doesn't have an endian.h file! */
  /* The actual #defines will be derived from CPU type after this switch. */
#elif defined(OS_SOLARIS)
  /* Try to derive from gcc's fixed includes */
  #include <sys/byteorder.h>
  #if !defined(BYTE_ORDER)     &&  defined(__BYTE_ORDER__)
    #define BYTE_ORDER     __BYTE_ORDER__
  #endif
  #if !defined(LITTLE_ENDIAN)  &&  defined(__LITTLE_ENDIAN__)
    #define LITTLE_ENDIAN  __LITTLE_ENDIAN__
  #endif
  #if !defined(BIG_ENDIAN)     &&  defined(__BIG_ENDIAN__)
    #define BIG_ENDIAN     __BIG_ENDIAN__
  #endif
#else
  #error Oops, do not know where to get "endian.h"!
#endif

/* Last resort -- guess by CPU type */
#ifndef BYTE_ORDER
  #if defined(CPU_X86)  ||  defined(CPU_ALPHA)
    #define BYTE_ORDER    LITTLE_ENDIAN
  #else  /* In fact, Sparc or M68k; we don't care about switchable-endian MIPS */
    #define BYTE_ORDER    BIG_ENDIAN
  #endif
#endif
#ifndef LITTLE_ENDIAN
  #define LITTLE_ENDIAN 1234
#endif
#ifndef BIG_ENDIAN
  #define BIG_ENDIAN    4321
#endif

  
#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CX_SYSDEPS_H */
