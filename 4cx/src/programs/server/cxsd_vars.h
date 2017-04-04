// Creation-date: 29.06.2005

#ifndef __CXSD_VARS_H
#define __CXSD_VARS_H


#include <stdio.h>
#include <sys/time.h>

#include "cx.h"


#if defined(D) || defined(V)
  #error Sorry, unable to compile when D or V defined
  #error (see warning(s) below for details).
#endif

#ifdef __CXSD_VARS_C
  #define D
  #define V(value...) = value
#else
  #define D extern
  #define V(value...)
#endif /* __CXSD_VARS_C */


D int normal_exit       V(0);       /* := 1 in legal exit()s  to prevent
                                       syslog()s when exiting */
D char pid_file[PATH_MAX];
D int  pid_file_created V(0);


#undef D
#undef V


#endif /* __CXSD_VARS_H */
