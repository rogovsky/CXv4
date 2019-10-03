#include <stdio.h>
#include <fcntl.h>
#include <string.h>

#include <math.h>

#include "cxsd_driver.h"

#include "drv_i/pxi6363_drv_i.h"


// ---- daqmx_lyr.h --------------------------------------------------
#ifndef __DAQMX_LYR_H
#define __DAQMX_LYR_H


#include "misc_types.h"

// Following defines are to prevent NIDAQmx.h from typedefing types with the same names as from misc_types.h
#define _NI_int8_DEFINED_
#define _NI_int16_DEFINED_
#define _NI_int32_DEFINED_
#define _NI_float32_DEFINED_
#define _NI_float64_DEFINED_
#define _NI_int64_DEFINED_
#include <NIDAQmx.h>


//--------------------------------------------------------------------
#include <unistd.h>
#include <sys/syscall.h>

static inline pid_t my_gettid(void)
{
#ifdef SYS_gettid
    return syscall(SYS_gettid);
#else
#warning "SYS_gettid unavailable on this system, will always return 0"
    return 0;
#endif
}
//--------------------------------------------------------------------
// Lower byte - channel type (voltage/current/...)

enum
{
    DCK_RW_mask = 1 << 0,
    DCK_IS_RO = 0,
    DCK_IS_RW = 1,

    DCK_nature_mask = 0xFE, DCK_nature_shift = 1,
    
    DCK_nature_NONE     = 0,
    DCK_nature_VOLTAGE  = 1,
    DCK_nature_CURRENT  = 2,
};

#define ENCODE_DCK(rw, nature) \
    ((rw) | (nature << DCK_nature_shift))

static inline int rw_of_dck    (int k)
{
    return k & DCK_RW_mask;
}

static inline int nature_of_dck(int k)
{
    return (k & DCK_nature_mask) >> DCK_nature_shift;
}

enum
{
    DCK_NONE       = 0, // No actions by daqmx_lyr

    DCK_AI_VOLTAGE = ENCODE_DCK(DCK_IS_RO, DCK_nature_VOLTAGE),
    DCK_AO_VOLTAGE = ENCODE_DCK(DCK_IS_RW, DCK_nature_VOLTAGE),

    DCK_AI_CURRENT = ENCODE_DCK(DCK_IS_RO, DCK_nature_CURRENT),
};

enum
{
    DAQMX_MEASURING_NOTHING = 0,
    DAQMX_MEASURING_SCALAR  = 1,
    DAQMX_MEASURING_OSCILL  = 2,
};


typedef struct
{
    int options;
} daqmx_task_dsc_t;


typedef struct
{
    int         kind;
    const char *name_in_dev;
    cxdtype_t   dtype;
    int         max_nelems;
    int         task_n;
    int         base_chan;
} daqmx_chan_dsc_t;

typedef struct
{
} daqmx_chan_cur_t;

typedef struct
{
    void *p;
    int   n;
} daqmx_dpl_t;

typedef void (*DaqmxTaskDoneProc)(int devid, void *devptr,
                                  int task_n, int32 status);

//typedef int (*DaqmxAddDevice)(int devid, void *devptr,
//                              const char *dev_name);

typedef struct
{
    int               numchans;
    daqmx_chan_dsc_t *map;
    daqmx_chan_cur_t *cur;
    /* Tasks-related */
    int               numtasks;
    daqmx_task_dsc_t *task_map;
    TaskHandle       *tasks;
    daqmx_dpl_t      *dpls;
    int              *tctrs;

    /* daqmx_lyr-private data */
    int               devid;
    void             *devptr;
    DaqmxTaskDoneProc task_done;

    int               event_pipe[2];
    sl_fdh_t          event_rfdh;
} daqmx_context_t;


static int  daqmx_fini(daqmx_context_t *ctx);

static void daqmx_geterrdescr(char errorString[], uInt32 bufferSize);


#endif /* __DAQMX_LYR_H */
// -------------------------------------------------------------------
// ---- daqmx_lyr.c --------------------------------------------------

typedef struct
{
    int32  status;
    int32  task_n;
    int32  ctr;
    int32  rsrvd;
} event_info_t;

