#include "cxsd_driver.h"

#include <math.h>

#include "drv_i/frolov_d16_drv_i.h"


enum
{
    A_A  = 0,
    A_B  = 4,
    
    A_RT = 0,
    A_RF = 1,
    A_RM = 2,
    A_RS = 3
};


enum
{
    LAM_MODE_EASY = 0,  // Standard D16, don't use LAMs
    LAM_MODE_MSKS = 1,  // Standard D16, use LAMs with masking START afterwards and unmasking upon DO_SHOT
    LAM_MODE_D16P = 2,  // D16P: use LAMs and D16P-specific commands; don't mask START
};

enum
{
    AB_MODE_AB     = 0,
    AB_MODE_A_ONLY = 1,
};

#define MIN_F_IN_NS 0
#define MAX_F_IN_NS 2e9 // 2e9ns==2s

typedef struct
{
    int      devid;
    int      N;
    int      lam_mode;
    int      ab_mode;
    float64  F_IN_NS;

    int      init_val_of_kstart;
    int      init_val_of_kclk_n;
    int      init_val_of_fclk_sel;
    int      init_val_of_start_sel;
    int      init_val_of_mode;
} frolov_d16_privrec_t;

static psp_lkp_t frolov_d16_kclk_n_lkp   [] =
{
    {"1", FROLOV_D16_V_KCLK_1},
    {"2", FROLOV_D16_V_KCLK_2},
    {"4", FROLOV_D16_V_KCLK_4},
    {"8", FROLOV_D16_V_KCLK_8},
    {NULL, 0},
};
static psp_lkp_t frolov_d16_fclk_sel_lkp []=
{
    {"fin",    FROLOV_D16_V_FCLK_FIN},
    {"quartz", FROLOV_D16_V_FCLK_QUARTZ},
    {"impact", FROLOV_D16_V_FCLK_IMPACT},
    {NULL, 0},
};
static psp_lkp_t frolov_d16_start_sel_lkp[] =
{
    {"start", FROLOV_D16_V_START_START},
    {"y1",    FROLOV_D16_V_START_Y1},
    {"camac", FROLOV_D16_V_START_Y1},
    {"50hz",  FROLOV_D16_V_START_50HZ},
    {NULL, 0},
};
static psp_lkp_t frolov_d16_mode_lkp     [] =
{
    {"continuous", FROLOV_D16_V_MODE_CONTINUOUS},
    {"cont",       FROLOV_D16_V_MODE_CONTINUOUS},
    {"oneshot",    FROLOV_D16_V_MODE_ONESHOT},
    {"one",        FROLOV_D16_V_MODE_ONESHOT},
    {"1",          FROLOV_D16_V_MODE_ONESHOT},
    {NULL, 0},
};
static psp_paramdescr_t frolov_d16_params[] =
{
    PSP_P_FLAG  ("easy",      frolov_d16_privrec_t, lam_mode, LAM_MODE_EASY,  1),
    PSP_P_FLAG  ("msks",      frolov_d16_privrec_t, lam_mode, LAM_MODE_MSKS,  0),
    PSP_P_FLAG  ("d16p",      frolov_d16_privrec_t, lam_mode, LAM_MODE_D16P,  0),

    PSP_P_FLAG  ("ab",        frolov_d16_privrec_t, ab_mode,  AB_MODE_AB,     1),
    PSP_P_FLAG  ("a_only",    frolov_d16_privrec_t, ab_mode,  AB_MODE_A_ONLY, 0),

    PSP_P_REAL  ("f_in",      frolov_d16_privrec_t, F_IN_NS, 0.0, 0.0, 0.0),

    PSP_P_LOOKUP("kclk",      frolov_d16_privrec_t, init_val_of_kclk_n,    -1, frolov_d16_kclk_n_lkp),
    PSP_P_INT   ("kstart",    frolov_d16_privrec_t, init_val_of_kstart,    -1, 0, 255),
    PSP_P_LOOKUP("fclk_sel",  frolov_d16_privrec_t, init_val_of_fclk_sel,  -1, frolov_d16_fclk_sel_lkp),
    PSP_P_LOOKUP("start_sel", frolov_d16_privrec_t, init_val_of_start_sel, -1, frolov_d16_start_sel_lkp),
    PSP_P_LOOKUP("mode",      frolov_d16_privrec_t, init_val_of_mode,      -1, frolov_d16_mode_lkp),

    PSP_P_END()
};


static void frolov_d16_rw_p(int devid, void *devptr,
                            int action,
                            int count, int *addrs,
                            cxdtype_t *dtypes, int *nelems, void **values);

