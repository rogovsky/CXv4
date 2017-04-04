#include <stdio.h>

#include "timeval_utils.h"

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/lebedev_lincamsel_drv_i.h"


enum
{
#if 1
    C_BIT0, // Gun
    C_BIT1, // Turn
    C_BIT2, // FaradayCup
    C_BIT3, // Camera Last
#else
    C_BIT0, // Camera 1
    C_BIT1, // Cone
    C_BIT2, // Camera 2
    C_BIT3, // Camera 3
    C_BIT4, // Camera 4
    C_BIT5, // Faraday Cup
#endif
    SUBORD_NUMCHANS
};

static vdev_sodc_dsc_t hw2our_mapping[SUBORD_NUMCHANS] =
{
#if 1
    [C_BIT0] = {"outrb8",  VDEV_PRIV, -1},
    [C_BIT1] = {"outrb9",  VDEV_PRIV, -1},
    [C_BIT2] = {"outrb10", VDEV_PRIV, -1},
    [C_BIT3] = {"outrb11", VDEV_PRIV, -1},
#else
    [C_BIT0] = {"outrb0",  VDEV_PRIV, -1},
    [C_BIT1] = {"outrb10", VDEV_PRIV, -1},
    [C_BIT2] = {"outrb6",  VDEV_PRIV, -1},
    [C_BIT3] = {"outrb7",  VDEV_PRIV, -1},
    [C_BIT4] = {"outrb12", VDEV_PRIV, -1},
    [C_BIT5] = {"outrb9",  VDEV_PRIV, -1},
#endif
};

static const char *devstate_names[] = {"_devstate"};

enum 
{
#if 1
    REAL_BITS_COUNT = 4,
#else
    REAL_BITS_COUNT = 6,
#endif
    VIRT_BITS_COUNT = REAL_BITS_COUNT + 1,
};

#if 1
static int off_vals[VIRT_BITS_COUNT] = {0, 0, 0, 0, 0};
#else
static int off_vals[VIRT_BITS_COUNT] = {1, 0, 0, 0, 0, 0, 0};
#endif

enum 
{
#if 1
    SEL_WAIT_TIME   = 30,   // 30s to wait while stepping motor moves
#else
    SEL_WAIT_TIME   = 40,   // 40s to wait while stepping motor moves
#endif
    CHAN_TIME_R     = 1000, // channel<->seconds coeff
};

typedef struct {
    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];

    sl_tid_t            tid;
    struct timeval      time_endwait;
} privrec_t;

static int32 calc_camera_f_val(privrec_t *me)
{
  int             bit_n;
  int32           any;

    for (bit_n = 0, any = 0;  bit_n < REAL_BITS_COUNT;  bit_n++)
        any |= me->cur[C_BIT0 + bit_n].v.i32 ^ off_vals[bit_n];

    return any == 0;
}