enum
{   /* this enumeration according to man(3) pipe */
    PIPE_RD_SIDE = 0,
    PIPE_WR_SIDE = 1,
};

static int32 EventCB(TaskHandle taskHandle, int32 status, void *callbackData)
{
  daqmx_dpl_t     *dp  = callbackData;
  daqmx_context_t *ctx = dp->p;
  event_info_t     info;

////fprintf(stderr, "%s tid=%d\n", __FUNCTION__, my_gettid());
    info.status = status;
    info.task_n = dp->n;
    info.ctr    = ctx->tctrs != NULL? ctx->tctrs[dp->n] : 0;

    write(ctx->event_pipe[PIPE_WR_SIDE], &info, sizeof(info));

    return 0;
}

static void event2pipe_proc(int uniq, void *privptr1,
                            sl_fdh_t fdh, int fd, int mask, void *privptr2)
{
  int              r;
  event_info_t     info;
  daqmx_context_t *ctx = privptr2;

    while ((r = read(fd, &info, sizeof(info))) > 0)
    {
        if (r != sizeof(info))
        {
            /*!!! Karaul!!!  */
            return;
        }

        if (ctx->task_done != NULL  &&
            (ctx->tctrs == NULL  ||  ctx->tctrs[info.task_n] == info.ctr))
            ctx->task_done(ctx->devid, ctx->devptr, info.task_n, info.status);
    }
}


static int  daqmx_init(daqmx_context_t *ctx,
                       int devid, void *devptr,
                       const char *dev_name,
                       int options,
                       DaqmxTaskDoneProc task_done)
{
  int    task_n;
  int    tn2;
  int    chan_n;

  char   chan_name[1024];

  int32  error;
  char   errbuf[2048];
  char  *er1;
  char  *er2;

////fprintf(stderr, "%s tid=%d\n", __FUNCTION__, my_gettid());
    ctx->devid  = devid;
    ctx->devptr = devptr;
    ctx->task_done = task_done;
    ctx->event_pipe[PIPE_RD_SIDE] = ctx->event_pipe[PIPE_WR_SIDE] = -1;
    ctx->event_rfdh = -1;

    if (pipe(ctx->event_pipe) < 0)
    {
        DoDriverLog(ctx->devid, DRIVERLOG_ERR | DRIVERLOG_ERRNO,
                    "%s: pipe()", __FUNCTION__);
        return -CXRF_DRV_PROBL;
    }
    set_fd_flags(ctx->event_pipe[PIPE_RD_SIDE], O_NONBLOCK, 1);
    set_fd_flags(ctx->event_pipe[PIPE_WR_SIDE], O_NONBLOCK, 1);
    if ((ctx->event_rfdh = sl_add_fd(ctx->devid, ctx->devptr,
                                     ctx->event_pipe[PIPE_RD_SIDE], SL_RD,
                                     event2pipe_proc, ctx)) < 0)
    {
        DoDriverLog(ctx->devid, DRIVERLOG_ERR | DRIVERLOG_ERRNO,
                    "%s: sl_add_fd()", __FUNCTION__);
        close(ctx->event_pipe[PIPE_RD_SIDE]); ctx->event_pipe[PIPE_RD_SIDE] = -1;
        close(ctx->event_pipe[PIPE_WR_SIDE]); ctx->event_pipe[PIPE_WR_SIDE] = -1;
        return -CXRF_DRV_PROBL;
    }

    for (task_n = 0;  task_n < ctx->numtasks;  task_n++)
    {
        error = DAQmxCreateTask("", ctx->tasks + task_n);
        if (DAQmxFailed(error))
        {
            daqmx_geterrdescr(errbuf, sizeof(errbuf));
            DoDriverLog(ctx->devid, DRIVERLOG_ERR,
                        "%s: DAQmxCreateTask(#%d): %s",
                        __FUNCTION__, task_n, errbuf);
            for (tn2 = 0;  tn2 < task_n;  tn2++)   // Note "<": tasks[task_n] was NOT created yet
                DAQmxClearTask(ctx->tasks[tn2]);
            return -CXRF_DRV_PROBL;
        }
        ctx->dpls[task_n].p = ctx;
        ctx->dpls[task_n].n = task_n;
        error = DAQmxRegisterDoneEvent(ctx->tasks[task_n], 0,
                                       EventCB, ctx->dpls + task_n);
        if (DAQmxFailed(error))
        {
            daqmx_geterrdescr(errbuf, sizeof(errbuf));
            DoDriverLog(ctx->devid, DRIVERLOG_ERR,
                        "%s: DAQmxRegisterDoneEvent(#%d): %s",
                        __FUNCTION__, task_n, errbuf);
            for (tn2 = 0;  tn2 <= task_n;  tn2++) // Note "<=": tasks[task_n] WAS created
                DAQmxClearTask(ctx->tasks[tn2]);
            return -CXRF_DRV_PROBL;
        }
    }

    for (chan_n = 0;  chan_n < ctx->numchans;  chan_n++)
        if (ctx->map[chan_n].kind != 0)
        {
            check_snprintf(chan_name, sizeof(chan_name), "%s/%s",
                           dev_name, ctx->map[chan_n].name_in_dev);
            er2   = chan_name;

            if      (ctx->map[chan_n].kind == DCK_AI_VOLTAGE)
            {
                er1   ="DAQmxCreateAIVoltageChan";
                error = DAQmxCreateAIVoltageChan(ctx->tasks[ctx->map[chan_n].task_n],
                                                 chan_name, "",
                                                 DAQmx_Val_Cfg_Default,
                                                 PXI6363_MIN_VALUE, PXI6363_MAX_VALUE, DAQmx_Val_Volts,
                                                 NULL);
                if (DAQmxFailed(error)) goto ERREXIT;
            }
            else if (ctx->map[chan_n].kind == DCK_AO_VOLTAGE)
            {
                er1   ="DAQmxCreateAOVoltageChan";
                error = DAQmxCreateAOVoltageChan(ctx->tasks[ctx->map[chan_n].task_n],
                                                 chan_name, "",
                                                 PXI6363_MIN_VALUE, PXI6363_MAX_VALUE, DAQmx_Val_Volts,
                                                 NULL);
                if (DAQmxFailed(error)) goto ERREXIT;
            }
        }

    return DEVSTATE_OPERATING;

 ERREXIT:
    daqmx_geterrdescr(errbuf, sizeof(errbuf));
    DoDriverLog(ctx->devid, DRIVERLOG_ERR,
                "%s: %s%s%s: %s",
                __FUNCTION__,
                er1, er2 != NULL? ":" : "", er2 != NULL? er2 : "",
                errbuf);
    daqmx_fini(ctx);
    return -CXRF_DRV_PROBL;
}