static void WriteOne (frolov_d16_privrec_t *me, int nc, int32 value)
{
  cxdtype_t  dt_int32 = CXDTYPE_INT32;
  int        nels_1   = 1;
  void      *vp       = &value;

    frolov_d16_rw_p(me->devid, me,
                    DRVA_WRITE,
                    1, &nc,
                    &dt_int32, &nels_1, &vp);
}

static void ReturnOne(frolov_d16_privrec_t *me, int nc)
{
    frolov_d16_rw_p(me->devid, me,
                    DRVA_READ,
                    1, &nc,
                    NULL, NULL, NULL);
}

static void ReturnVs(frolov_d16_privrec_t *me)
{
  static int addrs[FROLOV_D16_NUMOUTPUTS] = {
      FROLOV_D16_CHAN_V_n_base + 0,
      FROLOV_D16_CHAN_V_n_base + 1,
      FROLOV_D16_CHAN_V_n_base + 2,
      FROLOV_D16_CHAN_V_n_base + 3,
  };

    frolov_d16_rw_p(me->devid, me,
                    DRVA_READ,
                    FROLOV_D16_NUMOUTPUTS, addrs,
                    NULL, NULL, NULL);
}

static void SetALLOFF(frolov_d16_privrec_t *me, int32 value)
{
  int        chn      = FROLOV_D16_CHAN_ALLOFF;
  cxdtype_t  dt_int32 = CXDTYPE_INT32;
  int        nels_1   = 1;
  void      *vp       = &value;

    frolov_d16_rw_p(me->devid, me,
                    DRVA_WRITE,
                    1, &chn,
                    &dt_int32, &nels_1, &vp);
}


static void LAM_CB(int devid, void *devptr)
{
  frolov_d16_privrec_t *me = (frolov_d16_privrec_t*)devptr;
  int                   c;

    // Mask further "START"s
    if      (me->lam_mode == LAM_MODE_MSKS)
        SetALLOFF(me, 1);
    else if (me->lam_mode == LAM_MODE_D16P)
        ReturnOne(me, FROLOV_D16_CHAN_WAS_START);
    // Drop LAM
    DO_NAF(CAMAC_REF, me->N, 0, 10, &c);
    // And signal LAM upwards
    ReturnInt32Datum(devid, FROLOV_D16_CHAN_LAM_SIG, 0, 0);
}

static int frolov_d16_init_d(int devid, void *devptr,
                             int businfocount, int *businfo,
                             const char *auxinfo)
{
  frolov_d16_privrec_t *me = (frolov_d16_privrec_t*)devptr;
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

    SetChanReturnType(devid, FROLOV_D16_CHAN_WAS_START, 1, IS_AUTOUPDATED_YES);
    if (me->lam_mode != LAM_MODE_D16P)
        ReturnInt32Datum(devid, FROLOV_D16_CHAN_WAS_START, 0, CXRF_UNSUPPORTED);

    SetChanReturnType(devid, FROLOV_D16_CHAN_LAM_SIG,   1, IS_AUTOUPDATED_YES);
    if (me->lam_mode != LAM_MODE_EASY)
    {
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

        // Here we use the fact that status reg was just read
        if (me->lam_mode == LAM_MODE_D16P  &&  (c & 1 << 7) == 0)
            DoDriverLog(devid, DRIVERLOG_EMERG,
                        "D16P mode is requested, but statusReg=%d (msb=0)", c);
    }
    else
        ReturnInt32Datum(devid, FROLOV_D16_CHAN_LAM_SIG, 0, CXRF_UNSUPPORTED);

    if (me->init_val_of_kstart   >= 0)
        WriteOne(me, FROLOV_D16_CHAN_KSTART,    me->init_val_of_kstart);
    if (me->init_val_of_fclk_sel  >= 0)
        WriteOne(me, FROLOV_D16_CHAN_FCLK_SEL,  me->init_val_of_fclk_sel);
    if (me->init_val_of_start_sel >= 0)
        WriteOne(me, FROLOV_D16_CHAN_START_SEL, me->init_val_of_start_sel);
    if (me->init_val_of_mode      >= 0)
        WriteOne(me, FROLOV_D16_CHAN_MODE,      me->init_val_of_mode);

    return DEVSTATE_OPERATING;
}

