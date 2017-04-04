#include "misclib.h"
#include "timeval_utils.h"
#include "cxsd_driver.h"

#include "drv_i/u0632_drv_i.h"


enum
{
    TIMEOUT_SECONDS = 10*0,
    WAITTIME_MSECS  = TIMEOUT_SECONDS * 1000,
};

enum {POLL_PERIOD = 100*1000*0+5*1000}; // 100ms

enum
{
    U0632_USECS = 30,   // 30us delay after read/write cycle NAF
    U0632_CADDR = 070,  // Control word's address
    U0632_4TEST = 32,   // A word for tests
};

static double r2q[4] = {1/(5*400.00e3), 1/(5*100.00e3), 1/(5*25.00e3), 1/(5*6.25e3)};

enum
{
    SETTING_P_MASK = 0x1F00,  SETTING_P_SHIFT = 8,  SETTING_P_ALWD = SETTING_P_MASK >> SETTING_P_SHIFT,
    SETTING_M_MASK = 0xC0,    SETTING_M_SHIFT = 6,  SETTING_M_ALWD = SETTING_M_MASK >> SETTING_M_SHIFT,
};

enum
{
    NUM_LINES      = 2,
    UNITS_PER_LINE = 15
};

static inline int unitn2unitcode(int n)
{
    return (n % UNITS_PER_LINE) + (1 << 4) * (n / UNITS_PER_LINE);
}

static inline int unitn2line    (int n)
{
    return n / UNITS_PER_LINE;
}

static inline int IPPvalue2int(int iv)
{
  int  ret = (iv & 16382) >> 1;

    if (iv & (1 << 14))
        ret |= (~(int)8191);

    return ret;
}

//////////////////////////////////////////////////////////////////////

static int loop_NAF(int N, int A, int F, int *data, int tries)
{
  int  t;
  int  status;
  
    for (t = 0, status = 0;
         t < tries  &&  (status & CAMAC_Q) == 0;
         t++)
        status = DO_NAF(CAMAC_REF, N, A, F, data);
  
    return status;
}

static int write_word(int N, int unit, int addr, int val)
{
  int  aw = ((addr & 0x3F) << 8) | unitn2unitcode(unit);
  int  status;
    
    status = DO_NAF(CAMAC_REF, N, 2, 16, &aw);  // Set address
    SleepBySelect(U0632_USECS);
    if ((status & (CAMAC_X|CAMAC_Q)) != (CAMAC_X|CAMAC_Q)) return status;

    status = DO_NAF(CAMAC_REF, N, 0, 16, &val); // Perform "write" cycle
    SleepBySelect(U0632_USECS);

    return status;
}

static int read_word (int N, int unit, int addr, int *v_p)
{
  int  aw = ((addr & 0x3F) << 8) | unitn2unitcode(unit);
  int  status;
  int  w;
    
    status = DO_NAF(CAMAC_REF, N, 2, 16, &aw);  // Set address
    SleepBySelect(U0632_USECS);
    if ((status & (CAMAC_X|CAMAC_Q)) != (CAMAC_X|CAMAC_Q)) return status;

    status = DO_NAF(CAMAC_REF, N, 0, 0, &w);    // Initiate "read" cycle
    SleepBySelect(U0632_USECS);
    status = DO_NAF(CAMAC_REF, N, 0, 0, &w);    // Read previously requested value from the buffer register
    SleepBySelect(U0632_USECS);

    *v_p = (~w) & 0xFFFE;                 // Convert value from "IPP format" to a normal one
    
    return status;
}

//////////////////////////////////////////////////////////////////////

enum
{
    BIAS_STATE_ABSENT    = 0,
    BIAS_STATE_PRESENT   = 1,
    BIAS_STATE_MEASURING = 2,
    BIAS_STATE_DROPPING  = 3,
    BIAS_STATE_STARTING  = 4,
};

