#include "cxsd_driver.h"

#include "drv_i/frolov_ie4_drv_i.h"


enum
{
    // F=0,16
    A_A     = 0,
    A_K     = 4,

    // F=1,17
    A_REBUM = 0,
    A_RF    = 1,
    A_RM    = 2,
    A_RS    = 3,
    A_IEBUM = 4,
};


#define MIN_F_IN_NS 0
#define MAX_F_IN_NS 2e9 // 2e9ns==2s

typedef struct
{
    int      devid;
    int      N;
    float64  F_IN_NS;
} frolov_ie4_privrec_t;

static psp_paramdescr_t frolov_ie4_params[] =
{
    PSP_P_REAL("f_in", frolov_ie4_privrec_t, F_IN_NS, 0.0, 0.0, 0.0),
    PSP_P_END()
};


static void frolov_ie4_rw_p(int devid, void *devptr,
                            int action,
                            int count, int *addrs,
                            cxdtype_t *dtypes, int *nelems, void **values);

static void ReturnOne(frolov_ie4_privrec_t *me, int nc)
{
    frolov_ie4_rw_p(me->devid, me,
                    DRVA_READ,
                    1, &nc,
                    NULL, NULL, NULL);
}

static void ReturnVs(frolov_ie4_privrec_t *me)
{
  static int addrs[FROLOV_IE4_NUMOUTPUTS] = {
      FROLOV_IE4_CHAN_V_n_base + 0,
      FROLOV_IE4_CHAN_V_n_base + 1,
      FROLOV_IE4_CHAN_V_n_base + 2,
      FROLOV_IE4_CHAN_V_n_base + 3,
  };

    frolov_ie4_rw_p(me->devid, me,
                    DRVA_READ,
                    FROLOV_IE4_NUMOUTPUTS, addrs,
                    NULL, NULL, NULL);
}

static void LAM_CB(int devid, void *devptr)
{
  frolov_ie4_privrec_t *me = (frolov_ie4_privrec_t*)devptr;
  int                   c;

    // Drop LAM
    DO_NAF(CAMAC_REF, me->N, 0, 10, &c);
    // Immediately update BUM status
    ReturnOne(me, FROLOV_IE4_CHAN_BUM_GOING);
    // And signal LAM upwards
    ReturnInt32Datum(devid, FROLOV_IE4_CHAN_LAM_SIG, 0, 0);
}

static int frolov_ie4_init_d(int devid, void *devptr,
                             int businfocount, int *businfo,
                             const char *auxinfo)
{
  frolov_ie4_privrec_t *me = (frolov_ie4_privrec_t*)devptr;
  const char           *errstr;
  int                   c;
  
    me->devid   = devid;
    me->N       = businfo[0];

    /* Handle f_in specification, if set */
    // a. Negative -- nanoseconds specified, just change sign
    if      (me->F_IN_NS < 0.0) me->F_IN_NS = -me->F_IN_NS;
    // b. Positive -- in Hz, convert to nanoseconds
    else if (me->F_IN_NS > 0.0) me->F_IN_NS = 1e9 / me->F_IN_NS;
    // Now, impose limits
    if (me->F_IN_NS < MIN_F_IN_NS) me->F_IN_NS = MIN_F_IN_NS;
    if (me->F_IN_NS > MAX_F_IN_NS) me->F_IN_NS = MAX_F_IN_NS;
                                
    SetChanReturnType(devid, FROLOV_IE4_CHAN_LAM_SIG, 1, IS_AUTOUPDATED_YES);

    if ((errstr = WATCH_FOR_LAM(devid, devptr, me->N, LAM_CB)) != NULL)
    {
        DoDriverLog(devid, DRIVERLOG_ERRNO, "WATCH_FOR_LAM(): %s", errstr);
        return -CXRF_DRV_PROBL;
    }

    // Drop LAM
    DO_NAF(CAMAC_REF, me->N, 0, 10, &c);
    // Unmask LAM
    DO_NAF(CAMAC_REF, me->N, A_RS, 1,  &c);
    c = (c &~ (1 << 5));
    DO_NAF(CAMAC_REF, me->N, A_RS, 17, &c);

    return DEVSTATE_OPERATING;
}

