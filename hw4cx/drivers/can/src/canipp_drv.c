//#include <stdio.h>
//#include <unistd.h>
//#include <stdlib.h>

#include "cxsd_driver.h"
#include "cankoz_lyr.h"

#include "drv_i/canipp_drv_i.h"


enum
{
    TIMEOUT_SECONDS = 10*0,
    WAITTIME_MSECS  = TIMEOUT_SECONDS * 1000,
};

enum {CANIPP_CADDR = 070};  // Control word's address

enum {CANIPP_DATUM_ABSENT = 0xFFFF0000}; // A value which can NOT be received from hardware, thus signifying "not received yet"

/* CANIPP specifics */

enum
{
    DEVTYPE   = 9, /* CANIPP(-M) is 9 */
};

enum
{
    DESC_WRITE_n_base  = 0x00,
    DESC_READ_n_base   = 0x20,
};

// Addresses for "control register" manipulation
enum
{
    DESC_ACCESS_CREG = DESC_READ_n_base + 15,
    ADDR_ACCESS_CREG = 63,
};

enum
{
    STATUS_EnExSt = 1 << 4,
    STATUS_Rack   = 1 << 5,
    STATUS_ExSt0  = 1 << 6,
    STATUS_ExSt1  = 1 << 7,
};

enum
{
    ADDR_ST1 = 1 << 6,  // Enable external START
    ADDR_ST2 = 1 << 7,  // Perform prog. START immediately
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

static inline int unitcode2unit(int unitcode)
{
    return (unitcode / 16) * UNITS_PER_LINE + (unitcode & 15);
}

static inline int IPPvalue2int(int iv)
{
  int  ret = (iv & 16382) >> 1;

    if (iv & (1 << 14))
        ret |= (~(int)8191);

    return ret;
}

//////////////////////////////////////////////////////////////////////

enum
{
    READ_STATE_NONE = 0,
    READ_STATE_PROG = 1,
    READ_STATE_WAIT = 2,
    READ_STATE_READ = 3,
};

enum
{
    BIAS_STATE_ABSENT    = 0,
    BIAS_STATE_PRESENT   = 1,
    BIAS_STATE_MEASURING = 2,
    BIAS_STATE_DROPPING  = 3,
    BIAS_STATE_STARTING  = 4,
};

enum
{
    UNIT_KIND_DEFAULT = 0,
    UNIT_KIND_REPKOV  = 1,
    UNIT_KIND_CHEREP  = 2,
};

typedef struct
{
    cankoz_vmt_t    *lvmt;
    int              devid;
    int              handle;

    int              hw_ver;
    int              sw_ver;

    sl_tid_t         tid;
    int              measuring_now;
    struct timeval   measurement_start;

    int              read_state; // One of READ_STATE_nnnn
    sl_tid_t         read_tid;

    int32            def_kind;
    int32            def_nwires;
    int32            unit_online[CANIPP_MAXUNITS];
    int32            unit_kind  [CANIPP_MAXUNITS];
    int32            unit_nwires[CANIPP_MAXUNITS]; // Number of wires to read (1-32)
    int32            unit_M     [CANIPP_MAXUNITS]; // int*32* to be suitable
    int32            unit_P     [CANIPP_MAXUNITS]; // for Return*()
    int32            unit_CREG  [CANIPP_MAXUNITS];
    rflags_t         unit_rflags[CANIPP_MAXUNITS];

    int32            unit_d_rcvd[CANIPP_MAXUNITS]; // all Data was received
    int32            unit_data  [CANIPP_MAXUNITS]/*just-alignment*/[CANIPP_CELLS_PER_UNIT];

    int32            unit_bias  [CANIPP_MAXUNITS][CANIPP_NUMRANGES][CANIPP_CELLS_PER_UNIT];

    int32            bias_count;

    int              bias_state;
    int              bias_mes_count;
    int              bias_mes_r;
    int              bias_mes_n;
} privrec_t;

static psp_lkp_t kind_lkp[] =
{
    {"r",          UNIT_KIND_REPKOV},
    {"repkov",     UNIT_KIND_REPKOV},
    {"c",          UNIT_KIND_CHEREP},
    {"cherepanov", UNIT_KIND_CHEREP},
    {NULL, 0},
};

static psp_paramdescr_t canipp_params[] =
{
    PSP_P_LOOKUP("def_kind",   privrec_t, def_kind,   UNIT_KIND_DEFAULT, kind_lkp),
    PSP_P_INT   ("def_nwires", privrec_t, def_nwires, 0, 1, CANIPP_CELLS_PER_UNIT),

#define ONE_KIND(n) \
    PSP_P_LOOKUP(__CX_STRINGIZE(n), privrec_t, unit_kind[n], UNIT_KIND_DEFAULT, kind_lkp), \
    PSP_P_INT   ("nwires"__CX_STRINGIZE(n), privrec_t, unit_nwires[n], 0, 1, CANIPP_CELLS_PER_UNIT)

    ONE_KIND(0),  ONE_KIND(1),  ONE_KIND(2),  ONE_KIND(3),  ONE_KIND(4),
    ONE_KIND(5),  ONE_KIND(6),  ONE_KIND(7),  ONE_KIND(8),  ONE_KIND(9),
    ONE_KIND(10), ONE_KIND(11), ONE_KIND(12), ONE_KIND(13), ONE_KIND(14),
    ONE_KIND(15), ONE_KIND(16), ONE_KIND(17), ONE_KIND(18), ONE_KIND(19),
    ONE_KIND(20), ONE_KIND(21), ONE_KIND(22), ONE_KIND(23), ONE_KIND(24),
    ONE_KIND(25), ONE_KIND(26), ONE_KIND(27), ONE_KIND(28), ONE_KIND(29),
    PSP_P_END()
};

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
      CANIPP_CHAN_BIAS_R0_n_base + range_n * CANIPP_MAXUNITS + unit;