typedef struct
{
    int             devid;
    int             N_DEV;
    sl_tid_t        tid;
    int             measuring_now;
    struct timeval  measurement_start;

    int32           unit_online[U0632_MAXUNITS];
    int32           unit_M     [U0632_MAXUNITS]; // "32" to be suitable
    int32           unit_P     [U0632_MAXUNITS]; // for ReturnChanGroup()
    rflags_t        unit_rflags[U0632_MAXUNITS];

    int32           unit_bias  [U0632_MAXUNITS][U0632_NUMRANGES][U0632_CELLS_PER_UNIT];

    int32           bias_count;

    int             bias_state;
    int             bias_mes_count;
    int             bias_mes_r;
    int             bias_mes_n;
} privrec_t;

static void SetM(privrec_t *me, int unit, int M)
{
    if ((M | SETTING_M_ALWD) == SETTING_M_ALWD)
    {
        me->unit_online[unit] = 1;
        me->unit_M     [unit] = M;
    }
    else
    {
        me->unit_online[unit] = 0;
        me->unit_M     [unit] = 0;
        me->unit_P     [unit] = 0;
    }
}

static void SetP(privrec_t *me, int unit, int P)
{
    if ((P | SETTING_P_ALWD) == SETTING_P_ALWD)
    {
        me->unit_online[unit] = 1;
        me->unit_P     [unit] = P;
    }
    else
    {
        me->unit_online[unit] = 0;
        me->unit_M     [unit] = 0;
        me->unit_P     [unit] = 0;
    }
}

static void ReturnOneBias(privrec_t *me, int unit, int range_n)
{
  int chan = 
      U0632_CHAN_BIAS_R0_n_base + range_n * U0632_MAXUNITS + unit;

  void *val_p = &(me->unit_bias[unit][range_n]);

  static cxdtype_t  dt_int32    = CXDTYPE_INT32;
  static int        nels        = U0632_CELLS_PER_UNIT;
  static rflags_t   zero_rflags = 0;

    ReturnDataSet(me->devid,
                  1,
                  &chan, &dt_int32, &nels,
                  &val_p, &zero_rflags, NULL);
}

static void ReturnBiases (privrec_t *me)
{
  int     unit;
  int     range_n;

    for (unit = 0;  unit < U0632_MAXUNITS; unit++)
        if (me->unit_online[unit])
            for (range_n = 0;  range_n < U0632_NUMRANGES;  range_n++)
                ReturnOneBias(me, unit, range_n);
}

//////////////////////////////////////////////////////////////////////

static void SwitchToABSENT   (privrec_t *me)
{
    me->bias_state = BIAS_STATE_ABSENT;
    bzero(me->unit_bias, sizeof(me->unit_bias));
    ReturnBiases(me);
    /*!!! Drop other params? */
    ReturnInt32Datum(me->devid, U0632_CHAN_BIAS_STEPS_LEFT, 0, 0);
    ReturnInt32Datum(me->devid, U0632_CHAN_BIAS_IS_ON,      0, 0);
}

static void SwitchToPRESENT  (privrec_t *me)
{
  int     unit;
  int     range_n;
  int     wire;
  int     count = me->bias_mes_count;

    if (count == 0) count = 1;

    for (unit = 0;  unit < U0632_MAXUNITS; unit++)
        if (me->unit_online[unit])
            for (range_n = 0;  range_n < U0632_NUMRANGES;  range_n++)
                for (wire = 0;  wire < U0632_CELLS_PER_UNIT; wire++)
                    me->unit_bias[unit][range_n][wire] /= count;

    me->bias_state = BIAS_STATE_PRESENT;
    ReturnBiases(me);
    ReturnInt32Datum(me->devid, U0632_CHAN_BIAS_STEPS_LEFT, 0, 0);
    ReturnInt32Datum(me->devid, U0632_CHAN_BIAS_CUR_COUNT,  me->bias_mes_count, 0);
    ReturnInt32Datum(me->devid, U0632_CHAN_BIAS_IS_ON,      1, 0);
}