static void frolov_ie4_rw_p(int devid, void *devptr,
                            int action,
                            int count, int *addrs,
                            cxdtype_t *dtypes, int *nelems, void **values)
{
  frolov_ie4_privrec_t *me = (frolov_ie4_privrec_t*)devptr;

  int                   n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int                   chn;   // channel
  int                   l;     // Line #
  
  int                   c;
  int32                 value;
  rflags_t              rflags;

  int                   c_rf;
  int                   c_rs;
  int                   v_kclk;
  int                   c_Ai;
  double                v_fclk;
  double                quant;

  double                dval;
  void                 *vp;
  static cxdtype_t      dtype_f64 = CXDTYPE_DOUBLE;
  static int            n_1       = 1;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
        if (action == DRVA_WRITE  &&
            (chn >= FROLOV_IE4_DOUBLE_CHAN_base  &&
             chn <= FROLOV_IE4_DOUBLE_CHAN_base + FROLOV_IE4_DOUBLE_CHAN_count - 1))
        {
            if (nelems[n] != 1  ||  dtypes[n] != CXDTYPE_DOUBLE)
                goto NEXT_CHANNEL;
            dval = *((float64*)(values[n]));
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

        switch (chn)
        {
            case FROLOV_IE4_CHAN_A_n_base ...
                 FROLOV_IE4_CHAN_A_n_base + FROLOV_IE4_NUMOUTPUTS - 1:
                l = chn - FROLOV_IE4_CHAN_A_n_base;
                if (action == DRVA_WRITE)
                {
                    if (value < 0)      value = 0;
                    if (value > 0x0FFF) value = 0x0FFF;
                    c = value;
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_A+l, 16, &c));
                    ReturnOne(me, FROLOV_IE4_CHAN_V_n_base + l);
                }
                else
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_A+l, 0, &c));
                    value = c & 0x0FFF;
                }
                break;

            case FROLOV_IE4_CHAN_K_n_base + 1 ... /* No K for OUT1 */
                 FROLOV_IE4_CHAN_K_n_base + FROLOV_IE4_NUMOUTPUTS - 1:
                l = chn - FROLOV_IE4_CHAN_K_n_base;
                if (action == DRVA_WRITE)
                {
                    if (value < 0)      value = 0;
                    if (value > 0x001F) value = 0x001F;
                    c = value;
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_K+l-1, 16, &c));
                    ReturnVs(me);
                }
                else
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_K+l-1, 0, &c));
                    value = c & 0x001F;
                }
                break;

            case FROLOV_IE4_CHAN_V_n_base ...
                 FROLOV_IE4_CHAN_V_n_base + FROLOV_IE4_NUMOUTPUTS - 1:
                l = chn - FROLOV_IE4_CHAN_V_n_base;

                DO_NAF(CAMAC_REF, me->N, A_RF, 1, &c_rf);
                DO_NAF(CAMAC_REF, me->N, A_RS, 1, &c_rs);
                v_kclk = 1 << (c_rf & 3); // {0,1,2,3}=>{1,2,4,8}
                v_fclk = ((c_rs & 3) == 0)? me->F_IN_NS : 40; // 0(Fin)=>???, else=>40ns
                quant = v_fclk * v_kclk;

                rflags = 0;
                if (action == DRVA_WRITE  &&  quant != 0.0)
                {
                    if (dval < 0) dval = 0;

                    c_Ai = dval / quant;         if (c_Ai > 4095) c_Ai = 4095;
                    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_A+l, 16, &c_Ai));

                    ReturnOne(me, FROLOV_IE4_CHAN_A_n_base + l);
                }
                // Perform "read" anyway
                rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_A+l, 0, &c_Ai));
                if (quant != 0.0)
                {
                    dval = c_Ai * quant;
                }
                else
                {
                    rflags |= CXRF_OVERLOAD;
                    dval    = -999999;
                }
                vp     = &dval;
                rflags = 0;
                ReturnDataSet(devid, 1,
                              &chn, &dtype_f64, &n_1, &vp, &rflags, NULL);
                goto NEXT_CHANNEL;

            case FROLOV_IE4_CHAN_OFF_n_base ...
                 FROLOV_IE4_CHAN_OFF_n_base + FROLOV_IE4_NUMOUTPUTS - 1:
            case FROLOV_IE4_CHAN_ALLOFF: // "ALL" immediately follows individual ones in both channels and bits
                l = chn - FROLOV_IE4_CHAN_OFF_n_base;
                if (action == DRVA_WRITE)
                {
                    value = (value != 0);
                    rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, 2, 1,  &c));
                    c = (c &~ (1 << l)) | (value << l);
                    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, 2, 17, &c));
                }
                else
                {
                    rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, 2, 1, &c));
                    value   = ((c & (1 << l)) != 0);
                }
                break;

            case FROLOV_IE4_CHAN_KCLK_N:
                if (action == DRVA_WRITE)
                {
                    if (value < 0) value = 0;
                    if (value > 3) value = 3;
                    c = value;
                    rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RF, 17, &c));
                    ReturnVs(me);
                }
                else
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RF, 1, &c));
                    value = c;
                }
                break;

            case FROLOV_IE4_CHAN_FCLK_SEL:
                if (action == DRVA_WRITE)
                {
                    if (value < 0) value = 0;
                    if (value > 3) value = 3;
                    rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RS, 1,  &c));
                    c = (c &~ (3 << 0)) | (value << 0);
                    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_RS, 17, &c));
                    ReturnVs(me);
                }
                else
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RS, 1, &c));
                    c = (c >> 0) & 3;
                    value = c;
                }
                break;

            case FROLOV_IE4_CHAN_START_SEL:
                if (action == DRVA_WRITE)
                {
                    if (value < 0) value = 0;
                    if (value > 2) value = 2;
                    rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RS, 1,  &c));
                    c = (c &~ (3 << 2)) | (value << 2);
                    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_RS, 17, &c));
                }
                else
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RS, 1, &c));
                    c = (c >> 2) & 3;
                    if (c == 3) c = 2; // Since '11' means the same as '10'
                    value = c;
                }
                break;

            case FROLOV_IE4_CHAN_MODE:
                if (action == DRVA_WRITE)
                {
                    value = (value != 0);
                    rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RS, 1,  &c));
                    c = (c &~ (1 << 4)) | (value << 4);
                    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_RS, 17, &c));
                }
                else
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RS, 1, &c));
                    value  = ((c & (1 << 4)) != 0);
                }
                break;

            case FROLOV_IE4_CHAN_F_IN_HZ:
                if (action == DRVA_WRITE)
                {
                    // Convert HZ to ns
                    if (dval <= 0.0) dval = 0.0; else dval = 1e9 / dval;
                    // Impose regular limits on F_IN_NS
                    if (dval < MIN_F_IN_NS) dval = MIN_F_IN_NS;
                    if (dval > MAX_F_IN_NS) dval = MAX_F_IN_NS;
                    me->F_IN_NS = dval;
                    ReturnOne(me, FROLOV_IE4_CHAN_F_IN_NS);
                    ReturnVs (me);
                }
                if (me->F_IN_NS == 0.0) dval = 3.0; else dval = 1e9 / me->F_IN_NS;
                vp     = &dval;
                rflags = 0;
                ReturnDataSet(devid, 1,
                              &chn, &dtype_f64, &n_1, &vp, &rflags, NULL);
                goto NEXT_CHANNEL;

            case FROLOV_IE4_CHAN_F_IN_NS:
                if (action == DRVA_WRITE)
                {
                    if (dval < MIN_F_IN_NS) dval = MIN_F_IN_NS;
                    if (dval > MAX_F_IN_NS) dval = MAX_F_IN_NS;
                    me->F_IN_NS = dval;
                    ReturnOne(me, FROLOV_IE4_CHAN_F_IN_HZ);
                    ReturnVs (me);
                }
                vp     = &(me->F_IN_NS);
                rflags = 0;
                ReturnDataSet(devid, 1,
                              &chn, &dtype_f64, &n_1, &vp, &rflags, NULL);
                goto NEXT_CHANNEL;

            case FROLOV_IE4_CHAN_RE_BUM:
                if (action == DRVA_WRITE)
                {
                    if (value < 0)     value = 0;
                    if (value > 0xFFF) value = 0xFFF;
                    c = value;
                    rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, A_REBUM, 17, &c));
                }
                else
                {
                    rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, A_REBUM, 1,  &c));
                    value = c;
                }
                break;

            case FROLOV_IE4_CHAN_BUM_START:
                if (action == DRVA_WRITE  &&  value == CX_VALUE_COMMAND)
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, 1, 25, &c));
                    ReturnOne(me, FROLOV_IE4_CHAN_BUM_GOING);
                }
                else rflags = 0;
                value = 0;
                break;

            case FROLOV_IE4_CHAN_BUM_STOP:
                if (action == DRVA_WRITE  &&  value == CX_VALUE_COMMAND)
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, 2, 25, &c));
                    ReturnOne(me, FROLOV_IE4_CHAN_BUM_GOING);
                }
                else rflags = 0;
                value = 0;
                break;

            case FROLOV_IE4_CHAN_IE_BUM:
                rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, A_IEBUM, 1,  &c));
                value = c;
                break;

            case FROLOV_IE4_CHAN_BUM_GOING:
                rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RS, 1,  &c));
                value = (c & (1 << 6)) == 0;
                break;

            case FROLOV_IE4_CHAN_LAM_SIG:
                /* Just ignore this request: it is returned from LAM_CB() */
                goto NEXT_CHANNEL;

            default:
                value  = 0;
                rflags = CXRF_UNSUPPORTED;
        }

        ReturnInt32Datum(devid, chn, value, rflags);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(frolov_ie4, "Frolov IE4",
                   NULL, NULL,
                   sizeof(frolov_ie4_privrec_t), frolov_ie4_params,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   frolov_ie4_init_d, NULL, frolov_ie4_rw_p);
