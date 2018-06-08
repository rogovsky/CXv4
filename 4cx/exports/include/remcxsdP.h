#ifndef __REMCXSDP_H
#define __REMCXSDP_H


#include "remcxsd.h"


#if defined(D) || defined(V)
  #error Sorry, unable to compile when D or V defined
  #error (see warning(s) below for details).
#endif

#ifdef __REMCXSD_DRIVER_V4_C
  #define D
  #define V(value) = value
#else
  #define D extern
  #define V(value)
#endif /* __REMCXSD_DRIVER_V4_C */


/*==== A little bit of macros for interaction with drivers =========*/

D int active_devid V(DEVID_NOT_IN_DRIVER);

#define ENTER_DRIVER_S(devid, s) do {                              \
    s=active_devid; active_devid=devid;                            \
    if (devid >= 0               &&                                \
        devid < remcxsd_maxdevs  &&                                \
        remcxsd_devices[devid].in_use)                             \
        remcxsd_devices[devid].being_processed++;                  \
} while (0)
#define LEAVE_DRIVER_S(s)        do {                              \
    if (active_devid >= 0               &&                         \
        active_devid < remcxsd_maxdevs  &&                         \
        remcxsd_devices[active_devid].in_use)                      \
    {                                                              \
        remcxsd_devices[active_devid].being_processed--;           \
        if (remcxsd_devices[active_devid].being_processed == 0  && \
            remcxsd_devices[active_devid].being_destroyed)         \
            FreeDevID(active_devid);                               \
    }                                                              \
    active_devid=s;                                                \
} while (0)


#define CHECK_LOG_CORRECT(condition, correction, format, params...)    \
    if (condition)                                                     \
    {                                                                  \
        DoDriverLog(devid, 0, "%s():" format, __FUNCTION__, ##params); \
        correction;                                                    \
    }

#define DO_CHECK_SANITY_OF_DEVID(func_name, errret)                    \
    do {                                                               \
        if (devid < 0  ||  devid >= remcxsd_maxdevs)                   \
        {                                                              \
            DoDriverLog(DEVID_NOT_IN_DRIVER, 0,                        \
                        "%s(): devid=%d(active=%d), out_of[%d...%d]",  \
                        func_name, devid, active_devid, 0, remcxsd_maxdevs - 1);  \
            return errret;                                             \
        }                                                              \
        if (dev->in_use == 0)                                          \
        {                                                              \
            DoDriverLog(DEVID_NOT_IN_DRIVER, 0,                        \
                        "%s(): devid=%d(active=%d) is inactive",       \
                        func_name, devid, active_devid);               \
            return errret;                                             \
        }                                                              \
    } while (0)

#define CHECK_SANITY_OF_DEVID(errret) \
    DO_CHECK_SANITY_OF_DEVID(__FUNCTION__, errret)


#undef D
#undef V


#endif /* __REMCXSDP_H */