static void SwitchToMEASURING(privrec_t *me)
{
    me->bias_state = BIAS_STATE_MEASURING;
    me->bias_mes_count = me->bias_count;
    me->bias_mes_r = U0632_NUMRANGES    - 1;
    me->bias_mes_n = me->bias_mes_count - 1;
    bzero(me->unit_bias, sizeof(me->unit_bias));
    ReturnInt32Datum(me->devid, U0632_CHAN_BIAS_STEPS_LEFT, 
                     me->bias_mes_r * me->bias_mes_count + me->bias_mes_n + 1,
                     0);
}

static void SwitchToNextBiasMeasurementStep(privrec_t *me)
{
    if      (me->bias_mes_n > 0)
        me->bias_mes_n--;
    else if (me->bias_mes_r > 0)
    {
        me->bias_mes_r--;
        me->bias_mes_n = me->bias_mes_count - 1;
    }
    else
    {
        SwitchToPRESENT(me);
        return; // to skip returning of U0632_CHAN_BIAS_STEPS_LEFT
    }

    ReturnInt32Datum(me->devid, U0632_CHAN_BIAS_STEPS_LEFT, 
                     me->bias_mes_r * me->bias_mes_count + me->bias_mes_n + 1,
                     0);
}

static void SwitchToDROPPING (privrec_t *me)
{
    me->bias_state = BIAS_STATE_DROPPING;
}

static void SwitchToSTARTING (privrec_t *me)
{
    me->bias_state = BIAS_STATE_STARTING;
    ReturnInt32Datum(me->devid, U0632_CHAN_BIAS_STEPS_LEFT, 
                     U0632_NUMRANGES * me->bias_count,
                     0);
}

static int  StartMeasurements(privrec_t *me)
{
  int     unit;
  int     line;
  int     c;
  int     the_m;
  int     the_p;
  int     status;
  int     line_active[NUM_LINES];

    /* Program individual boxes */
    bzero(line_active, sizeof(line_active));
    for (unit = 0;  unit < U0632_MAXUNITS;  unit++)
        if (me->unit_online[unit])
        {
            the_m = me->bias_state == BIAS_STATE_MEASURING? me->bias_mes_r
                                                          : me->unit_M[unit];
            the_p =                                         me->unit_P[unit];
            status = write_word(me->N_DEV, unit, U0632_CADDR,
                                (the_m << SETTING_M_SHIFT) |
                                (the_p << SETTING_P_SHIFT));
            
            line_active[unitn2line(unit)] = 1;
        }

    /* Enable start */
    for (line = 0;  line < NUM_LINES;  line++)
        if (line_active[line])
        {
            ////DoDriverLog(devid, 0, "Start %d", line);
            c = 040017 | (line << 4);
            status = DO_NAF(CAMAC_REF, me->N_DEV, 2,  16, &c);
            SleepBySelect(U0632_USECS);
            c = 0;
            status = DO_NAF(CAMAC_REF, me->N_DEV, 12, 16, &c);
            SleepBySelect(U0632_USECS);
        }

    return 0;
}

static int  AbortMeasurements(privrec_t *me)
{
    /* No real way to do this? */

    return 1;
}

