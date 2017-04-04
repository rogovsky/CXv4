#include <stdio.h>

#include "timeval_utils.h"

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/lebedev_ringcamsel_drv_i.h"


enum
{
    C_OUTREG8,
    C_IN_OUT,
    C_INPREG8,
    C_I_MOTOR,
    SUBORD_NUMCHANS
};

static vdev_sodc_dsc_t hw2our_mapping[SUBORD_NUMCHANS] =
{
    [C_OUTREG8] = {"outr8b", VDEV_PRIV, -1},
    [C_IN_OUT]  = {"outrb5", VDEV_TUBE, LEBEDEV_RINGCAMSEL_CHAN_IS_IN},
    [C_INPREG8] = {"inpr8b", VDEV_PRIV, -1},
    [C_I_MOTOR] = {"adc32",  VDEV_TUBE, LEBEDEV_RINGCAMSEL_CHAN_I_MOTOR},
};

static const char *devstate_names[] = {"_devstate"};

enum 
{
    POSITIONS_COUNT = LEBEDEV_RINGCAMSEL_POSITIONS_COUNT /* =24 */,
};

enum 
{
    SEL_WAIT_TIME   = 300,  // 300s to wait while stepping motor moves
    CHAN_TIME_R     = 1000, // channel<->seconds coeff
};

typedef struct {
    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];

    int32               lim_sw_known_vals[POSITIONS_COUNT][2];

    struct timeval      time_endwait;

    struct timeval      lim_sw_sensible_after;
} privrec_t;

static inline void SndCVal(privrec_t *me, int sodc, int32 val)
{
    cda_snd_ref_data(me->cur[sodc].ref, CXDTYPE_INT32, 1, &val);
}

//--------------------------------------------------------------------

static void lebedev_ringcamsel_sodc_cb(int devid, void *devptr,
                                       int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;
  int             pos;
  struct timeval  now;
  int32           stt;

    if      (sodc == C_OUTREG8)
    {
        pos = (((val >> 2) & 0x7) | ((val << 3) & 0x18))
            - 8 /* Because "Type" starts from 1, while 0 is "N/A" */;
        ReturnInt32Datum(devid, LEBEDEV_RINGCAMSEL_CHAN_CHOICE, pos, 0);

        gettimeofday(&(me->lim_sw_sensible_after), NULL);
        me->lim_sw_sensible_after.tv_sec += 1; // Let it be sensible 1s later
    }
    /* C_IN_OUT is taken care by vdev (TUBE) */
    else if (sodc == C_INPREG8)
    {
        stt = (val >> 1) & 7;
        /* 1. Return state */
        ReturnInt32Datum(devid, LEBEDEV_RINGCAMSEL_CHAN_STATE,
                         stt, 0);
        /* 2. Manifest lim_sw state */
        gettimeofday(&now, NULL);
        if (TV_IS_AFTER(now, me->lim_sw_sensible_after))
        {
            pos = (((me->cur[C_OUTREG8].v.i32 >> 2) & 0x7) | 
                   ((me->cur[C_OUTREG8].v.i32 << 3) & 0x18)) 
                - 8 /* Because "Type" starts from 1, while 0 is "N/A" */;
            if (pos >= 0  &&  pos < POSITIONS_COUNT)
            {
                ReturnInt32Datum(devid,
                                 LEBEDEV_RINGCAMSEL_CHAN_LIMIT_SW_UP_n_base + pos,
                                 me->lim_sw_known_vals[pos][0] = (stt == 4), 0);
                ReturnInt32Datum(devid,
                                 LEBEDEV_RINGCAMSEL_CHAN_LIMIT_SW_DN_n_base + pos,
                                 me->lim_sw_known_vals[pos][1] = (stt == 7), 0);
            }
        }
    }
}

static int lebedev_ringcamsel_init_d(int devid, void *devptr,
                                     int businfocount, int businfo[],
                                     const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;
  int             pos;

    SetChanRDs(devid, LEBEDEV_RINGCAMSEL_CHAN_TIME_LEFT, 1, CHAN_TIME_R, 0);
    SetChanReturnType(devid, LEBEDEV_RINGCAMSEL_CHAN_I_MOTOR,            
                      1, IS_AUTOUPDATED_YES);
    SetChanRDs(devid, LEBEDEV_RINGCAMSEL_CHAN_I_MOTOR, 1, 1000000.0, 0); /*!!! Should tube/tunnel source channel's */
//    SetChanReturnType(devid, LEBEDEV_RINGCAMSEL_CHAN_LIMIT_SW_UP_n_base,
//                      LEBEDEV_RINGCAMSEL_POSITIONS_COUNT, IS_AUTOUPDATED_TRUSTED);
//    SetChanReturnType(devid, LEBEDEV_RINGCAMSEL_CHAN_LIMIT_SW_DN_n_base,
//                      LEBEDEV_RINGCAMSEL_POSITIONS_COUNT, IS_AUTOUPDATED_TRUSTED);
    for (pos = 0;  pos < POSITIONS_COUNT;  pos++)
    {
        ReturnInt32Datum(devid, LEBEDEV_RINGCAMSEL_CHAN_LIMIT_SW_UP_n_base + pos,
                         me->lim_sw_known_vals[pos][0] = CX_VALUE_DISABLED_MASK, 0);
        ReturnInt32Datum(devid, LEBEDEV_RINGCAMSEL_CHAN_LIMIT_SW_DN_n_base + pos,
                         me->lim_sw_known_vals[pos][1] = CX_VALUE_DISABLED_MASK, 0);
    }

    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = hw2our_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = NULL;
    me->ctx.sodc_cb        = lebedev_ringcamsel_sodc_cb;

    return vdev_init(&(me->ctx), devid, devptr, -1, auxinfo);
}

