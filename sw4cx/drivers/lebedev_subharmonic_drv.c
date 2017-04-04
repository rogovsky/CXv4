#include "cxsd_driver.h"
#include "cda.h"

#include "drv_i/lebedev_subharmonic_drv_i.h"


typedef struct
{
    cda_context_t  cid;
    cda_dataref_t  data_r;
    cda_dataref_t  stat_r;
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

static void data_evproc(int            devid,
                        void          *devptr,
                        cda_dataref_t  ref,
                        int            reason,
                        void          *info_ptr,
                        void          *privptr2)
{
  privrec_t  *me = devptr;
  int32       val;
  rflags_t    rflags;
  cx_time_t   timestamp;
  void *valp = &val;

    cda_get_ref_data(ref, 0, sizeof(val), &val);
    cda_get_ref_stat(ref, &rflags, &timestamp);
    val = reg2val(val);

    ReturnInt32DatumTimed(devid, LEBEDEV_SUBHARMONIC_CHAN_SET, val,
                          rflags, timestamp);
}

static void stat_evproc(int            devid,
                        void          *devptr,
                        cda_dataref_t  ref,
                        int            reason,
                        void          *info_ptr,
                        void          *privptr2)
{
  privrec_t  *me = devptr;
  int32       val;

    cda_get_ref_data(ref, 0, sizeof(val), &val);

    if (val == DEVSTATE_OFFLINE) val = DEVSTATE_NOTREADY;
    SetDevState(devid, val, 0, NULL);
}

static int  lebedev_subharmonic_init_d(int devid, void *devptr,
                                       int businfocount, int businfo[],
                                       const char *auxinfo)
{
  privrec_t  *me = devptr;

    if (auxinfo == NULL)
    {
        DoDriverLog(devid, 0, "%s(): empty auxinfo", __FUNCTION__);
        return -CXRF_CFG_PROBL;
    }
    if ((me->cid = cda_new_context(devid, devptr,
                                   "insrv::", 0,
                                   NULL,
                                   0, NULL, 0)) < 0)
    {
        DoDriverLog(devid, 0, "cda_new_context(): %s", cda_last_err());
        return -CXRF_DRV_PROBL;
    }
    if ((me->data_r = cda_add_chan(me->cid, NULL, auxinfo, 0,
                                   CXDTYPE_INT32, 1,
                                   CDA_REF_EVMASK_UPDATE,
                                   data_evproc, NULL)) < 0)
    {
        DoDriverLog(devid, 0, "cda_new_chan(\"%s\"): %s",
                    auxinfo, cda_last_err());
        cda_del_context(me->cid);    me->cid = -1;
        return -CXRF_DRV_PROBL;
    }
    me->stat_r = cda_add_chan(me->cid, auxinfo, "_devstate", 0,
                              CXDTYPE_INT32, 1,
                              CDA_REF_EVMASK_UPDATE,
                              stat_evproc, NULL);

    /* "Return" currently-known value */
    data_evproc(devid, devptr, me->data_r, CDA_REF_R_UPDATE, NULL, NULL);

    return DEVSTATE_OPERATING;
}

static void lebedev_subharmonic_term_d(int devid, void *devptr)
{
  privrec_t *me = devptr;

    cda_del_context(me->cid); me->cid = -1;
    me->data_r = -1;
    me->stat_r = -1;
}

static void lebedev_subharmonic_rw_p(int devid, void *devptr,
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
        cda_snd_ref_data(me->data_r, CXDTYPE_INT32, 1, &regval);
    }
}

DEFINE_CXSD_DRIVER(lebedev_subharmonic, "Lebedev's subharmonic ROUGH channel driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   lebedev_subharmonic_init_d,
                   lebedev_subharmonic_term_d,
                   lebedev_subharmonic_rw_p);
