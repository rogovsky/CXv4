#include <stdio.h>

#include "cxsd_driver.h"
#include "vdev.h"


enum {NUM_AVG = 5};

enum
{
    C_SRC,
    SUBORD_NUMCHANS
};

static vdev_sodc_dsc_t hw2our_mapping[SUBORD_NUMCHANS] =
{
    [C_SRC] = {"", VDEV_PRIV, -1},
};

static const char *devstate_names[] = {"_devstate"};

typedef struct {
    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];

    int32               ring[NUM_AVG];
    int                 ring_pos;
} privrec_t;


static void lebedev_avg_sodc_cb(int devid, void *devptr,
                                int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;
  int             x;
  int32           avg_val;

    if (me->ring_pos < 0)
    {
        for (x = 0;  x < NUM_AVG;  x++) me->ring[x] = val;
        me->ring_pos = 0;
    }
    else
    {
        me->ring[me->ring_pos] = val;
        me->ring_pos = (me->ring_pos + 1) % NUM_AVG;
    }

    for (x = 0, avg_val = 0;  x < NUM_AVG;  x++)
        avg_val += me->ring[x] / NUM_AVG;

    ReturnInt32Datum(devid, 0, avg_val, me->cur[C_SRC].flgs);
}

static int lebedev_avg_init_d(int devid, void *devptr,
                              int businfocount, int businfo[],
                              const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;

    me->ring_pos = -1; // Signal "requires initialization"
    SetChanReturnType(devid, 0, 1, IS_AUTOUPDATED_YES);
    SetChanRDs       (devid, 0, 1, 1000000, 0); /*!!!*/

    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = hw2our_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = NULL;
    me->ctx.sodc_cb        = lebedev_avg_sodc_cb;

    return vdev_init(&(me->ctx), devid, devptr, -1, auxinfo);
}

static void lebedev_avg_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

DEFINE_CXSD_DRIVER(lebedev_avg, "Avg calculation for lebedev's Tout",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   lebedev_avg_init_d,
                   lebedev_avg_term_d,
                   NULL);