static void ReadMeasurements (privrec_t *me)
{
  int       unit;
  int       c;
  int       junk;

  int       wire;
  int       cell;
  rflags_t  crflags;
  int32     wdata  [U0632_CELLS_PER_UNIT];

    SleepBySelect(7000); // 7ms delay -- 3.37ms max-delay plus ~2ms

    for (unit = 0;  unit < U0632_MAXUNITS;  unit++)
        if (me->unit_online[unit])
        {
            c = 0 + unitn2unitcode(unit);
            crflags  = status2rflags(loop_NAF(me->N_DEV, 0, 8, &junk, U0632_USECS));
            crflags |= status2rflags(DO_NAF(CAMAC_REF, me->N_DEV, 2, 16, &c));
            DO_NAF(CAMAC_REF, me->N_DEV, 1, 0, &c);

            for (cell = 0;  cell < U0632_CELLS_PER_UNIT;  cell++)
            {
                wire = (cell + U0632_CELLS_PER_UNIT-1) % U0632_CELLS_PER_UNIT;
                crflags |= status2rflags(loop_NAF(me->N_DEV, 0, 8, &junk, U0632_USECS));
                crflags |= status2rflags(DO_NAF(CAMAC_REF, me->N_DEV, 1, 0, &c));
                wdata  [wire] = IPPvalue2int(c);
            }
            if      (me->bias_state == BIAS_STATE_ABSENT  ||
                     me->bias_state == BIAS_STATE_PRESENT)
            {
                if (me->bias_state == BIAS_STATE_PRESENT)
                {
                    for (cell = 0;  cell < U0632_CELLS_PER_UNIT;  cell++)
                        wdata[cell] -= me->unit_bias[unit][me->unit_M[unit]][cell];
                }
                SetChanRDs   (me->devid, U0632_CHAN_DATA_n_base + unit, 1,
                              r2q[me->unit_M[unit]], 0.0);
                ReturnDataSet(me->devid,
                              3,
                              (int      []){U0632_CHAN_CUR_M_n_base + unit, U0632_CHAN_CUR_P_n_base + unit, U0632_CHAN_DATA_n_base + unit},
                              (cxdtype_t[]){CXDTYPE_INT32,                  CXDTYPE_INT32,                  CXDTYPE_INT32},
                              (int      []){1,                              1,                              U0632_CELLS_PER_UNIT},
                              (void*    []){me->unit_M              + unit, me->unit_P              + unit, wdata},
                              (rflags_t []){crflags,                        crflags,                        crflags},
                              NULL);
            }
            else if (me->bias_state == BIAS_STATE_MEASURING)
            {
                for (cell = 0;  cell < U0632_CELLS_PER_UNIT;  cell++)
                    me->unit_bias[unit][me->bias_mes_r][cell] += wdata[cell];
            }
        }

    if      (me->bias_state == BIAS_STATE_MEASURING)
        SwitchToNextBiasMeasurementStep(me);
    else if (me->bias_state == BIAS_STATE_DROPPING)
        SwitchToABSENT   (me);
    else if (me->bias_state == BIAS_STATE_STARTING)
        SwitchToMEASURING(me);
}

static void PerformReturn(privrec_t *me, rflags_t rflags)
{
    /* That is done in ReadMeasurements() as for now... */
}

//////////////////////////////////////////////////////////////////////

static void tout_p(int devid, void *devptr __attribute__((unused)),
                   sl_tid_t tid __attribute__((unused)),
                   void *privptr);

static void ReturnMeasurements (privrec_t *me, rflags_t rflags)
{
    ReadMeasurements(me);
    PerformReturn   (me, rflags);
}

static void SetDeadline(privrec_t *me)
{
  struct timeval   msc; // timeval-representation of MilliSeConds
  struct timeval   dln; // DeadLiNe

    if (WAITTIME_MSECS > 0)
    {
        msc.tv_sec  =  WAITTIME_MSECS / 1000;
        msc.tv_usec = (WAITTIME_MSECS % 1000) * 1000;
        timeval_add(&dln, &(me->measurement_start), &msc);
        
        me->tid = sl_enq_tout_at(me->devid, me,
                                 &dln,
                                 tout_p, me);
    }
}

static void RequestMeasurements(privrec_t *me)
{
    StartMeasurements(me);
    me->measuring_now = 1;
    gettimeofday(&(me->measurement_start), NULL);
    SetDeadline(me);
}

static void PerformTimeoutActions(privrec_t *me, int do_return)
{
    me->measuring_now = 0;
    if (me->tid >= 0)
    {
        sl_deq_tout(me->tid);
        me->tid = -1;
    }
    AbortMeasurements(me);
    if (do_return)
        ReturnMeasurements (me, CXRF_IO_TIMEOUT);

    RequestMeasurements(me);
}

