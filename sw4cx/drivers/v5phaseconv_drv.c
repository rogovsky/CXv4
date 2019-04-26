#include <stdio.h>
#include <math.h>

#include "cxsd_driver.h"
#include "vdev.h"

#include "drv_i/v5phaseconv_drv_i.h"

//////////////////////////////////////////////////////////////////////
/* An excerpt from Gusev's epics/phr/src/main_cb.c */

#if 1  // Standard PI
  #define PI M_PI
#else  // Gusev's PI
  #define PI 3.1415926
#endif

#if 1
// [1,2]=Block3, [3,4]=Block2
static double p0[4] = { -4.202, 40.495, 32.696, -11.625 }; // fi0_X
static double ku[4] = {  6.048,  6.054,  5.984,   5.924 }; // Kux
static double zp[4] = {  1.778,  4.097,  3.859,   1.747 }; // zpX
static double zl[4] = {  3.044,  2.531,  2.573,   3.042 }; // zlX
static double rs[4] = {  0.250,  0.450,  0.500,   0.080 }; // rsX
static double b [4] = {  0.832,  0.788,  0.670,   0.696 }; // bX
static double g [4] = {  0.355,  0.350,  0.406,   0.403 }; // gX
#else
// [1,2]=Block1, [3,4]=Block2
static double p0[4] = { -4.202, -8.528, 32.696, -11.625 }; // fi0_X
static double ku[4] = {  6.000,  5.900,  5.984,   5.924 }; // Kux
static double zp[4] = {  1.435,  1.764,  3.859,   1.747 }; // zpX
static double zl[4] = {  3.142,  3.082,  2.573,   3.042 }; // zlX
static double rs[4] = {  0.025,  0.025,  0.500,   0.080 }; // rsX
static double b [4] = {  0.674,  0.810,  0.670,   0.696 }; // bX
static double g [4] = {  0.424,  0.376,  0.406,   0.403 }; // gX
#endif

static double
convertVoltToPhase(
    int	   k,
    double U )
{
    double zu, zu2, zp2 = zp[k] * zp[k];
    double phase;

    if ( U < 0.0 )
	U = 0.0;

    zu  = b[k] * pow( U * ku[k], g[k] ) - zl[k];
    zu2 = zu * zu;
    phase = atan( -2. * zp[k] * ( zu2 + zu * zp[k] + rs[k] * rs[k] ) /
		( zu2 * zp2 + rs[k] * rs[k] * ( zp2 - 1. ) - zu2 -
		   2. * zu * zp[k] - zp2 ) );
    zu = rs[k] * ( zp2 - 1. );
    zu = ( zp[k] - sqrt( zp2 * zp2 - zu * zu ) ) / ( ( zp2 - 1. ) * b[k] ) +
	   zl[k] / b[k];
    zu = pow( zu, 1. / g[k] );
    if ( U * ku[k] > zu )
	phase += PI;

    phase = ( phase * 180. ) / PI - p0[k];
    return( phase );
}

static double
convertPhaseToVolt(
    int	   k,
    double phase )
{
    double x,	  zp2 = zp[k] * zp[k];
    double a, a2, rs2 = rs[k] * rs[k];

    if ( phase < 0. )
	phase = 0.;

    phase += p0[k];
    a  = 1 - zp2;
    a2 = a * a;
    if ( ( phase > 89.9 ) && ( phase < 90.1 ) )
	x  = -( zp[k] - sqrt( zp2 * zp2 - rs2 * a2 ) ) / a;
    else {

	double T = tan( phase * PI / 180. );

	x  = sqrt( ( zp2 * zp2 - rs2 * a2 ) * T * T + 4. * rs2 * zp[k] * a * T +
		     zp2 * ( zp2 - 4. * rs2 ) );
	x  = ( phase < 90. ) ? ( T * zp[k] - zp2 - x ) :
			       ( T * zp[k] - zp2 + x );
	x /= 2. * zp[k] - a * T;
    }

    x = ( pow( ( x + zl[k] ) / b[k], 1. / g[k] ) ) / ku[k];
    return( x );
}

//////////////////////////////////////////////////////////////////////

enum
{
    C_DAC1,  C_DAC_n_base = C_DAC1,
    C_DAC2,
    C_DAC3,
    C_DAC4,
    SUBORD_NUMCHANS
};

