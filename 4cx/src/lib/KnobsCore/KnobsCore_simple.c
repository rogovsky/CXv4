#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "misc_macros.h"
#include "paramstr_parser.h"

#include "Knobs.h"
#include "KnobsP.h"


static void Simple_SndPhys_m(DataKnob k, double v)
{
  simpleknob_cb_t  cb = k->upprivptr1;
    
    if (cb != NULL)
        cb(k, v, k->upprivptr2);
}

dataknob_cont_vmt_t Simple_VMT = {.SndPhys = Simple_SndPhys_m};

static data_knob_t Simple_fakecont = {.vmtlink = (void *)&Simple_VMT};


typedef struct
{
    char   *ident;
    char   *label;
    char   *tip;
    char   *comment;
    char   *options;
    char   *layinfo;
    char   *geoinfo;

    char   *look;
    char   *style;

    int     is_rw;
    int     is_local;
    int     no_params;

    char   *items;
    char   *units;
    char    dpyfmt[16];
    
    double  step;
    double  safeval;
    double  grpcoeff;
    double  param_rsrvd1;
    double  alwd_min;
    double  alwd_max;
    double  norm_min;
    double  norm_max;
    double  yelw_min;
    double  yelw_max;
    double  disp_min;
    double  disp_max;
} simplerec_t;

static psp_paramdescr_t text2simplerec[] =
{
    //PSP_P_MSTRING("ident",   simplerec_t, ident,      NULL, 100),
    PSP_P_MSTRING("label",   simplerec_t, label,      NULL, 100),
    PSP_P_MSTRING("tip",     simplerec_t, tip,        NULL, 100),
    PSP_P_MSTRING("comment", simplerec_t, comment,    NULL, 0),
    PSP_P_MSTRING("options", simplerec_t, options,    NULL, 0),
    //PSP_P_MSTRING("layinfo", simplerec_t, layinfo,    NULL, 0),
    //PSP_P_MSTRING("geoinfo", simplerec_t, geoinfo,    NULL, 0),

    PSP_P_MSTRING("look",    simplerec_t, look,       NULL, 0),
    PSP_P_MSTRING("style",   simplerec_t, style,      NULL, 0),
    
    PSP_P_MSTRING("items",   simplerec_t, items,      NULL, 0),
    PSP_P_MSTRING("units",   simplerec_t, units,      NULL, 100),
    PSP_P_STRING ("dpyfmt",  simplerec_t, dpyfmt,     "%8.3f"),
    
    PSP_P_FLAG("ro",         simplerec_t, is_rw,      0,    1),
    PSP_P_FLAG("rw",         simplerec_t, is_rw,      1,    0),
    PSP_P_FLAG("noparams",   simplerec_t, no_params,  1,    0),

    PSP_P_FLAG("local",      simplerec_t, is_local,   1,    0),

    PSP_P_REAL("step",       simplerec_t, step,       1.0,  0.0, 0.0),
    PSP_P_REAL("safeval",    simplerec_t, safeval,    0.0,  0.0, 0.0),
    PSP_P_REAL("grpcoeff",   simplerec_t, grpcoeff,   0.0,  0.0, 0.0),
    PSP_P_REAL("minalwd",    simplerec_t, alwd_min,   0.0,  0.0, 0.0),
    PSP_P_REAL("maxalwd",    simplerec_t, alwd_max,   0.0,  0.0, 0.0),
    PSP_P_REAL("minnorm",    simplerec_t, norm_min,   0.0,  0.0, 0.0),
    PSP_P_REAL("maxnorm",    simplerec_t, norm_max,   0.0,  0.0, 0.0),
    PSP_P_REAL("minyelw",    simplerec_t, yelw_min,   0.0,  0.0, 0.0),
    PSP_P_REAL("maxyelw",    simplerec_t, yelw_max,   0.0,  0.0, 0.0),
    PSP_P_REAL("mindisp",    simplerec_t, disp_min,   0.0,  0.0, 0.0),
    PSP_P_REAL("maxdisp",    simplerec_t, disp_max,   0.0,  0.0, 0.0),
    
    PSP_P_END()
};