static void frolov_d16_rw_p(int devid, void *devptr,
                            int action,
                            int count, int *addrs,
                            cxdtype_t *dtypes, int *nelems, void **values)
{
  frolov_d16_privrec_t *me = (frolov_d16_privrec_t*)devptr;

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
  int                   c_Bi;
  double                v_fclk;
  double                quant;
  double                v_R;

  double                dval;
  void                 *vp;
  static cxdtype_t      dtype_f64 = CXDTYPE_DOUBLE;
  static int            n_1       = 1;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
        if (action == DRVA_WRITE  &&
            (chn >= FROLOV_D16_DOUBLE_CHAN_base  &&
             chn <= FROLOV_D16_DOUBLE_CHAN_base + FROLOV_D16_DOUBLE_CHAN_count - 1))
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
            case FROLOV_D16_CHAN_A_n_base ...
                 FROLOV_D16_CHAN_A_n_base   + FROLOV_D16_NUMOUTPUTS - 1:
                l = chn - FROLOV_D16_CHAN_A_n_base;
                if (action == DRVA_WRITE)
                {
                    if (value < 0)      value = 0;
                    if (value > 0xFFFF) value = 0xFFFF;
                    c = value;
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_A+l, 16, &c));
                    ReturnOne(me, FROLOV_D16_CHAN_V_n_base + l);
                }
                else
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_A+l, 0, &c));
                    value = c;
                }
                break;

            case FROLOV_D16_CHAN_B_n_base ...
                 FROLOV_D16_CHAN_B_n_base   + FROLOV_D16_NUMOUTPUTS - 1:
                l = chn - FROLOV_D16_CHAN_B_n_base;
                if (action == DRVA_WRITE)
                {
                    if (value < 0)    value = 0;
                    if (value > 0xFF) value = 0xFF;
                    c = value;
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_B+l, 16, &c));
                    ReturnOne(me, FROLOV_D16_CHAN_V_n_base + l);
                }
                else
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_B+l, 0, &c));
                    value = c;
                }
                break;

            case FROLOV_D16_CHAN_V_n_base ...
                 FROLOV_D16_CHAN_V_n_base   + FROLOV_D16_NUMOUTPUTS - 1:
                l = chn - FROLOV_D16_CHAN_V_n_base;

                DO_NAF(CAMAC_REF, me->N, A_RF, 1, &c_rf);
                DO_NAF(CAMAC_REF, me->N, A_RS, 1, &c_rs);
                v_kclk = 1 << (c_rf & 3); // {0,1,2,3}=>{1,2,4,8}
                v_fclk = ((c_rs & 3) == 0)? me->F_IN_NS : 40; // 0(Fin)=>???, else=>40ns
                quant = v_fclk * v_kclk;

                rflags = 0;
                if (action == DRVA_WRITE  &&  quant != 0.0)
                {
                    if (dval < 0) dval = 0;

                    if (me->ab_mode == AB_MODE_AB)
                    {
#if 0
                        // NO!!!  May NOT round(), because remainder from division
                        //        is still used for B
                        c_Ai = round(dval / quant);  if (c_Ai > 65535) c_Ai = 65535;
                        v_R  = dval - c_Ai * quant;  if (v_R  < 0)     v_R  = 0;
#else
                        // ? c_Ai = remquo(dval, quant, &v_R) ?  No, too underspecified to rely upon.
                        c_Ai = dval / quant;         if (c_Ai > 65535) c_Ai = 65535;
                        v_R  = fmod(dval, quant) /*+ 0.125*/;
#endif
////fprintf(stderr, "dval=%8.3f c_Ai=%d v_R=%8.3f\n", dval, c_Ai, v_R);
                        c_Bi = v_R * 4; /* =/0.25 */ if (c_Bi > 255)   c_Bi = 255;

                        rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_A+l, 16, &c_Ai));
                        rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_B+l, 16, &c_Bi));

                        ReturnOne(me, FROLOV_D16_CHAN_A_n_base + l);
                        ReturnOne(me, FROLOV_D16_CHAN_B_n_base + l);
                    }
                    else /* ab_mode == AB_MODE_A_ONLY */
                    {
                        c_Ai = round(dval / quant);  if (c_Ai > 65535) c_Ai = 65535;
                        rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_A+l, 16, &c_Ai));

                        ReturnOne(me, FROLOV_D16_CHAN_A_n_base + l);
                    }
                }
                // Perform "read" anyway
                rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_A+l, 0, &c_Ai));
                rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_B+l, 0, &c_Bi));
                if (quant != 0.0)
                {
                    if (me->ab_mode == AB_MODE_AB)
                        dval = c_Ai * quant + c_Bi / 4.;
                    else /* ab_mode == AB_MODE_A_ONLY */
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

            case FROLOV_D16_CHAN_OFF_n_base ...
                 FROLOV_D16_CHAN_OFF_n_base + FROLOV_D16_NUMOUTPUTS - 1:
            case FROLOV_D16_CHAN_ALLOFF: // "ALL" immediately follows individual ones in both channels and bits
                l = chn - FROLOV_D16_CHAN_OFF_n_base;
                if (action == DRVA_WRITE)
                {
                    value = (value != 0);
                    rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RM, 1,  &c));
                    c = (c &~ (1 << l)) | (value << l);
                    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_RM, 17, &c));
                }
                else
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RM, 1, &c));
                    value  = ((c & (1 << l)) != 0);
                }
                break;

            case FROLOV_D16_CHAN_DO_SHOT:
                rflags = 0;
                if (action == DRVA_WRITE  &&  value == CX_VALUE_COMMAND)
                {
                    if      (me->lam_mode == LAM_MODE_EASY)
                        rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, 0, 10, &c));
                    else if (me->lam_mode == LAM_MODE_MSKS)
                        SetALLOFF(me, 0);
                    else if (me->lam_mode == LAM_MODE_D16P)
                    {
                        rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, 1, 25, &c));
                        ReturnOne(me, FROLOV_D16_CHAN_WAS_START);
                    }
                }
                value = 0;
                break;

            case FROLOV_D16_CHAN_ONES_STOP:
                if      (me->lam_mode != LAM_MODE_D16P)
                    rflags = CXRF_UNSUPPORTED;
                else if (action == DRVA_WRITE  &&  value == CX_VALUE_COMMAND)
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, 2, 25, &c));
                }
                else
                    rflags = 0;
                value = 0;
                break;

            case FROLOV_D16_CHAN_KSTART:
                if (action == DRVA_WRITE)
                {
                    if (value < 0)   value = 0;
                    if (value > 255) value = 255;
                    c = value;
                    rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RT, 17, &c));
                }
                else
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RT, 1, &c));
                    value  = c;
                }
                break;

            case FROLOV_D16_CHAN_KCLK_N:
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

            case FROLOV_D16_CHAN_FCLK_SEL:
                if (action == DRVA_WRITE)
                {
                    if (value < 0) value = 0;
                    if (value > 2) value = 2;
                    rflags  = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RS, 1,  &c));
                    c = (c &~ (3 << 0)) | (value << 0);
                    rflags |= x2rflags(DO_NAF(CAMAC_REF, me->N, A_RS, 17, &c));
                    ReturnVs(me);
                }
                else
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RS, 1, &c));
                    c = (c >> 0) & 3;
                    if (c == 3) c = 2; // Since '11' means the same as '10'
                    value = c;
                }
                break;

            case FROLOV_D16_CHAN_START_SEL:
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

            case FROLOV_D16_CHAN_MODE:
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

            case FROLOV_D16_CHAN_F_IN_HZ:
                if (action == DRVA_WRITE)
                {
                    // Convert HZ to ns
                    if (dval <= 0.0) dval = 0.0; else dval = 1e9 / dval;
                    // Impose regular limits on F_IN_NS
                    if (dval < MIN_F_IN_NS) dval = MIN_F_IN_NS;
                    if (dval > MAX_F_IN_NS) dval = MAX_F_IN_NS;
                    me->F_IN_NS = dval;
                    ReturnOne(me, FROLOV_D16_CHAN_F_IN_NS);
                    ReturnVs (me);
                }
                if (me->F_IN_NS == 0.0) dval = 3.0; else dval = 1e9 / me->F_IN_NS;
                vp     = &dval;
                rflags = 0;
                ReturnDataSet(devid, 1,
                              &chn, &dtype_f64, &n_1, &vp, &rflags, NULL);
                goto NEXT_CHANNEL;

            case FROLOV_D16_CHAN_F_IN_NS:
                if (action == DRVA_WRITE)
                {
                    if (dval < MIN_F_IN_NS) dval = MIN_F_IN_NS;
                    if (dval > MAX_F_IN_NS) dval = MAX_F_IN_NS;
                    me->F_IN_NS = dval;
                    ReturnOne(me, FROLOV_D16_CHAN_F_IN_HZ);
                    ReturnVs (me);
                }
                vp     = &(me->F_IN_NS);
                rflags = 0;
                ReturnDataSet(devid, 1,
                              &chn, &dtype_f64, &n_1, &vp, &rflags, NULL);
                goto NEXT_CHANNEL;

            case FROLOV_D16_CHAN_WAS_START:
                if (me->lam_mode == LAM_MODE_D16P)
                {
                    rflags = x2rflags(DO_NAF(CAMAC_REF, me->N, A_RS, 1, &c));
                    value  = ((c & (1 << 6)) != 0);
                }
                else
                {
                    value  = 0;
                    rflags = CXRF_UNSUPPORTED;
                }
                break;

            case FROLOV_D16_CHAN_LAM_SIG:
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


DEFINE_CXSD_DRIVER(frolov_d16, "Frolov D16",
                   NULL, NULL,
                   sizeof(frolov_d16_privrec_t), frolov_d16_params,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   frolov_d16_init_d, NULL, frolov_d16_rw_p);