static void tout_p(int devid, void *devptr __attribute__((unused)),
                   sl_tid_t tid __attribute__((unused)),
                   void *privptr)
{
  privrec_t *me = (privrec_t *)privptr;

    me->tid = -1;
    if (me->measuring_now == 0  ||
        WAITTIME_MSECS <= 0)
    {
        DoDriverLog(devid, DRIVERLOG_WARNING,
                    "strange timeout: measuring_now=%d, waittime=%d",
                    me->measuring_now,
                    WAITTIME_MSECS);
        return;
    }

    PerformTimeoutActions(me, 1);
}

static void drdy_p(privrec_t *me,  int do_return, rflags_t rflags)
{
    if (me->measuring_now == 0) return; 
    me->measuring_now = 0; 
    if (me->tid >= 0) 
    {
        sl_deq_tout(me->tid);  
        me->tid = -1; 
    }
    if (do_return)
        ReturnMeasurements (me, rflags); 
    RequestMeasurements(me);     
}

//////////////////////////////////////////////////////////////////////

static void poll_tout_p(int devid, void *devptr,
                        sl_tid_t tid,
                        void *privptr)
{
  privrec_t *me = (privrec_t *)devptr;

  int  junk;

    sl_enq_tout_after(me->devid, me, POLL_PERIOD, poll_tout_p, me);

    if (!me->measuring_now) return;
    if ((DO_NAF(CAMAC_REF, me->N_DEV, 3, 8, &junk) & CAMAC_Q) != 0)
        drdy_p(me, 1, 0);
} 