DataKnob  CreateSimpleKnob(const char      *spec,     // Knob specification
                           int              options,
                           CxWidget         parent,   // Parent widget
                           simpleknob_cb_t  cb,       // Callback proc
                           void            *privptr)  // Pointer to pass to cb
{
  int            r;
  simplerec_t    rec;
  data_knob_t   *k      = NULL;
  CxKnobParam_t *params = NULL;
    
    /* First, let's parse the requirements */
    bzero(&rec, sizeof(rec));
    r = psp_parse(spec, NULL, &rec, '=', " \t", "", text2simplerec);
    if (r != PSP_R_OK)
    {
        fprintf(stderr, "%s(): psp_parse(): %s\n", __FUNCTION__, psp_error());
    }

    if ((options & SIMPLEKNOB_OPT_READONLY) != 0) rec.is_rw = 0;

    /* Second, allocate a knob and its parameters, and fill its data */
    if ((k = malloc(sizeof(*k))) == NULL) goto ERREXIT;
    bzero(k, sizeof(*k));
    if (!rec.no_params)
    {
        if ((params = malloc(sizeof(*params) * DATAKNOB_NUM_STD_PARAMS)) == NULL)
            goto ERREXIT;
        bzero(params, sizeof(*params) * DATAKNOB_NUM_STD_PARAMS);
    }

    k->type      = DATAKNOB_KNOB;
    k->look      = rec.look;
    k->options   = rec.options;

    k->ident     = rec.ident;
    k->label     = rec.label;
    k->tip       = rec.tip;
    k->comment   = rec.comment;
    k->style     = rec.style;
    k->layinfo   = rec.layinfo;
    k->geoinfo   = rec.geoinfo;

    k->is_rw     = rec.is_rw;

    if (rec.is_local) k->behaviour |= DATAKNOB_B_NO_WASJUSTSET;
    
    k->u.k.items = rec.items;
    k->u.k.units = rec.units;
    memcpy(
    k->u.k.dpyfmt, rec.dpyfmt, sizeof(k->u.k.dpyfmt));

    if (!rec.no_params)
    {
        params[DATAKNOB_PARAM_STEP    ].value = rec.step;
        params[DATAKNOB_PARAM_SAFEVAL ].value = rec.safeval;
        params[DATAKNOB_PARAM_GRPCOEFF].value = rec.grpcoeff;
        params[DATAKNOB_PARAM_ALWD_MIN].value = rec.alwd_min;
        params[DATAKNOB_PARAM_ALWD_MAX].value = rec.alwd_max;
        params[DATAKNOB_PARAM_NORM_MIN].value = rec.norm_min;
        params[DATAKNOB_PARAM_NORM_MAX].value = rec.norm_max;
        params[DATAKNOB_PARAM_YELW_MIN].value = rec.yelw_min;
        params[DATAKNOB_PARAM_YELW_MAX].value = rec.yelw_max;
        params[DATAKNOB_PARAM_DISP_MIN].value = rec.disp_min;
        params[DATAKNOB_PARAM_DISP_MAX].value = rec.disp_max;
        k->u.k.params     = params;
        k->u.k.num_params = DATAKNOB_NUM_STD_PARAMS;
    }

    k->curstate  = KNOBSTATE_NONE;
    
    /* Third, give birth to it */
    if (CreateKnob(k, parent) != 0) goto ERREXIT;

    /* Finally, store simple-data and return it */

    k->uplink     = &Simple_fakecont;
    k->upprivptr1 = cb;
    k->upprivptr2 = privptr;

    return k;

 ERREXIT:
    safe_free(params);
    safe_free(k);
    psp_free (&rec, text2simplerec);
    return NULL;
}

void      SetSimpleKnobValue(DataKnob k, double v)
{
  dataknob_knob_data_t *kd      = &(k->u.k);
  time_t                timenow = time(NULL);
  rflags_t              rflags;
  
    /* Is it a knob at all? */
    if (k->type != DATAKNOB_KNOB) return;
    
    kd->curv = v;

    /* Should select the value-dependent flags first... */
    rflags = choose_knob_rflags(k, 0, v);
    //!!! At this point can compare old with new for ALARM accounting
    k->currflags = rflags;
    
    /* ...than colorize... */
    set_knobstate(k, choose_knobstate(k, rflags));
    
    /* ...and finally show the value */
    if (
        !k->is_rw  ||
        (
         difftime(timenow, k->usertime) > KNOBS_USERINACTIVITYLIMIT  &&
         !k->wasjustset
        )
       )
        set_knob_controlvalue(k, v, 0);
    
    k->wasjustset = 0;
}

void      SetSimpleKnobState(DataKnob k, knobstate_t newstate)
{
    set_knobstate(k, newstate);
}

CxWidget  GetKnobWidget(DataKnob k)
{
    return k->w;
}