  void *val_p = &(me->unit_bias[unit][range_n]);

  static cxdtype_t  dt_int32    = CXDTYPE_INT32;
  static int        nels        = CANIPP_CELLS_PER_UNIT;
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

    for (unit = 0;  unit < CANIPP_MAXUNITS; unit++)
        if (me->unit_online[unit])
            for (range_n = 0;  range_n < CANIPP_NUMRANGES;  range_n++)
                ReturnOneBias(me, unit, range_n);
}

static void DropReadTout(privrec_t *me)
{
    if (me->read_tid >= 0)
    {
        sl_deq_tout(me->read_tid);
        me->read_tid = -1;
    }
}

//////////////////////////////////////////////////////////////////////

static void SwitchToABSENT   (privrec_t *me)
{
    me->bias_state = BIAS_STATE_ABSENT;
    bzero(me->unit_bias, sizeof(me->unit_bias));
    ReturnBiases(me);
    /*!!! Drop other params? */
    ReturnInt32Datum(me->devid, CANIPP_CHAN_BIAS_STEPS_LEFT, 0, 0);
    ReturnInt32Datum(me->devid, CANIPP_CHAN_BIAS_IS_ON,      0, 0);
}

static void SwitchToPRESENT  (privrec_t *me)
{
  int     unit;
  int     range_n;
  int     wire;
  int     count = me->bias_mes_count;

    if (count == 0) count = 1;

    for (unit = 0;  unit < CANIPP_MAXUNITS; unit++)
        if (me->unit_online[unit])
            for (range_n = 0;  range_n < CANIPP_NUMRANGES;  range_n++)
                for (wire = 0;  wire < CANIPP_CELLS_PER_UNIT; wire++)
                    me->unit_bias[unit][range_n][wire] /= count;

    me->bias_state = BIAS_STATE_PRESENT;
    ReturnBiases(me);
    ReturnInt32Datum(me->devid, CANIPP_CHAN_BIAS_STEPS_LEFT, 0, 0);
    ReturnInt32Datum(me->devid, CANIPP_CHAN_BIAS_CUR_COUNT,  me->bias_mes_count, 0);
    ReturnInt32Datum(me->devid, CANIPP_CHAN_BIAS_IS_ON,      1, 0);
}

