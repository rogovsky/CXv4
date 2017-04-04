#ifndef __CM5307_CAMAC_H
#define __CM5307_CAMAC_H


#include "cx.h"


enum {CAMAC_X = 1, CAMAC_Q = 2};

int init_cm5307_camac(void);
int do_naf(int fd, int N, int A, int F, int *data);
int do_nafb(int fd, int N, int A, int F, int *data, int count);
int camac_setlam(int fd, int N, int onoff);

static inline rflags_t status2rflags(int status)
{
    return
        (status & CAMAC_Q? 0 : CXRF_CAMAC_NO_Q) |
        (status & CAMAC_X? 0 : CXRF_CAMAC_NO_X);
}

static inline rflags_t x2rflags(int status)
{
    return (status & CAMAC_X? 0 : CXRF_CAMAC_NO_X);
}


#endif /* __CM5307_CAMAC_H */