static void lebedev_ringcamsel_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void lebedev_ringcamsel_rw_p(int devid, void *devptr,
                                    int action,
                                    int count, int *addrs,
                                    cxdtype_t *dtypes, int *nelems, void **values) 
{
  privrec_t      *me = devptr;
  int             n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int             chn;   // channel
  int32           val;

  struct timeval  now;
  struct timeval  timediff;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
        if (action == DRVA_WRITE)
        {
            if (nelems[n] != 1  ||
                (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            val = *((int32*)(values[n]));
            ////fprintf(stderr, " write #%d:=%d\n", chn, val);
        }
        else
            val = 0xFFFFFFFF; // That's just to make GCC happy

        if      (chn == LEBEDEV_RINGCAMSEL_CHAN_CHOICE)
        {
            if (action == DRVA_WRITE)
            {
                if (val < 0)                   val = 0;
                if (val > POSITIONS_COUNT - 1) val = POSITIONS_COUNT - 1;
                val += 8; /* Because "Type" starts from 1, while 0 is "N/A" */

                SndCVal(me, C_OUTREG8,
                        (me->cur[C_OUTREG8].v.i32 &~ 0x3F) /* bit5:=0, preserve bits 6,7 */
                        |
                        ((val & 0x18) >> 3) /* "Type" (E/R/P) lies in bits 0,1 */
                        |
                        ((val & 0x07) << 2) /* CameraN lies in bits 2,3,4 */
                        );
                me->lim_sw_sensible_after.tv_sec = 0;
            }
        }
        else if (chn == LEBEDEV_RINGCAMSEL_CHAN_IS_IN)
        {
            if (action == DRVA_WRITE)
            {
                SndCVal(me, C_IN_OUT, val & 1);

                /* Take care of "time remaining" channel */
                gettimeofday(&(me->time_endwait), NULL);
                me->time_endwait.tv_sec += SEL_WAIT_TIME;
                ReturnInt32Datum(devid, LEBEDEV_RINGCAMSEL_CHAN_TIME_LEFT,
                                 SEL_WAIT_TIME * CHAN_TIME_R, 0);
            }
        }
        else if (chn == LEBEDEV_RINGCAMSEL_CHAN_TIME_LEFT)
        {
            gettimeofday(&now, NULL);
            ReturnInt32Datum(devid, chn,
                             (timeval_subtract(&timediff,
                                               &(me->time_endwait),
                                               &now)) ? 0
                                                      : timediff.tv_sec * CHAN_TIME_R + 
                                                        timediff.tv_usec / (1000000 / CHAN_TIME_R),
                             0);
        }
        else if (chn >= LEBEDEV_RINGCAMSEL_CHAN_LIMIT_SW_UP_n_base  &&
                 chn <  LEBEDEV_RINGCAMSEL_CHAN_LIMIT_SW_UP_n_base + POSITIONS_COUNT)
        {
            ReturnInt32Datum(devid, chn,
                             me->lim_sw_known_vals[chn - LEBEDEV_RINGCAMSEL_CHAN_LIMIT_SW_UP_n_base][0],
                             0);
        }
        else if (chn >= LEBEDEV_RINGCAMSEL_CHAN_LIMIT_SW_DN_n_base  &&
                 chn <  LEBEDEV_RINGCAMSEL_CHAN_LIMIT_SW_DN_n_base + POSITIONS_COUNT)
        {
            ReturnInt32Datum(devid, chn,
                             me->lim_sw_known_vals[chn - LEBEDEV_RINGCAMSEL_CHAN_LIMIT_SW_DN_n_base][1],
                             0);
        }

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(lebedev_ringcamsel, "Lebedev's Ring CCD selector driver",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   lebedev_ringcamsel_init_d,
                   lebedev_ringcamsel_term_d,
                   lebedev_ringcamsel_rw_p);