static void SwitchToMEASURING(privrec_t *me)
{
    me->bias_state = BIAS_STATE_MEASURING;
    me->bias_mes_count = me->bias_count;
    me->bias_mes_r = CANIPP_NUMRANGES   - 1;
    me->bias_mes_n = me->bias_mes_count - 1;
    bzero(me->unit_bias, sizeof(me->unit_bias));
    ReturnInt32Datum(me->devid, CANIPP_CHAN_BIAS_STEPS_LEFT, 
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
        return; // to skip returning of CANIPP_CHAN_BIAS_STEPS_LEFT
    }

    ReturnInt32Datum(me->devid, CANIPP_CHAN_BIAS_STEPS_LEFT, 
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
    ReturnInt32Datum(me->devid, CANIPP_CHAN_BIAS_STEPS_LEFT, 
                     CANIPP_NUMRANGES * me->bias_count,
                     0);
}

static int  StartMeasurements(privrec_t *me)
{
  int       unit;
  int       cell;
  int       the_m;
  int       the_p;
  uint8     creg_code;
  int       addr;

    me->read_state = READ_STATE_PROG; // In fact, a dummy action -- is overwritten with _WAIT at end of function

    for (unit = 0;  unit < CANIPP_MAXUNITS;  unit++)
    {
        me->unit_rflags[unit] = 0;
        me->unit_d_rcvd[unit] = 0;
        for (cell = 0;  cell < me->unit_nwires[unit];  cell++)
            me->unit_data[unit][cell] = CANIPP_DATUM_ABSENT;

        if (me->unit_online[unit])
        {
            if (me->unit_kind[unit] == UNIT_KIND_REPKOV)
            {
                the_m = me->bias_state == BIAS_STATE_MEASURING? me->bias_mes_r
                                                              : me->unit_M[unit];
                the_p =                                         me->unit_P[unit];
                creg_code = (the_m << SETTING_M_SHIFT) | (the_p << SETTING_P_SHIFT);
            }
            else
                creg_code = me->unit_CREG[unit];

            me->lvmt->q_enq_ons_v(me->handle, SQ_ALWAYS,
                                  3,
                                  DESC_WRITE_n_base + unitn2unitcode(unit),
                                  CANIPP_CADDR,
                                  creg_code);
        }
    }

    me->lvmt->q_enq_ons_v(me->handle, SQ_ALWAYS, 
                          2,
                          DESC_ACCESS_CREG,
                          ADDR_ACCESS_CREG | ADDR_ST1);

    me->read_state = READ_STATE_WAIT;
    ReturnInt32Datum(me->devid, CANIPP_CHAN_READ_STATE, me->read_state, 0);

    return 0;
}

static int  AbortMeasurements(privrec_t *me)
{
    /* Send a "stop!" packet */
    // a. A direct-send, to be performed as soon as possible
    me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS, 
                          SQ_TRIES_DIR,
                          0,
                          NULL, 0,
                          0, 2,
                          DESC_ACCESS_CREG,
                          ADDR_ACCESS_CREG/*Note: '0' in ST1 and ST2 bits*/);
    // b. A backup way - via queue, to ensure "drop ST1/ST2" delivery even if direct-send fails
    me->lvmt->q_enq_ons_v(me->handle, SQ_ALWAYS, 
                          2,
                          DESC_ACCESS_CREG,
                          ADDR_ACCESS_CREG/*Note: '0' in ST1 and ST2 bits*/);

    me->read_state = READ_STATE_NONE;
    ReturnInt32Datum(me->devid, CANIPP_CHAN_READ_STATE, me->read_state, 0);
    DropReadTout(me);

    if (me->bias_state != BIAS_STATE_ABSENT  &&
        me->bias_state != BIAS_STATE_PRESENT)
        SwitchToABSENT(me);

    return 1;
}