static int  daqmx_fini(daqmx_context_t *ctx)
{
  int    task_n;

    for (task_n = 0;  task_n < ctx->numtasks;  task_n++)
        DAQmxClearTask(ctx->tasks[task_n]);

    if (ctx->event_rfdh > 0)
    {
        sl_del_fd(ctx->event_rfdh); ctx->event_rfdh = -1;
    }

    if (ctx->event_pipe[PIPE_RD_SIDE] > 0)
    {
        close(ctx->event_pipe[PIPE_RD_SIDE]); ctx->event_pipe[PIPE_RD_SIDE] = -1;
    }
    if (ctx->event_pipe[PIPE_WR_SIDE] > 0)
    {
        close(ctx->event_pipe[PIPE_WR_SIDE]); ctx->event_pipe[PIPE_WR_SIDE] = -1;
    }

    return 0;
}


static void daqmx_geterrdescr(char errorString[], uInt32 bufferSize)
{
  size_t  l;

    DAQmxGetExtendedErrorInfo(errorString, bufferSize);
    errorString[bufferSize-1] = '\0';
    l = strlen(errorString);
    if (l > 0  &&  errorString[l-1] == '\n') errorString[l-1] = '\0';
}


// -------------------------------------------------------------------

enum
{
    TASK_N_ADC = 0,
    TASK_N_OUT = 1,
    TASK_N_INT = 2,

    NUMTASKS   = 3
};

