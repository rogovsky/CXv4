#include <stdio.h>

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/lebedev_subharmonic_drv_i.h"


enum
{
    CAN_OUTR8B,
    SUBORD_NUMCHANS
};

static vdev_sodc_dsc_t hw2our_mapping[SUBORD_NUMCHANS] =
{
    [CAN_OUTR8B] = {"", VDEV_PRIV, -1},
};

static const char *devstate_names[] = {"_devstate"};

typedef struct {
    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];
} privrec_t;

static int32 reg2val(int32 regval)
{
    return
        (regval & (1 << 7)? 160 : 0) +
        (regval & (1 << 6)?  80 : 0) +
        (regval & (1 << 5)?  80 : 0) +
        (regval & (1 << 4)?  40 : 0) +
        (regval & (1 << 3)?  20 : 0) +
        (regval & (1 << 2)?  10 : 0) +
        (regval & (1 << 1)?   5 : 0) +
        (regval & (1 << 0)?   5 : 0);
}

static int32 val2reg(int32 subval)
{
  int32  regval;

    if (subval != 360)
        subval %= 360;
    if (subval < 0) subval += 360;

    regval = 0;
    if (subval >= 160) {subval -= 160; regval |= 1 << 7;}
    if (subval >=  80) {subval -=  80; regval |= 1 << 6;}
    if (subval >=  80) {subval -=  80; regval |= 1 << 5;}
    if (subval >=  40) {subval -=  40; regval |= 1 << 4;}
    if (subval >=  20) {subval -=  20; regval |= 1 << 3;}
    if (subval >=  10) {subval -=  10; regval |= 1 << 2;}
    if (subval >=   5) {subval -=   5; regval |= 1 << 1;}
    if (subval >=   5) {subval -=   5; regval |= 1 << 0;}

    return regval;
}

static void zebedev_subharmonic_sodc_cb(int devid, void *devptr,
                                        int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;

    ReturnInt32DatumTimed(devid, LEBEDEV_SUBHARMONIC_CHAN_SET,
                          reg2val(val),
                          me->cur[CAN_OUTR8B].flgs,
                          me->cur[CAN_OUTR8B].ts);
}

static int zebedev_subharmonic_init_d(int devid, void *devptr,
                                      int businfocount, int businfo[],
                                      const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;

    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = hw2our_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = NULL;
    me->ctx.sodc_cb        = zebedev_subharmonic_sodc_cb;

    return vdev_init(&(me->ctx), devid, devptr, -1, auxinfo);
}

static void zebedev_subharmonic_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void zebedev_subharmonic_rw_p(int devid, void *devptr,
                                     int action,
                                     int count, int *addrs,
                                     cxdtype_t *dtypes, int *nelems, void **values) 
{
  privrec_t *me = devptr;
  int32      regval;

    if (action == DRVA_WRITE  &&
        (dtypes[0] == CXDTYPE_INT32  ||  dtypes[0] == CXDTYPE_UINT32))
    {
        regval = val2reg(*((int32*)(values[0])));
        cda_snd_ref_data(me->cur[CAN_OUTR8B].ref, CXDTYPE_INT32, 1, &regval);
    }
}


DEFINE_CXSD_DRIVER(zebedev_subharmonic, "Lebedev's subharmonic ROUGH channel driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   zebedev_subharmonic_init_d,
                   zebedev_subharmonic_term_d,
                   zebedev_subharmonic_rw_p);
