#include "cxsd_driver.h"

#include "drv_i/g0401_drv_i.h"


enum
{
    CREG_T_QUANT_MODE_mask   = 7,
    CREG_BREAK_ON_OUT7       = 1 << (4+1),
    CREG_DISABLE_COPY_ON_END = 1 << (5+1),
};


typedef struct
{
    int       N;

    int       t_quant_mode;
    int       break_on_ch7;
    rflags_t  creg_rfl;

    int       val_cache[G0401_CHAN_OUT_count];
    rflags_t  rfl_cache[G0401_CHAN_OUT_count];
} g0401_privrec_t;


static psp_lkp_t g0401_t_quant_lkp[] =
{
    {"12800", 0},
    { "6400", 1},
    { "3200", 2},
    { "1600", 3},
    {  "800", 4},
    {  "400", 5},
    {  "200", 6},
    {  "100", 7},
    {NULL, 0},
};

static psp_paramdescr_t g0401_params[] =
{
    PSP_P_LOOKUP("t_quant",      g0401_privrec_t, t_quant_mode, -1, g0401_t_quant_lkp),
    PSP_P_INT   ("break_on_ch7", g0401_privrec_t, break_on_ch7, -1, 0, 1),
    PSP_P_END()
};

static int g0401_init_d(int devid, void *devptr,
                        int businfocount, int *businfo,
                        const char *auxinfo)
{
  g0401_privrec_t *me = (g0401_privrec_t*)devptr;

  int              l;

  int              w;
  
    me->N = businfo[0];

    // We can't read output settings, so pre-fill as "unknown"
    for (l = 0;  l < G0401_CHAN_OUT_count;  l++)
    {
        me->val_cache[l] = 0;
        me->rfl_cache[l] = CXRF_CAMAC_NO_Q;
    }

    // Read current period-mode setting...
    me->creg_rfl = status2rflags(DO_NAF(CAMAC_REF, me->N, 8, 0, &w));
    if (me->t_quant_mode < 0)
        me->t_quant_mode = w & CREG_T_QUANT_MODE_mask;
    // ...plus break_on_ch7 setting, if not pre-set in auxinfo...
    if (me->break_on_ch7 < 0)
        me->break_on_ch7 = ((w & CREG_BREAK_ON_OUT7) != 0);
    // ...and pre-program (reset) "disable copy on end" state
    w = (me->t_quant_mode & CREG_T_QUANT_MODE_mask) |
        (me->break_on_ch7 ? CREG_BREAK_ON_OUT7 : 0);
    DO_NAF(CAMAC_REF, me->N, 8, 16, &w);

    SetChanRDs(devid, G0401_CHAN_QUANT_N_base, G0401_CHAN_OUT_count,
               10.0, 0.0);

    return DEVSTATE_OPERATING;
}

static void g0401_rw_p(int devid, void *devptr,
                       int action,
                       int count, int *addrs,
                       cxdtype_t *dtypes, int *nelems, void **values)
{
  g0401_privrec_t *me = (g0401_privrec_t*)devptr;

  int              n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int              chn;   // channel
  int              line;

  int32            value;
  rflags_t         rflags;

  int              w;

  int              quant;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
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

        if      (chn >= G0401_CHAN_RAW_V_n_base  &&
                 chn <  G0401_CHAN_RAW_V_n_base + G0401_CHAN_OUT_count)
        {
            line = chn - G0401_CHAN_RAW_V_n_base;
            if (action == DRVA_WRITE)
            {
                // Impose limits
                if (value < 0)      value = 0;
                if (value > 0xFFFF) value = 0xFFFF;
                // Do write
                me->val_cache[line] = value;
                w = value;
                me->rfl_cache[line] = status2rflags(DO_NAF(CAMAC_REF, me->N, line, 16, &w));
                // Return alias-chan value
                quant = (1 << (7 - me->t_quant_mode));
                ReturnInt32Datum(devid, G0401_CHAN_QUANT_N_base + line,
                                 value * quant, me->rfl_cache[line]);
            }
            value  = me->val_cache[line];
            rflags = me->rfl_cache[line];
        }
        else if (chn >= G0401_CHAN_QUANT_N_base  &&
                 chn <  G0401_CHAN_QUANT_N_base + G0401_CHAN_OUT_count)
        {
            line = chn - G0401_CHAN_QUANT_N_base;
            quant = (1 << (7 - me->t_quant_mode));
            if (action == DRVA_WRITE)
            {
                // Impose limits and scale from quants to raw
                if (value < 0) value = 0;
                value /= quant;
                if (value > 0xFFFF) value = 0xFFFF;
                // Do write
                me->val_cache[line] = value;
                w = value;
                me->rfl_cache[line] = status2rflags(DO_NAF(CAMAC_REF, me->N, line, 16, &w));
                // Return alias-chan value
                ReturnInt32Datum(devid, G0401_CHAN_RAW_V_n_base + line,
                                 me->val_cache[line], me->rfl_cache[line]);
            }
            value  = me->val_cache[line] * quant;
            rflags = me->rfl_cache[line];
        }
        else if (chn == G0401_CHAN_PROGSTART)
        {
            if (action == DRVA_WRITE  &&  value == CX_VALUE_COMMAND)
                DO_NAF(CAMAC_REF, me->N, 0, 25, &w);
            value  = 0;
            rflags = 0;
        }
        else if (chn == G0401_CHAN_T_QUANT_MODE)
        {
            if (action == DRVA_WRITE)
            {
                if (value < 0) value = 0;
                if (value > 7) value = 7;
                me->t_quant_mode = value;
                w = value;
                me->creg_rfl = status2rflags(DO_NAF(CAMAC_REF, me->N, 8, 0, &w));
            }
            value  = me->t_quant_mode;
            rflags = me->creg_rfl;
        }
        else if (chn == G0401_CHAN_BREAK_ON_CH7)
        {
            if (action == DRVA_WRITE)
            {
                me->break_on_ch7 = (value != 0);
                w = (me->t_quant_mode & CREG_T_QUANT_MODE_mask) |
                    (me->break_on_ch7 ? CREG_BREAK_ON_OUT7 : 0);
                DO_NAF(CAMAC_REF, me->N, 8, 16, &w);
            }
            value  = me->break_on_ch7;
            rflags = me->creg_rfl;
        }
        else
        {
            value  = 0;
            rflags = CXRF_UNSUPPORTED;
        }

        ReturnInt32Datum(devid, chn, value, rflags);

 NEXT_CHANNEL:;
    }
}


DEFINE_CXSD_DRIVER(g0401, "G0401",
                   NULL, NULL,
                   sizeof(g0401_privrec_t), g0401_params,
                   1, 1,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   g0401_init_d, NULL, g0401_rw_p);