typedef struct
{
    int               devid;

    int               measuring_now;
    int               any_line_rqd;
    int               line_rqd[PXI6363_NUM_ADC_LINES];

    daqmx_context_t   ctx;
    daqmx_chan_cur_t  cur_info[PXI6363_NUMCHANS];
    TaskHandle        tasks[NUMTASKS];
    daqmx_dpl_t       dpls [NUMTASKS];
    int               tctrs[NUMTASKS];

    int32             cur_numpts;
    int32             nxt_numpts;
    int32             cur_starts;
    int32             nxt_starts;
    int32             cur_timing;
    int32             nxt_timing;
    float64           cur_osc_rate;
    float64           nxt_osc_rate;

    float64           retdata[PXI6363_MAX_NUMPTS*PXI6363_NUM_ADC_LINES];
} privrec_t;

daqmx_task_dsc_t pxi6363_taskdescr[NUMTASKS] =
{
    [TASK_N_ADC] = {0},
    [TASK_N_OUT] = {0},
};

daqmx_chan_dsc_t pxi6363_chandescr[PXI6363_NUMCHANS] =
{
    [PXI6363_CHAN_ADC_n_base +  0] = {DCK_AI_VOLTAGE, "ai0",  CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base +  1] = {DCK_AI_VOLTAGE, "ai1",  CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base +  2] = {DCK_AI_VOLTAGE, "ai2",  CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base +  3] = {DCK_AI_VOLTAGE, "ai3",  CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base +  4] = {DCK_AI_VOLTAGE, "ai4",  CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base +  5] = {DCK_AI_VOLTAGE, "ai5",  CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base +  6] = {DCK_AI_VOLTAGE, "ai6",  CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base +  7] = {DCK_AI_VOLTAGE, "ai7",  CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base +  8] = {DCK_AI_VOLTAGE, "ai8",  CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base +  9] = {DCK_AI_VOLTAGE, "ai9",  CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base + 10] = {DCK_AI_VOLTAGE, "ai10", CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base + 11] = {DCK_AI_VOLTAGE, "ai11", CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base + 12] = {DCK_AI_VOLTAGE, "ai12", CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base + 13] = {DCK_AI_VOLTAGE, "ai13", CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base + 14] = {DCK_AI_VOLTAGE, "ai14", CXDTYPE_DOUBLE, 1, TASK_N_ADC},
    [PXI6363_CHAN_ADC_n_base + 15] = {DCK_AI_VOLTAGE, "ai15", CXDTYPE_DOUBLE, 1, TASK_N_ADC},

    [PXI6363_CHAN_OUT_n_base +  0] = {DCK_AO_VOLTAGE, "ao0",  CXDTYPE_DOUBLE, 1, TASK_N_OUT},
    [PXI6363_CHAN_OUT_n_base +  1] = {DCK_AO_VOLTAGE, "ao1",  CXDTYPE_DOUBLE, 1, TASK_N_OUT},
    [PXI6363_CHAN_OUT_n_base +  2] = {DCK_AO_VOLTAGE, "ao2",  CXDTYPE_DOUBLE, 1, TASK_N_OUT},
    [PXI6363_CHAN_OUT_n_base +  3] = {DCK_AO_VOLTAGE, "ao3",  CXDTYPE_DOUBLE, 1, TASK_N_OUT},
};

static void pxi6363_task_done(int devid, void *devptr,
                              int task_n, int32 status);

static int pxi6363_init_d(int devid, void *devptr,
                          int businfocount, int businfo[],
                          const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;
  int             devstate;

    me->ctx.numchans = PXI6363_NUMCHANS;
    me->ctx.map      = pxi6363_chandescr;
    me->ctx.cur      = me->cur_info;
    me->ctx.numtasks = NUMTASKS;
    me->ctx.task_map = pxi6363_taskdescr;
    me->ctx.tasks    = me->tasks;
    me->ctx.dpls     = me->dpls;
    me->ctx.tctrs    = me->tctrs;

    devstate = daqmx_init(&(me->ctx), devid, devptr, auxinfo, 0, pxi6363_task_done);
    if (devstate < 0) return devstate;

    me->nxt_numpts   = 1000;
    me->nxt_starts   = PXI6363_STARTS_INTERNAL;
    me->nxt_timing   = PXI6363_TIMING_INTERNAL;
    me->nxt_osc_rate = 10000.0;

    /*!!! SetChanReturnType() for requiring channels?
          Maybe in daqmx_init()?  Based on kind? */
    SetChanReturnType(me->devid, PXI6363_CHAN_OSC_n_base, PXI6363_CHAN_OSC_n_count, DO_IGNORE_UPD_CYCLE);

    return devstate;
}