static int init_d(int devid, void *devptr,
                  int businfocount, int *businfo,
                  const char *auxinfo)
{
  privrec_t  *me = (privrec_t*)devptr;

  int     online_count;
  int     c;
  int     unit;
  int     s_cw, cword;
  int     s_w1, s_r1, r1;
  int     s_w2, s_r2, r2;
  int     range_n;

  enum {NUM_REPKOV_US = 26};

  enum {TEST_VAL_1 = 0xFF00, TEST_VAL_2 = 0xa5e1 & 0xFFFE};

    me->devid = devid;
    me->N_DEV = businfo[0];

    /* Determine which units are online and obtain their settings */
    /*!!! Maybe should broadcast a "stop" command? */
    c = 0;
    DO_NAF(CAMAC_REF, me->N_DEV, 2, 16, &c);
    DO_NAF(CAMAC_REF, me->N_DEV, 0, 16, &c);
    SleepBySelect(10000);

    SetChanReturnType(devid, U0632_CHAN_ONLINE_n_base, U0632_MAXUNITS, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, U0632_CHAN_DATA_n_base,   U0632_MAXUNITS, IS_AUTOUPDATED_YES);

    online_count = 0;
    for (unit = 0; unit < U0632_MAXUNITS;  unit++)
    {
        me->unit_online[unit] = 0;
        
        s_cw = read_word(me->N_DEV, unit, U0632_CADDR, &cword);

        s_w1 = s_r1 = r1 = s_w2 = s_r2 = r2 = 0;
        
        if (s_cw & CAMAC_Q)
        {
            s_w1 = write_word(me->N_DEV, unit, U0632_4TEST, TEST_VAL_1);
            s_r1 = read_word (me->N_DEV, unit, U0632_4TEST, &r1);
            s_w2 = write_word(me->N_DEV, unit, U0632_4TEST, TEST_VAL_2);
            s_r2 = read_word (me->N_DEV, unit, U0632_4TEST, &r2);

            me->unit_rflags[unit] = status2rflags(s_cw & s_w1 & s_r1 & s_w2 & s_r2);
            me->unit_online[unit] =
                ((s_cw & s_w1 & s_r1 & s_w2 & s_r2) == (CAMAC_X | CAMAC_Q)  &&
                 r1 == TEST_VAL_1  &&  r2 == TEST_VAL_2);
        }
        ReturnInt32Datum(devid,
                         U0632_CHAN_ONLINE_n_base + unit,
                         me->unit_online           [unit], 0);

        if (me->unit_online[unit])
        {
            online_count++;
            me->unit_M     [unit] = ((cword & SETTING_M_MASK) >> SETTING_M_SHIFT);
            me->unit_P     [unit] = ((cword & SETTING_P_MASK) >> SETTING_P_SHIFT);
        }
        else
        {
            me->unit_M     [unit] = 0;
            me->unit_P     [unit] = 0;
        };
        
        ////DoDriverLog(devid, 0, "Scan [%2d]: %d", unit, me->unit_online[unit]);
        ////DoDriverLog(devid, 0, "Scan [%2d]: %d: %d; %d,%d->%04x; %d,%d->%04x", unit, me->unit_online[unit], s_cw, s_w1, s_r1, r1, s_w2, s_r2, r2);
        DoDriverLog(devid, 0,
                    "Scan: %2d%c  cw=%04x/%c%c  w1=%c%c/%04x/%c%c  w2=%c%c/%04x/%c%c",
                    unit, me->unit_online[unit]?'+':' ',
                    cword,
                    (s_cw & CAMAC_X)? 'X':'-', (s_cw & CAMAC_Q)? 'Q':'-',
                    (s_w1 & CAMAC_X)? 'X':'-', (s_w1 & CAMAC_Q)? 'Q':'-',
                    r1,
                    (s_r1 & CAMAC_X)? 'X':'-', (s_r1 & CAMAC_Q)? 'Q':'-',
                    (s_w2 & CAMAC_X)? 'X':'-', (s_w2 & CAMAC_Q)? 'Q':'-',
                    r2,
                    (s_r2 & CAMAC_X)? 'X':'-', (s_r2 & CAMAC_Q)? 'Q':'-');
    }

    if (online_count == 0)
    {
        DoDriverLog(devid, 0, "No boxes found. Deactivating driver.");
        return DEVSTATE_OFFLINE;
    }

    for (unit = 0; unit < U0632_MAXUNITS;  unit++)
        if (me->unit_online[unit])
            for (range_n = 0;  range_n < U0632_NUMRANGES;  range_n++)
                SetChanRDs(me->devid,
                           U0632_CHAN_BIAS_R0_n_base + range_n * U0632_MAXUNITS + unit,
                           1,
                           r2q[range_n], 0.0);

    SetChanReturnType(devid, U0632_CHAN_BIAS_R0_n_base, U0632_MAXUNITS*U0632_NUMRANGES, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, U0632_CHAN_BIAS_IS_ON,      1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, U0632_CHAN_BIAS_STEPS_LEFT, 1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, U0632_CHAN_BIAS_CUR_COUNT,  1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, U0632_CHAN_CONST32,         1, IS_AUTOUPDATED_TRUSTED);
    ReturnInt32Datum (devid, U0632_CHAN_BIAS_IS_ON,      0, 0);
    ReturnInt32Datum (devid, U0632_CHAN_BIAS_STEPS_LEFT, 0, 0);
    ReturnInt32Datum (devid, U0632_CHAN_BIAS_CUR_COUNT,  me->bias_count = 5, 0);
    ReturnInt32Datum (devid, U0632_CHAN_CONST32,         U0632_CELLS_PER_UNIT, 0);
    SwitchToABSENT(me);

    sl_enq_tout_after(me->devid, me, POLL_PERIOD, poll_tout_p, me);
    
    RequestMeasurements(me);

    return DEVSTATE_OPERATING;
}

