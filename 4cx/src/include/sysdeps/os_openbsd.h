/*********************************************************************
*                                                                    *
*  os_openbsd.h                                                      *
*      Tested on 4.2                                                 *
*                                                                    *
*********************************************************************/

#if defined(OS_OPENBSD)  &&  !defined(__OS_OPENBSD_H)
#define __OS_OPENBSD_H


#define OPTION_REQUIRES_RAISE              0
#define OPTION_REQUIRES_STRCASECMP         0
#define OPTION_REQUIRES_STRSIGNAL          0
#define OPTION_REQUIRES_STRERROR           0

#define OPTION_REQUIRES_INADDR_NONE        0
#define OPTION_REQUIRES_OPTXXX             0

#define OPTION_HAS_RANDOM                  1
#define OPTION_HAS_PROGRAM_INVOCATION_NAME 0

#define OPTION_USE_ON_EXIT                 0
#define OPTION_USE_IOCTL_FOR_ASYNC         0
#define OPTION_MUST_SET_PID_FOR_ASYNC      1
#define OPTION_MUST_USE_NONBLOCK_FOR_ASYNC 0
#define OPTION_CYGWIN_BROKEN_WRITE_SIZE    0

#define OPTION_SA_FLAGS                    SA_RESTART

#define OPTION_ENDIAN_H_LOCATION           <machine/endian.h>

#define OPTION_CLONE_ATT                   0


#endif /* __OS_OPENBSD_H */