static void pxi6363_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    daqmx_fini(&(me->ctx));
}

static void ReturnDoubleDatum(int devid, int chan, float64 v, rflags_t rflags)
{
  static cxdtype_t  dt_double = CXDTYPE_DOUBLE;
  static int        nels_1    = 1;

  void             *vp        = &v;

  ReturnDataSet(devid, 1,
                &chan, &dt_double, &nels_1,
                &vp, &rflags, NULL);
}
static void pxi6363_task_done(int devid, void *devptr,
                              int task_n, int32 status)
{
  privrec_t      *me = (privrec_t *)devptr;

  int            l;
  rflags_t       rflags;

  int32          spcr; // Samples Per Channel Read
  double         datavolts [PXI6363_CHAN_ADC_n_count];
  int            dataaddrs [PXI6363_CHAN_ADC_n_count];
  cxdtype_t      datadtypes[PXI6363_CHAN_ADC_n_count];
  int            datanelems[PXI6363_CHAN_ADC_n_count];
  void          *datavals_p[PXI6363_CHAN_ADC_n_count];
  rflags_t       datarflags[PXI6363_CHAN_ADC_n_count];

  int32          error;
  char           errbuf[2048];

    if (task_n != TASK_N_ADC) return;

    if      (me->measuring_now == DAQMX_MEASURING_NOTHING)
    {
        fprintf(stderr, "%s %s[%d](): measuring_now==0; task_n=%d status=%d",
                strcurtime_msc(), __FUNCTION__, devid, task_n, status);
        return;
    }

    else if (me->measuring_now == DAQMX_MEASURING_SCALAR)
    {
        error = DAQmxReadAnalogF64(me->tasks[task_n],
                                   1,
                                   0.0,
                                   DAQmx_Val_GroupByChannel,
                                   datavolts, countof(datavolts),
                                   &spcr,
                                   NULL);
        if (DAQmxFailed(error))
        {
            daqmx_geterrdescr(errbuf, sizeof(errbuf));
            DoDriverLog(me->devid, DRIVERLOG_ERR,
                        "%s: DAQmxReadAnalogF64(ADC): %s",
                        __FUNCTION__, errbuf);
            rflags = CXRF_CAMAC_NO_Q;
        }
        else
            rflags = 0;
    }
    else if (me->measuring_now == DAQMX_MEASURING_OSCILL)
    {
        error = DAQmxReadAnalogF64(me->tasks[task_n],
                                   me->cur_numpts,
                                   0.0,
                                   DAQmx_Val_GroupByChannel,
                                   me->retdata, countof(me->retdata),
                                   &spcr,
                                   NULL);
        if (DAQmxFailed(error))
        {
            daqmx_geterrdescr(errbuf, sizeof(errbuf));
            DoDriverLog(me->devid, DRIVERLOG_ERR,
                        "%s: DAQmxReadAnalogF64(ADC): %s",
                        __FUNCTION__, errbuf);
            rflags = CXRF_CAMAC_NO_Q;
        }
        else
            rflags = 0;

        for (l = 0;  l < PXI6363_NUM_ADC_LINES;  l++)
        {
            datavolts [l] = me->retdata[me->cur_numpts * l + me->cur_numpts-1];

            dataaddrs [l] = PXI6363_CHAN_OSC_n_base + l;
            datadtypes[l] = CXDTYPE_DOUBLE;
            datanelems[l] = me->cur_numpts;
            datavals_p[l] = me->retdata + me->cur_numpts * l;
            datarflags[l] = rflags;
        }
        ReturnDataSet(devid,
                      PXI6363_NUM_ADC_LINES,
                      dataaddrs,  datadtypes, datanelems,
                      datavals_p, datarflags, NULL);
        /*!!! CUR_NUMPTS, MARKER? */
        /*!!! Drop "requested" flags */
        me->any_line_rqd = 0;
        bzero(me->line_rqd, sizeof(me->line_rqd));

        ReturnDoubleDatum(devid, PXI6363_CHAN_ALL_RANGEMIN, PXI6363_MIN_VALUE, 0);
        ReturnDoubleDatum(devid, PXI6363_CHAN_ALL_RANGEMAX, PXI6363_MAX_VALUE, 0);
        ReturnInt32Datum (devid, PXI6363_CHAN_CUR_NUMPTS,   me->cur_numpts,    0);
        ReturnInt32Datum (devid, PXI6363_CHAN_MARKER,       0,                 0);
    }
    else
    {
        rflags = CXRF_CAMAC_NO_X;
        for (l = 0;  l < PXI6363_CHAN_ADC_n_count;  l++)
            datavolts[l] = NAN;
    }

    DAQmxStopTask(me->tasks[task_n]);

    me->measuring_now = DAQMX_MEASURING_NOTHING;

    for (l = 0;  l < PXI6363_CHAN_ADC_n_count;  l++)
    {
        dataaddrs [l] = PXI6363_CHAN_ADC_n_base + l;
        datadtypes[l] = CXDTYPE_DOUBLE;
        datanelems[l] = 1;
        datavals_p[l] = datavolts + l;
        datarflags[l] = rflags;
    }
    ReturnDataSet(devid,
                  PXI6363_CHAN_ADC_n_count,
                  dataaddrs,  datadtypes, datanelems,
                  datavals_p, datarflags, NULL);
}