static void lebedev_lincamsel_sodc_cb(int devid, void *devptr,
                                      int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;
  int             bit_n = sodc - C_BIT0;
  struct timeval  now;
  struct timeval  timediff;
  int32           disabled_mask;

    val ^= off_vals[bit_n]; // Invert if required
    gettimeofday(&now, NULL);
    disabled_mask =
        timeval_subtract(&timediff, &(me->time_endwait), &now)? 0
                                                              : CX_VALUE_DISABLED_MASK;

    ReturnInt32DatumTimed(devid, LEBEDEV_LINCAMSEL_CHAN_IN_n_base + bit_n,
                          val | disabled_mask,
                          me->cur[sodc].flgs,
                          me->cur[sodc].ts);
#if 1
    ReturnInt32Datum     (devid, LEBEDEV_LINCAMSEL_CHAN_CAMERA_NONE_IN,
#else
    ReturnInt32Datum     (devid, LEBEDEV_LINCAMSEL_CHAN_CAMERA_F_IN,
#endif
                          calc_camera_f_val(me) | disabled_mask,
                          0);
}

static void tout_p(int devid, void *devptr,
                   sl_tid_t tid __attribute__((unused)),
                   void *privptr)
{
  privrec_t      *me = (privrec_t *)devptr;
  int             bit_n;

    me->tid = -1;
    for (bit_n = 0;  bit_n < REAL_BITS_COUNT;  bit_n++)
    {
        ReturnInt32DatumTimed(devid, LEBEDEV_LINCAMSEL_CHAN_IN_n_base + bit_n,
                              me->cur[C_BIT0 + bit_n].v.i32 ^ off_vals[bit_n],
                              me->cur[C_BIT0 + bit_n].flgs,
                              me->cur[C_BIT0 + bit_n].ts);
    }
#if 1
    ReturnInt32Datum         (devid, LEBEDEV_LINCAMSEL_CHAN_CAMERA_NONE_IN,
#else
    ReturnInt32Datum         (devid, LEBEDEV_LINCAMSEL_CHAN_CAMERA_F_IN,
#endif
                              calc_camera_f_val(me),
                              0);
}

static int lebedev_lincamsel_init_d(int devid, void *devptr,
                                    int businfocount, int businfo[],
                                    const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;

    SetChanRDs(devid, LEBEDEV_LINCAMSEL_CHAN_TIME_LEFT, 1, CHAN_TIME_R, 0);

    me->tid = -1;

    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = hw2our_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = NULL;
    me->ctx.sodc_cb        = lebedev_lincamsel_sodc_cb;

    return vdev_init(&(me->ctx), devid, devptr, -1, auxinfo);
}

static void lebedev_lincamsel_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void lebedev_lincamsel_rw_p(int devid, void *devptr,
                                   int action,
                                   int count, int *addrs,
                                   cxdtype_t *dtypes, int *nelems, void **values) 
{
  privrec_t      *me = devptr;
  int             n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int             chn;   // channel
  int             bit_n;
  int32           out_bits[VIRT_BITS_COUNT];

  struct timeval  now;
  struct timeval  timediff;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];

        if      (chn >= LEBEDEV_LINCAMSEL_CHAN_IN_n_base  &&
                 chn <  LEBEDEV_LINCAMSEL_CHAN_IN_n_base + VIRT_BITS_COUNT)
        {
            if (action == DRVA_WRITE  &&
                (dtypes[0] == CXDTYPE_INT32  ||  dtypes[0] == CXDTYPE_UINT32))
            {
                /*
                      Note:
                          We "calculate" values of 7 bits (including a 
                          "fake/virtual" last one, "CAMERA_F_IN"),
                          but only 6 "real" are sent to hardware.
                          There's NO way to directly move-out the "CAMERA_F",
                          besides moving-in some other camera.
                 */

                /* 1. Calc values of all bits */
                for (bit_n = 0;  bit_n < VIRT_BITS_COUNT;  bit_n++)
                    out_bits[bit_n] = off_vals[bit_n];
                bit_n = chn - LEBEDEV_LINCAMSEL_CHAN_IN_n_base;
                out_bits[bit_n] = (
                                   (*((int32*)(values[0]))) ^ off_vals[bit_n]
                                  ) & 1;

                /* 2. Send bit-data to hardware */
                for (bit_n = 0;  bit_n < REAL_BITS_COUNT;  bit_n++)
                    cda_snd_ref_data(me->cur[C_BIT0 + bit_n].ref,
                                     CXDTYPE_INT32, 1,
                                     out_bits + bit_n);

                /* 3. Take care of "time remaining" channel */
                gettimeofday(&(me->time_endwait), NULL);
                me->time_endwait.tv_sec += SEL_WAIT_TIME;
                ReturnInt32Datum(devid, LEBEDEV_LINCAMSEL_CHAN_TIME_LEFT,
                                 SEL_WAIT_TIME * CHAN_TIME_R, 0);
                if (me->tid >= 0)
                {
                    sl_deq_tout(me->tid);
                    me->tid = -1;
                }
                me->tid = sl_enq_tout_at(devid, me,
                                         &(me->time_endwait), tout_p, NULL);
            }
        }
        else if (chn == LEBEDEV_LINCAMSEL_CHAN_TIME_LEFT)
        {
            gettimeofday(&now, NULL);
            ReturnInt32Datum(devid, LEBEDEV_LINCAMSEL_CHAN_TIME_LEFT,
                             (timeval_subtract(&timediff,
                                               &(me->time_endwait),
                                               &now)) ? 0
                                                      : timediff.tv_sec * CHAN_TIME_R + 
                                                        timediff.tv_usec / (1000000 / CHAN_TIME_R),
                             0);
        }
    }
}


DEFINE_CXSD_DRIVER(lebedev_lincamsel, "Lebedev's Linac CCD selector driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   lebedev_lincamsel_init_d,
                   lebedev_lincamsel_term_d,
                   lebedev_lincamsel_rw_p);
