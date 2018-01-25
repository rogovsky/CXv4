#ifndef __BIVME2_DEFINE_DRIVER_H
#define __BIVME2_DEFINE_DRIVER_H


#include "remdrvlet.h"

#include "bivme2_io.h"


#if defined(D) || defined(V)
  #error Sorry, unable to compile when D or V defined
  #error (see warning(s) below for details).
#endif

#ifdef __BIVME2_DEFINE_DRIVER_C
  #define D
  #define V(value) = value
#else
  #define D extern
  #define V(value)
#endif /* __BIVME2_DEFINE_DRIVER_C */


#define DEFINE_CXSD_DRIVER(name, comment,                           \
                           init_mod, term_mod,                      \
                           privrecsize, paramtable,                 \
                           min_businfo_n, max_businfo_n,            \
                           layer, layerver,                         \
                           extensions,                              \
                           chan_nsegs, chan_info, chan_namespace,   \
                           init_dev, term_dev, rdwr_p)              \
    CxsdDriverModRec CXSD_DRIVER_MODREC_NAME(name) =                \
    {                                                               \
        {                                                           \
            CXSD_DRIVER_MODREC_MAGIC, CXSD_DRIVER_MODREC_VERSION,   \
            __CX_STRINGIZE(name), comment,                          \
            init_mod, term_mod                                      \
        },                                                          \
        privrecsize, paramtable,                                    \
        min_businfo_n, max_businfo_n,                               \
        layer, layerver,                                            \
        extensions,                                                 \
        chan_nsegs, chan_info, chan_namespace,                      \
        init_dev, term_dev,                                         \
        rdwr_p                                                      \
    };                                                              \
    int main(void)                                                  \
    {                                                               \
      static int8  privrec[privrecsize];                            \
                                                                    \
        return remdrvlet_main(&CXSD_DRIVER_MODREC_NAME(name),       \
                              (privrecsize) != 0? privrec : NULL);  \
    }


/* VME/BIVME2-specific API should go here */


#undef D
#undef V


#endif /* __BIVME2_DEFINE_DRIVER_H */