static void StartMeasurements(privrec_t *me)
{
  int32  error;
  char   errbuf[2048];

    if (me->any_line_rqd)
    {
        me->cur_numpts   = me->nxt_numpts;
        me->cur_starts   = me->nxt_starts;
        me->cur_timing   = me->nxt_timing;
        me->cur_osc_rate = me->nxt_osc_rate;
        DAQmxCfgSampClkTiming(me->tasks[TASK_N_ADC],
                              "",
                              me->cur_osc_rate,
                              DAQmx_Val_Rising,
                              DAQmx_Val_FiniteSamps,
                              me->cur_numpts);
        me->measuring_now = DAQMX_MEASURING_OSCILL;
    }
    else
    {
        DAQmxCfgSampClkTiming(me->tasks[TASK_N_ADC],
                              "",
                              10000.0,
                              DAQmx_Val_Rising,
                              DAQmx_Val_FiniteSamps,
                              2); // "1" results in error, "2" is OK
        me->measuring_now = DAQMX_MEASURING_SCALAR;
    }

    me->tctrs[TASK_N_ADC]++;
    error = DAQmxStartTask(me->tasks[TASK_N_ADC]);
    if (DAQmxFailed(error))
    {
        daqmx_geterrdescr(errbuf, sizeof(errbuf));
        DoDriverLog(me->devid, DRIVERLOG_ERR,
                    "%s: DAQmxStartTask(ADC): %s",
                    __FUNCTION__, errbuf);
    }
}