static void rdwr_p(int devid, void *devptr,
                   int action,
                   int count, int *addrs,
                   cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t  *me = (privrec_t*)devptr;

  int         n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int         chn;   // channel
  int         unit;
  int32       value;

  rflags_t    rflags;
  
    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];

        /* Note: if ever support for writable BIAS channels will be added,
           here an additional check should be performed */

        if (action == DRVA_WRITE)
        {
            if (nelems[n] != 1  ||
                (dtypes[n] != CXDTYPE_INT32  &&  dtypes[n] != CXDTYPE_UINT32))
                goto NEXT_CHANNEL;
            value = *((int32*)(values[n]));
            ////fprintf(stderr, " write #%d:=%d\n", chn, val);
        }
        else
            value = 0xFFFFFFFF; // That's just to make GCC happy

        switch (chn)
        {
            case U0632_CHAN_ONLINE_n_base ... U0632_CHAN_ONLINE_n_base+U0632_MAXUNITS-1:
                unit = chn - U0632_CHAN_ONLINE_n_base;
                ReturnInt32Datum(devid, chn, me->unit_online[unit], 0);
                break;

            case U0632_CHAN_M_n_base ... U0632_CHAN_M_n_base+U0632_MAXUNITS-1:
                unit = chn - U0632_CHAN_M_n_base;
                if (action == DRVA_WRITE) SetM(me, unit, value);
                rflags = me->unit_rflags[unit] |
                    (me->unit_online [unit]? 0 : CXRF_CAMAC_NO_Q);
                ReturnInt32Datum(devid, chn, me->unit_M[unit], rflags);
                break;
                
            case U0632_CHAN_P_n_base ... U0632_CHAN_P_n_base+U0632_MAXUNITS-1:
                unit = chn - U0632_CHAN_P_n_base;
                if (action == DRVA_WRITE) SetP(me, unit, value);
                rflags = me->unit_rflags[unit] |
                    (me->unit_online [unit]? 0 : CXRF_CAMAC_NO_Q);
                ReturnInt32Datum(devid, chn, me->unit_P[unit], rflags);
                break;

            case U0632_CHAN_BIAS_GET:
                if (action == DRVA_WRITE  &&  value == CX_VALUE_COMMAND)
                    SwitchToSTARTING(me);
                ReturnInt32Datum(devid, chn, 0, 0);
                break;

            case U0632_CHAN_BIAS_DROP:
                if (action == DRVA_WRITE  &&  value == CX_VALUE_COMMAND)
                    SwitchToDROPPING(me);
                ReturnInt32Datum(devid, chn, 0, 0);
                break;

            case U0632_CHAN_BIAS_COUNT:
                if (action == DRVA_WRITE)
                {
                    if (value < 1)    value = 1;
                    if (value > 1000) value = 1000;
                    me->bias_count = value;
                }
                ReturnInt32Datum (devid, chn,  me->bias_count, 0);
                break;
        }

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(u0632, "Repkov's KIPP",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   init_d, NULL, rdwr_p);

/*********************************************************************

  About U0632 programming

  I. Data model

  U0632 "IPP controller" supports up to 30 IPP units -- 2 lines of
  15 units each (4-bit addressing, with "15" used as broadcast address).
  Each unit measures 32 words, which are accessible at addresses 0-31.
  *Exact* use of that 32 measurements depends on particular application.
  (E.g., VEPP-5 uses 30 words -- 15 'X' wires and 15 'Y' wires.)
  

  II. NAF command set


  III. Notes

  1. When the line isn't terminated, the controller can erroneously "see"
  a reply from a missing unit.  So, just trusting Q in NA(0)F(0) isn't
  sufficient.  Besides mandatory terminators (on both ends of line!),
  for "who is there" scan V.Repkov recommends to write+read-back+compare
  some value in the unit's memory.

  2. V.Repkov also highly recommends to use A(0/1)F(8) to check for
  receiver/transmitter readyness (instead of testing Q after A(*)F(0/16)
  and retrying if Q=0).

  3. EVERYTHING read from "boxes" is read in complementary code, with bit 0
  replaced with parity bit.  I.e., after writing 0xFFFF one would read 0x0001,
  0x0000 returns as 0xFFFF.

  4. Memory map:
      - 0-31 hold measurements;
        wires 0-30 are put into words #1..31, while wire 32 is put into word #0.
        (That's because connector's wires are numbered from 1.)
      - 56 (070, 0x38) - status word.
      - All others are free for use.
  
  
*********************************************************************/
