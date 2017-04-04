#ifndef __CM5307_DEFINE_DRIVER_H
#define __CM5307_DEFINE_DRIVER_H


#include "remdrvlet.h"

#include "cm5307_camac.h"
#define DO_NAF  do_naf
#define DO_NAFB do_nafb
#define CAMAC_REF camac_fd


#if defined(D) || defined(V)
  #error Sorry, unable to compile when D or V defined
  #error (see warning(s) below for details).
#endif

#ifdef __CM5307_DEFINE_DRIVER_C
  #define D
  #define V(value) = value
#else
  #define D extern
  #define V(value)
#endif /* __CM5307_DEFINE_DRIVER_C */


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
        camac_fd = init_cm5307_camac();                             \
        /* Any error handling? */                                   \
                                                                    \
        return remdrvlet_main(&CXSD_DRIVER_MODREC_NAME(name),       \
                              (privrecsize) != 0? privrec : NULL);  \
    }


D int  camac_fd          V(-1);

typedef void (*CAMAC_LAM_CB_T)(int devid, void *devptr);
const char * WATCH_FOR_LAM(int devid, void *devptr, int N, CAMAC_LAM_CB_T cb);


#undef D
#undef V


#endif /* __CM5307_DEFINE_DRIVER_H */
