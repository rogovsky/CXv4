#include "cxsd_driver.h"


typedef struct
{
    int qqq;
} privrec_t;


static int  zzz_init_m(void)
{
    fprintf(stderr, "%s()\n", __FUNCTION__);
    return 0;
}

static void zzz_term_m(void)
{
    fprintf(stderr, "%s()\n", __FUNCTION__);
}

static int  zzz_init_d(int devid, void *devptr,
                       int businfocount, int businfo[],
                       const char *auxinfo)
{
    fprintf(stderr, "%s(devid=%d)\n", __FUNCTION__, devid);
    return 0;
}
static void zzz_term_d(int devid, void *devptr)
{
    fprintf(stderr, "%s(devid=%d)\n", __FUNCTION__, devid);
}


DEFINE_CXSD_DRIVER(zzz, "Zzz!!!",
                   zzz_init_m, zzz_term_m,
                   sizeof(privrec_t), NULL,
                   0, 10,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   zzz_init_d,
                   zzz_term_d,
                   NULL);