static void pxi6363_rw_p(int devid, void *devptr,
                         int action,
                         int count, int *addrs,
                         cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t          *me = (privrec_t *)devptr;
  int                 n;       // channel N in addrs[]/.../values[] (loop ctl var)
  int                 chn;     // channel
  int                 l;       // Line #
  int32               value;
  double              dval;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
        if      (action == DRVA_WRITE  &&
                 ((chn >= PXI6363_CONFIG_DBL_WR_base  &&
                   chn <  PXI6363_CONFIG_DBL_WR_base + PXI6363_CONFIG_DBL_WR_count)
                  ||
                  (chn >= PXI6363_CHAN_OUT_n_base  &&
                   chn <  PXI6363_CHAN_OUT_n_base + PXI6363_CHAN_OUT_n_count)))
        {
            if (nelems[n] != 1  ||  dtypes[n] != CXDTYPE_DOUBLE)
                goto NEXT_CHANNEL;
            dval = *((float64*)(values[n]));
            value = 0xFFFFFFFF; // That's just to make GCC happy
        }
        else if (action == DRVA_WRITE  &&
                 (chn >= PXI6363_CHAN_OUT_TAB_n_base  &&
                  chn <  PXI6363_CHAN_OUT_TAB_n_base + PXI6363_CHAN_OUT_TAB_n_count))
        {
            if (dtypes[n] != CXDTYPE_DOUBLE)
                goto NEXT_CHANNEL;
            value = 0xFFFFFFFF; // That's just to make GCC happy
        }
        else if (action == DRVA_WRITE) /* Classic driver's dtype-check */
        {
            if (nelems[n] != 1  ||
                (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            value = *((int32*)(values[n]));
            ////fprintf(stderr, " write #%d:=%d\n", chn, val);
        }
        else
            value = 0xFFFFFFFF; // That's just to make GCC happy

        if      (chn >= PXI6363_CHAN_ADC_n_base  &&
                 chn <  PXI6363_CHAN_ADC_n_base + PXI6363_CHAN_ADC_n_count)
        {
            l = chn - PXI6363_CHAN_ADC_n_base;

            if (me->measuring_now == DAQMX_MEASURING_NOTHING)
                StartMeasurements(me);
        }
        else if (chn >= PXI6363_CHAN_OSC_n_base  &&
                 chn <  PXI6363_CHAN_OSC_n_base + PXI6363_CHAN_OSC_n_count)
        {
            l = chn - PXI6363_CHAN_OSC_n_base;
            me->line_rqd[l] = me->any_line_rqd = 1;
            //fprintf(stderr, "%s: l=%d measuring_now=%d\n", __FUNCTION__, l, me->measuring_now);
            if (me->measuring_now == DAQMX_MEASURING_NOTHING)
                StartMeasurements(me);
        }
        else if (chn >= PXI6363_CHAN_OUT_n_base  &&
                 chn <  PXI6363_CHAN_OUT_n_base + PXI6363_CHAN_OUT_n_count)
        {
            l = chn - PXI6363_CHAN_OUT_n_base;
        }
        else if (chn == PXI6363_CHAN_NUMPTS)
        {
            if (action == DRVA_WRITE)
            {
                if (value < 2)                  value = 2;
                if (value > PXI6363_MAX_NUMPTS) value = PXI6363_MAX_NUMPTS;
                me->nxt_numpts = value;
            }
            ReturnInt32Datum(devid, chn, me->nxt_numpts, 0);
        }
        else if (chn == PXI6363_CHAN_STARTS_SEL)
        {
            if (action == DRVA_WRITE)
            {
                if (value < 0) value = 0;
                if (value > 1) value = 1;
                me->nxt_starts = value;
            }
            ReturnInt32Datum(devid, chn, me->nxt_starts, 0);
        }
        else if (chn == PXI6363_CHAN_TIMING_SEL)
        {
            if (action == DRVA_WRITE)
            {
                if (value < 0) value = 0;
                if (value > 1) value = 1;
                me->nxt_timing = value;
            }
            ReturnInt32Datum(devid, chn, me->nxt_timing, 0);
        }
        else if (chn == PXI6363_CHAN_OSC_RATE)
        {
            if (action == DRVA_WRITE)
            {
                if (dval < 1.0)       dval = 1.0;
                if (dval > 1000000.0) dval = 1000000.0;
                me->nxt_osc_rate = dval;
            }
            ReturnDoubleDatum(devid, chn, me->nxt_osc_rate, 0);
        }
        else if (chn == PXI6363_CHAN_CUR_NUMPTS    ||
                 chn == PXI6363_CHAN_MARKER        ||
                 chn == PXI6363_CHAN_ALL_RANGEMIN  ||
                 chn == PXI6363_CHAN_ALL_RANGEMAX  ||
                 chn == PXI6363_CHAN_XS_PER_POINT)
        {/*DO-NOTHING*/}
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(pxi6363, "PXIe-6363",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   pxi6363_init_d, pxi6363_term_d, pxi6363_rw_p);