static vdev_sodc_dsc_t hw2our_mapping[SUBORD_NUMCHANS] =
{
    [C_DAC1] = {"out0", VDEV_PRIV | VDEV_DO_RD_CONV, -1, 0, CXDTYPE_DOUBLE},
    [C_DAC2] = {"out1", VDEV_PRIV | VDEV_DO_RD_CONV, -1, 0, CXDTYPE_DOUBLE},
    [C_DAC3] = {"out2", VDEV_PRIV | VDEV_DO_RD_CONV, -1, 0, CXDTYPE_DOUBLE},
    [C_DAC4] = {"out3", VDEV_PRIV | VDEV_DO_RD_CONV, -1, 0, CXDTYPE_DOUBLE},
};

static const char *devstate_names[] = {"_devstate"};

typedef struct {
    vdev_context_t      ctx;
    vdev_sodc_cur_t     cur[SUBORD_NUMCHANS];
    vdev_sodc_cur_t     devstate_cur[countof(devstate_names)];
} privrec_t;

//--------------------------------------------------------------------

static void v5phaseconv_sodc_cb(int devid, void *devptr,
                                int sodc, int32 val)
{
  privrec_t      *me = (privrec_t *)devptr;
  int             nl;
  int             chn;
  void           *vp;
  float64         phase;

  static cxdtype_t dtype_f64 = CXDTYPE_DOUBLE;
  static int       n_1       = 1;

    if (sodc >= C_DAC_n_base  &&
        sodc <= C_DAC_n_base + V5PHASECONV_PHASE_n_count)
    {
        nl    = sodc - C_DAC_n_base;
        chn   = V5PHASECONV_PHASE_n_base + nl;
        phase = convertVoltToPhase(nl, me->cur[sodc].v.f64);
        vp    = &phase;
        ReturnDataSet(devid, 1,
                      &chn, &dtype_f64, &n_1, &vp, &(me->cur[sodc].flgs), NULL);
    }
}

static int v5phaseconv_init_d(int devid, void *devptr,
                              int businfocount, int businfo[],
                              const char *auxinfo)
{
  privrec_t      *me = (privrec_t *)devptr;

    me->ctx.num_sodcs      = SUBORD_NUMCHANS;
    me->ctx.map            = hw2our_mapping;
    me->ctx.cur            = me->cur;
    me->ctx.devstate_count = countof(devstate_names);
    me->ctx.devstate_names = devstate_names;
    me->ctx.devstate_cur   = me->devstate_cur;
    me->ctx.do_rw          = NULL;
    me->ctx.sodc_cb        = v5phaseconv_sodc_cb;

    return vdev_init(&(me->ctx), devid, devptr, -1, auxinfo);
}

static void v5phaseconv_term_d(int devid __attribute__((unused)), void *devptr)
{
  privrec_t      *me = (privrec_t *)devptr;

    vdev_fini(&(me->ctx));
}

static void v5phaseconv_rw_p(int devid, void *devptr,
                             int action,
                             int count, int *addrs,
                             cxdtype_t *dtypes, int *nelems, void **values) 
{
  privrec_t      *me = devptr;
  int             n;     // channel N in addrs[]/.../values[] (loop ctl var)
  int             chn;   // channel
  double          val;
  int             nl;
  double          volts;

    for (n = 0;  n < count;  n++)
    {
        chn = addrs[n];
        if (action == DRVA_WRITE)
        {
            if (nelems[n] != 1) goto NEXT_CHANNEL;
            if      (dtypes[n] == CXDTYPE_SINGLE) val = *((float32*)(values[n]));
            else if (dtypes[n] == CXDTYPE_DOUBLE) val = *((float64*)(values[n]));
            else goto NEXT_CHANNEL;
        }
        else
            val = NAN;

        if (chn >= V5PHASECONV_PHASE_n_base  &&
            chn <  V5PHASECONV_PHASE_n_base + V5PHASECONV_PHASE_n_count)
        {
            nl = chn - V5PHASECONV_PHASE_n_base;
            if (action == DRVA_WRITE)
            {
                volts = convertPhaseToVolt(nl, val);
                cda_snd_ref_data(me->cur[C_DAC_n_base + nl].ref,
                                 CXDTYPE_DOUBLE, 1, &volts);
            }
        }
        else
            ReturnInt32Datum(devid, chn, 0, CXRF_UNSUPPORTED);

 NEXT_CHANNEL:;
    }
}

DEFINE_CXSD_DRIVER(v5phaseconv, "VEPP5 phase shifters' U<->phase conversion",
                   NULL, NULL,
                   sizeof(privrec_t), NULL,
                   0, 0,
                   NULL, 0,
                   NULL,
                   -1, NULL, NULL,
                   v5phaseconv_init_d,
                   v5phaseconv_term_d,
                   v5phaseconv_rw_p);