static void ReadMeasurements (privrec_t *me)
{
    me->read_state = READ_STATE_NONE;
    ReturnInt32Datum(me->devid, CANIPP_CHAN_READ_STATE, me->read_state, 0);
    DropReadTout(me);
}

static void PerformReturn(privrec_t *me, rflags_t rflags)
{
  int       unit;
  int       cell;
  rflags_t  unit_rflags;

#if __GNUC__ < 3
  int              r_addrs [3];
  cxdtype_t        r_dtypes[3];
  int              r_nelems[3];
  void            *r_values[3];
  rflags_t         r_rflags[3];
#endif

    for (unit = 0;  unit < CANIPP_MAXUNITS;  unit++)
        if (me->unit_online[unit])
        {
            if      (me->bias_state == BIAS_STATE_ABSENT  ||
                     me->bias_state == BIAS_STATE_PRESENT)
            {
                unit_rflags = me->unit_rflags[unit] | rflags;
                if (me->bias_state == BIAS_STATE_PRESENT  &&
                    me->unit_kind[unit] == UNIT_KIND_REPKOV)
                {
                    for (cell = 0;  cell < me->unit_nwires[unit];  cell++)
                        me->unit_data[unit][cell] -= me->unit_bias[unit][me->unit_M[unit]][cell];
                }
                if (me->unit_kind[unit] == UNIT_KIND_REPKOV)
                    SetChanRDs   (me->devid, CANIPP_CHAN_DATA_n_base + unit, 1,
                                  r2q[me->unit_M[unit]], 0.0);
#if __GNUC__ < 3
                r_addrs [0] = CANIPP_CHAN_CUR_M_n_base + unit; r_addrs [1] = CANIPP_CHAN_CUR_P_n_base + unit; r_addrs [2] = CANIPP_CHAN_CUR_CREG_n_base + unit; r_addrs [3] = CANIPP_CHAN_DATA_n_base + unit;
                r_dtypes[0] = CXDTYPE_INT32;                   r_dtypes[1] = CXDTYPE_INT32;                   r_dtypes[2] = CXDTYPE_INT32;                      r_dtypes[3] = CXDTYPE_INT32;
                r_nelems[0] = 1;                               r_nelems[1] = 1;                               r_nelems[2] = 1;                                  r_nelems[3] = me->unit_nwires[unit];
                r_values[0] = me->unit_M               + unit; r_values[1] = me->unit_P               + unit; r_values[2] = me->unit_CREG               + unit; r_values[3] = me->unit_data           + unit;
                r_rflags[0] =                                  r_rflags[1] =                                  r_rflags[2] =                                     r_rflags[3] = unit_rflags;
                ReturnDataSet(me->devid,
                              3,
                              r_addrs,
                              r_dtypes,
                              r_nelems,
                              r_values,
                              r_rflags,
                              NULL);
#else
                ReturnDataSet(me->devid,
                              4,
                              (int      []){CANIPP_CHAN_CUR_M_n_base + unit, CANIPP_CHAN_CUR_P_n_base + unit, CANIPP_CHAN_CUR_CREG_n_base + unit, CANIPP_CHAN_DATA_n_base + unit},
                              (cxdtype_t[]){CXDTYPE_INT32,                   CXDTYPE_INT32,                   CXDTYPE_INT32,                      CXDTYPE_INT32},
                              (int      []){1,                               1,                               1,                                  me->unit_nwires[unit]},
                              (void*    []){me->unit_M               + unit, me->unit_P               + unit, me->unit_CREG               + unit, me->unit_data           + unit},
                              (rflags_t []){unit_rflags,                     unit_rflags,                     unit_rflags,                        unit_rflags},
                              NULL);
#endif
            }
            else if (me->bias_state == BIAS_STATE_MEASURING  &&
                     me->unit_kind[unit] == UNIT_KIND_REPKOV)
            {
                for (cell = 0;  cell < me->unit_nwires[unit];  cell++)
                    me->unit_bias[unit][me->bias_mes_r][cell] += me->unit_data[unit][cell];
            }
        }

    if      (me->bias_state == BIAS_STATE_MEASURING)
        SwitchToNextBiasMeasurementStep(me);
    else if (me->bias_state == BIAS_STATE_DROPPING)
        SwitchToABSENT   (me);
    else if (me->bias_state == BIAS_STATE_STARTING)
        SwitchToMEASURING(me);

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
    if (me->measuring_now) return;

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

static void RequestScan(privrec_t *me)
{
  int  unit;

    for (unit = 0;  unit < CANIPP_MAXUNITS;  unit++)
    {
        me->unit_online[unit] = 0;
        me->lvmt->q_enq_v(me->handle, SQ_ALWAYS, 2,
                          DESC_READ_n_base + unitn2unitcode(unit), CANIPP_CADDR);
    }
}

//////////////////////////////////////////////////////////////////////
static void canipp_start_hbt(int devid, void *devptr,
                           sl_tid_t tid, void *privptr)
{
  privrec_t *me = devptr;

    sl_enq_tout_after(devid, devptr, 10*1000000, canipp_start_hbt, NULL);
    if (me->read_state == READ_STATE_WAIT)
        me->lvmt->q_enq_ons_v(me->handle, SQ_ALWAYS, 
                              2,
                              DESC_ACCESS_CREG,
                              ADDR_ACCESS_CREG | ADDR_ST1);
}
//////////////////////////////////////////////////////////////////////


static void canipp_ff(int devid, void *devptr, int is_a_reset);
static void canipp_in(int devid, void *devptr,
                      int desc,  size_t dlc, uint8 *data);

static int canipp_init_d(int devid, void *devptr, 
                         int businfocount, int businfo[],
                         const char *auxinfo)
{
  privrec_t *me    = (privrec_t *) devptr;

  int     unit;
  int     range_n;

    DoDriverLog(devid, 0 | DRIVERLOG_C_ENTRYPOINT, "ENTRY %s(%s)", __FUNCTION__, auxinfo);

    if (me->def_kind   == UNIT_KIND_DEFAULT) me->def_kind   = UNIT_KIND_REPKOV;
    if (me->def_nwires <= 0)                 me->def_nwires = CANIPP_CELLS_PER_UNIT;
    for (unit = 0;  unit < CANIPP_MAXUNITS;  unit++)
    {
        if (me->unit_kind  [unit] == UNIT_KIND_DEFAULT)
            me->unit_kind  [unit]  = me->def_kind;
        if (me->unit_nwires[unit] <= 0)
            me->unit_nwires[unit]  = me->def_nwires;
            
    }
////fprintf(stderr, "#%d: unit_kind[0]=%d\n", devid, me->unit_kind[0]);

    /* Initialize interface */
    me->lvmt   = GetLayerVMT(devid);
    me->devid  = devid;
    me->handle = me->lvmt->add(devid, devptr,
                               businfocount, businfo,
                               DEVTYPE,
                               canipp_ff, canipp_in,
                               CANIPP_NUMCHANS * 2/*!!!*/
                               +CANIPP_MAXUNITS*CANIPP_CELLS_PER_UNIT,
                               CANKOZ_LYR_OPTION_NONE);
    if (me->handle < 0) return me->handle;

    me->tid      = -1;
    me->read_tid = -1;

    SetChanReturnType(devid, CANIPP_CHAN_HW_VER, 1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, CANIPP_CHAN_SW_VER, 1, IS_AUTOUPDATED_TRUSTED);

    SetChanReturnType(devid, CANIPP_CHAN_ONLINE_n_base, CANIPP_MAXUNITS, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, CANIPP_CHAN_DATA_n_base,   CANIPP_MAXUNITS, IS_AUTOUPDATED_YES);

    for (unit = 0; unit < CANIPP_MAXUNITS;  unit++)
        if (me->unit_kind[unit] == UNIT_KIND_REPKOV)
            for (range_n = 0;  range_n < CANIPP_NUMRANGES;  range_n++)
                SetChanRDs(me->devid,
                           CANIPP_CHAN_BIAS_R0_n_base + range_n * CANIPP_MAXUNITS + unit,
                           1,
                           r2q[range_n], 0.0);

    SetChanReturnType(devid, CANIPP_CHAN_BIAS_R0_n_base, CANIPP_MAXUNITS*CANIPP_NUMRANGES, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, CANIPP_CHAN_BIAS_IS_ON,      1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, CANIPP_CHAN_BIAS_STEPS_LEFT, 1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, CANIPP_CHAN_BIAS_CUR_COUNT,  1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, CANIPP_CHAN_READ_STATE,      1, IS_AUTOUPDATED_TRUSTED);
    SetChanReturnType(devid, CANIPP_CHAN_CONST32,         1, IS_AUTOUPDATED_TRUSTED);
    ReturnInt32Datum (devid, CANIPP_CHAN_BIAS_IS_ON,      0, 0);
    ReturnInt32Datum (devid, CANIPP_CHAN_BIAS_STEPS_LEFT, 0, 0);
    ReturnInt32Datum (devid, CANIPP_CHAN_BIAS_CUR_COUNT,  me->bias_count = 5, 0);
    ReturnInt32Datum (devid, CANIPP_CHAN_READ_STATE,      me->read_state,     0);
    ReturnInt32Datum (devid, CANIPP_CHAN_CONST32,         CANIPP_CELLS_PER_UNIT, 0);
    SwitchToABSENT(me);

    canipp_start_hbt(devid, devptr, -1, NULL);

    return DEVSTATE_OPERATING;
}


static void canipp_ff(int devid, void *devptr, int is_a_reset)
{
  privrec_t *me = devptr;

    me->lvmt->get_dev_ver(me->handle, &(me->hw_ver), &(me->sw_ver), NULL);
    ReturnInt32Datum(me->devid, CANIPP_CHAN_HW_VER, me->hw_ver, 0);
    ReturnInt32Datum(me->devid, CANIPP_CHAN_SW_VER, me->sw_ver, 0);

    /* Note: unfortunately, data will remain "OFFLINE", because _rst() is called BEFORE triggering  */
    if (is_a_reset)
    {
        PerformTimeoutActions(me, 1);
    }

    RequestScan(me);
}

static void read_timeout(int devid, void *devptr,
                         sl_tid_t tid,
                         void *privptr)
{
  privrec_t  *me     = (privrec_t *) devptr;

    me->read_tid = -1;

    PerformTimeoutActions(me, 1);
}
static void canipp_in(int devid, void *devptr,
                      int desc,  size_t dlc, uint8 *data)
{
  privrec_t  *me     = (privrec_t *) devptr;

  int         unit;
  int         addr;
  uint32      code;
  rflags_t    rflags;
  int         wire;

    switch (desc)
    {
        case DESC_READ_n_base        ... DESC_READ_n_base + 0x0e: // NO 0x0F -- "broadcast read" is impossible"
        case DESC_READ_n_base + 0x10 ... DESC_READ_n_base + 0x1e: // NO 0x1F -- "broadcast read" is impossible"
            if (dlc < 5) return;
            me->lvmt->q_erase_and_send_next_v(me->handle, 2, desc, data[1]);

            unit = unitcode2unit(desc - DESC_READ_n_base);
            addr = data[1];
            code = data[2] | (data[3] << 8);
            if     (addr == CANIPP_CADDR)
            {
                /* Process read values only if "idle" (this means scan) */
                if (me->read_state != READ_STATE_NONE) return;

                if ((data[4] & STATUS_Rack) == 0)
                {
                    me->unit_online[unit] = 0;
                    me->unit_M     [unit] = 0;
                    me->unit_P     [unit] = 0;
                    me->unit_CREG  [unit] = 0;
                }
                else
                {
                    me->unit_online[unit] = 1;
                    me->unit_M     [unit] = ((code & SETTING_M_MASK) >> SETTING_M_SHIFT);
                    me->unit_P     [unit] = ((code & SETTING_P_MASK) >> SETTING_P_SHIFT);
                    me->unit_CREG  [unit] = code;
                }
                rflags = (me->unit_online [unit]? 0 : CXRF_CAMAC_NO_Q);
DoDriverLog(devid, 0, "unit=%-02d %c:%-2d M=%d P=%d CREG=%d", unit, me->unit_kind==UNIT_KIND_REPKOV?'r':'c', me->unit_nwires[unit], me->unit_M[unit], me->unit_P[unit], me->unit_CREG[unit]);

                ReturnInt32Datum(devid,
                                 CANIPP_CHAN_M_n_base      + unit,
                                 me->unit_M                 [unit], rflags);
                ReturnInt32Datum(devid,
                                 CANIPP_CHAN_P_n_base      + unit,
                                 me->unit_P                 [unit], rflags);
                ReturnInt32Datum(devid,
                                 CANIPP_CHAN_CREG_n_base   + unit,
                                 me->unit_CREG              [unit], rflags);
                /* Note: "online" flag is returned *after* M and P, for their values to be ready when "box becomes ready" */
                ReturnInt32Datum(devid,
                                 CANIPP_CHAN_ONLINE_n_base + unit,
                                 me->unit_online            [unit], 0);

                if (unit == CANIPP_MAXUNITS-1) // The last one? OK, scan finished
                    RequestMeasurements(me);
            }
            else if (addr >= 0  &&  addr < me->unit_nwires[unit])
            {
                if (me->measuring_now == 0) return;
                if (me->unit_d_rcvd[unit])  return;
                /*!!! should also check if "state==STATE_READING" */

                /* Store datum */
#if 1
                wire = addr;
#else
                wire = (addr + CANIPP_CELLS_PER_UNIT+1) % CANIPP_CELLS_PER_UNIT;
#endif
                me->unit_data[unit][wire] = IPPvalue2int(code);

                /* Is ALL box data received? */
                for (wire = 0;  wire < me->unit_nwires[unit];  wire++)
                    if (me->unit_data[unit][wire] == CANIPP_DATUM_ABSENT) return;
                me->unit_d_rcvd[unit] = 1;

                /* Finally, if ALL the boxes are processed, go ahead */
                for (unit = 0;  unit < CANIPP_MAXUNITS;  unit++)
                    if (me->unit_online[unit]  &&
                        me->unit_d_rcvd[unit] == 0) return;

                me->read_state = READ_STATE_NONE;
                ReturnInt32Datum(me->devid, CANIPP_CHAN_READ_STATE, me->read_state, 0);
                drdy_p(me, 1, 0);
            }
            else
            {
                /* Log out-of-bounds address somehow? */
            }
            break;

        case CANKOZ_DESC_GETDEVSTAT:
            if (dlc < 2) return;
            /* Ignore if not waiting for data (in fact, should almost never be true -- except during initial scan) */
            if (me->measuring_now == 0) return;
            /* Ignore if is already reading (just a safety check) */
            if (me->read_state != READ_STATE_WAIT)              return;
            /* Check if ANY of START pulses had happened */
            if ((data[1] & (STATUS_ExSt0 | STATUS_ExSt1)) == 0) return;

            /* A "disable starts" (in fact, a dummy command)
               and "perform 10ms pause after sending" packet */
            me->lvmt->q_enqueue_v(me->handle, SQ_ALWAYS, 
                                  SQ_TRIES_ONS,
                                  10000 /* 10ms pause */,
                                  NULL, 0,
                                  0, 2,
                                  DESC_ACCESS_CREG,
                                  ADDR_ACCESS_CREG/*Note: '0' in ST1 and ST2 bits*/);
            /* Request reading of all active boxen */
            for (unit = 0; unit < CANIPP_MAXUNITS;  unit++)
                if (me->unit_online[unit])
                    for (addr = 0;  addr < me->unit_nwires[unit]; addr++)
                        me->lvmt->q_enq_v(me->handle, SQ_ALWAYS,
                                          2, 
                                          DESC_READ_n_base + unitn2unitcode(unit),
                                          addr);
            me->read_state = READ_STATE_READ;
            ReturnInt32Datum(me->devid, CANIPP_CHAN_READ_STATE, me->read_state, 0);
            DropReadTout(me);
            me->read_tid   = sl_enq_tout_after(devid, me, 10*1000000, read_timeout, NULL);
            break;
    }
}

static void canipp_rw_p(int devid, void *devptr,
                        int action,
                        int count, int *addrs,
                        cxdtype_t *dtypes, int *nelems, void **values)
{
  privrec_t *me = (privrec_t *)devptr;

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
            case CANIPP_CHAN_ONLINE_n_base ... CANIPP_CHAN_ONLINE_n_base+CANIPP_MAXUNITS-1:
                unit = chn - CANIPP_CHAN_ONLINE_n_base;
                ReturnInt32Datum(devid, chn, me->unit_online[unit], 0);
                break;

            case CANIPP_CHAN_M_n_base ... CANIPP_CHAN_M_n_base+CANIPP_MAXUNITS-1:
                unit = chn - CANIPP_CHAN_M_n_base;
                if (action == DRVA_WRITE) SetM(me, unit, value);
                rflags = (me->unit_online [unit]? 0 : CXRF_CAMAC_NO_Q);
                ReturnInt32Datum(devid, chn, me->unit_M[unit], rflags);
                break;
                
            case CANIPP_CHAN_P_n_base ... CANIPP_CHAN_P_n_base+CANIPP_MAXUNITS-1:
                unit = chn - CANIPP_CHAN_P_n_base;
                if (action == DRVA_WRITE) SetP(me, unit, value);
                rflags = (me->unit_online [unit]? 0 : CXRF_CAMAC_NO_Q);
                ReturnInt32Datum(devid, chn, me->unit_P[unit], rflags);
                break;

            case CANIPP_CHAN_CREG_n_base ... CANIPP_CHAN_CREG_n_base+CANIPP_MAXUNITS-1:
                unit = chn - CANIPP_CHAN_CREG_n_base;
                if (action == DRVA_WRITE) me->unit_CREG[unit] = value & 0xFFFF;
                rflags = (me->unit_online [unit]? 0 : CXRF_CAMAC_NO_Q);
                ReturnInt32Datum(devid, chn, me->unit_CREG[unit], rflags);
                break;

            case CANIPP_CHAN_BIAS_GET:
                if (action == DRVA_WRITE  &&  value == CX_VALUE_COMMAND)
                    SwitchToSTARTING(me);
                ReturnInt32Datum(devid, chn, 0, 0);
                break;

            case CANIPP_CHAN_BIAS_DROP:
                if (action == DRVA_WRITE  &&  value == CX_VALUE_COMMAND)
                    SwitchToDROPPING(me);
                ReturnInt32Datum(devid, chn, 0, 0);
                break;

            case CANIPP_CHAN_BIAS_COUNT:
                if (action == DRVA_WRITE)
                {
                    if (value < 1)    value = 1;
                    if (value > 1000) value = 1000;
                    me->bias_count = value;
                }
                ReturnInt32Datum (devid, chn,  me->bias_count, 0);
                break;

            case CANIPP_CHAN_HW_VER:
            case CANIPP_CHAN_SW_VER:
                /* Do-nothing -- returned from _ff() */
                break;

            case CANIPP_CHAN_READ_STATE:
                /* Do-nothing -- returned upon change */
                break;

            default:
                ;/*??? ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED); */
                /*!!! NO!!!  Because no data channels */
        }

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(canipp, "CANIPP-M driver",
                   NULL, NULL,
                   sizeof(privrec_t), canipp_params,
                   2, 2,
                   CANKOZ_LYR_NAME, CANKOZ_LYR_VERSION,
                   NULL,
                   -1, NULL, NULL,
                   canipp_init_d, NULL, canipp_rw_p);
