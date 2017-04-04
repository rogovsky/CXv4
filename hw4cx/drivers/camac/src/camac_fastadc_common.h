#include "misc_macros.h"
#include "pzframe_drv.h"


#ifndef FASTADC_NAME
  #error The "FASTADC_NAME" is undefined
#endif


#define FASTADC_PRIVREC_T __CX_CONCATENATE(FASTADC_NAME,_privrec_t)
#define FASTADC_PARAMS    __CX_CONCATENATE(FASTADC_NAME,_params)
#define FASTADC_LAM_CB    __CX_CONCATENATE(FASTADC_NAME,_lam_cb)
#define FASTADC_INIT_D    __CX_CONCATENATE(FASTADC_NAME,_init_d)
#define FASTADC_TERM_D    __CX_CONCATENATE(FASTADC_NAME,_term_d)
#define FASTADC_RW_P      __CX_CONCATENATE(FASTADC_NAME,_rw_p)


static void FASTADC_LAM_CB(int devid, void *devptr)
{
  FASTADC_PRIVREC_T   *me = (FASTADC_PRIVREC_T *)devptr;

    pzframe_drv_drdy_p(&(me->pz), 1, 0);
}

static int  FASTADC_INIT_D(int devid, void *devptr, 
                           int businfocount, int businfo[],
                           const char *auxinfo __attribute__((unused)))
{
  FASTADC_PRIVREC_T   *me = (FASTADC_PRIVREC_T *)devptr;
  const char          *errstr;

    me->devid  = devid;
    me->N_DEV  = businfo[0];

    pzframe_drv_init(&(me->pz), devid,
                     PARAM_SHOT, PARAM_ISTART, PARAM_WAITTIME, PARAM_STOP, PARAM_ELAPSED,
                     StartMeasurements, TrggrMeasurements,
                     AbortMeasurements, ReadMeasurements,
                     PrepareRetbufs);

    if ((errstr = WATCH_FOR_LAM(devid, devptr, me->N_DEV, FASTADC_LAM_CB)) != NULL)
    {
        DoDriverLog(devid, DRIVERLOG_ERRNO, "WATCH_FOR_LAM(): %s", errstr);
        return -CXRF_DRV_PROBL;
    }

    return InitParams(&(me->pz));
}

static void FASTADC_TERM_D(int devid, void *devptr)
{
  FASTADC_PRIVREC_T   *me = (FASTADC_PRIVREC_T *)devptr;

    pzframe_drv_term(&(me->pz));
}

static void FASTADC_RW_P (int devid, void *devptr,
                          int action,
                          int count, int *addrs,
                          cxdtype_t *dtypes, int *nelems, void **values)
{
  FASTADC_PRIVREC_T *me = devptr;

  int                n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int                chn;   // channel
  int                ct;
  int32              val;   // Value

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];

        if (chn < 0  ||  chn >= countof(chinfo)  ||
            (ct = chinfo[chn].chtype) == PZFRAME_CHTYPE_UNSUPPORTED)
        {
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);
        }
        else if (ct == PZFRAME_CHTYPE_BIGC)
        {
            if (chn == PARAM_DATA)
                me->data_rqd                        = 1;
            else
                me->line_rqd[chn-PARAM_LINE0] = 1;

            pzframe_drv_req_mes(&(me->pz));
        }
        else if (ct == PZFRAME_CHTYPE_MARKER)
        {/* Ignore */}
        else
        {
            if (action == DRVA_WRITE)
            {
                if (nelems[n] != 1  ||
                    (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                    goto NEXT_CHANNEL;
                val = *((int32*)(values[n]));
            }
            else
                val = 0xFFFFFFFF; // That's just to make GCC happy

            if      (ct == PZFRAME_CHTYPE_STATUS  ||  ct == PZFRAME_CHTYPE_AUTOUPDATED)
            {
                ReturnInt32Datum(devid, chn, me->cur_args[chn], 0);
            }
            else if (ct == PZFRAME_CHTYPE_VALIDATE)
            {
                if (action == DRVA_WRITE)
                    me->nxt_args[chn] = ValidateParam(&(me->pz), chn, val);
                ReturnInt32Datum(devid, chn, me->nxt_args[chn], 0);
            }
            else if (ct == PZFRAME_CHTYPE_INDIVIDUAL)
            {
                /* That's DEVTYPE channels, which are returned automatically
                   at init, so we don't need to do anything */
            }
            else /*  ct == PZFRAME_CHTYPE_PZFRAME_STD, which also returns UPSUPPORTED for unknown */
                ////fprintf(stderr, "PZFRAME_STD(%d,%d)\n", chn, val),
                pzframe_drv_rw_p  (&(me->pz), chn,
                                   action == DRVA_WRITE? val : 0,
                                   action);
        }

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(FASTADC_NAME, __CX_STRINGIZE(FASTADC_NAME) " fast-ADC",
                   NULL, NULL,
                   sizeof(FASTADC_PRIVREC_T), FASTADC_PARAMS,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   FASTADC_INIT_D, FASTADC_TERM_D, FASTADC_RW_P);
