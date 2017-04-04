#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/Form.h>
#include <Xm/PushB.h>

#include "cda.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_cda_scenario_knob.h"


typedef struct
{
    Widget  start_btn;
    Widget  stop_btn;
} cda_scenario_privrec_t;

static psp_paramdescr_t text2cda_scenarioopts[] =
{

    PSP_P_END()
};

static void StartCB(Widget     w         __attribute__((unused)),
                    XtPointer  closure,
                    XtPointer  call_data __attribute__((unused)))
{
  DataKnob     k  = (DataKnob)closure;
  
    MotifKnobs_SetControlValue(k, CX_VALUE_COMMAND, 1);
}

static void StopCB (Widget     w         __attribute__((unused)),
                    XtPointer  closure,
                    XtPointer  call_data __attribute__((unused)))
{
  DataKnob     k  = (DataKnob)closure;
  CxDataRef_t  wr;

    wr                               = k->u.k.wr_ref;
    if (!cda_ref_is_sensible(wr)) wr = k->u.k.rd_ref;

    cda_stop_formula(wr);
}

static int ScenarioCreate_m(DataKnob k, CxWidget parent)
{
  cda_scenario_privrec_t *me = (cda_scenario_privrec_t *)k->privptr;
  XmString                s;

    k->behaviour |= DATAKNOB_B_IS_LIGHT | DATAKNOB_B_IS_BUTTON | DATAKNOB_B_IGN_OTHEROP | DATAKNOB_B_STEP_FXD;

    k->w =
        ABSTRZE(XtVaCreateManagedWidget("scenarioForm",
                                        xmFormWidgetClass,
                                        CNCRTZE(parent),
                                        XmNshadowThickness, 0,
                                        NULL));
    MotifKnobs_HookButton3(k, CNCRTZE(k->w));

    me->start_btn = XtVaCreateManagedWidget("push_i",
                                            xmPushButtonWidgetClass,
                                            CNCRTZE(k->w),
                                            XmNtraversalOn, k->is_rw,
                                            XmNlabelString, s = XmStringCreateLtoR("Start", xh_charset),
                                            NULL);
    XmStringFree(s);
    XtAddCallback(me->start_btn, XmNactivateCallback,
                  StartCB, (XtPointer)k);
    MotifKnobs_HookButton3(k, me->start_btn);

    me->stop_btn  = XtVaCreateManagedWidget("push_i",
                                            xmPushButtonWidgetClass,
                                            CNCRTZE(k->w),
                                            XmNtraversalOn, k->is_rw,
                                            XmNlabelString, s = XmStringCreateLtoR("Stop",  xh_charset),
                                            NULL);
    XmStringFree(s);
    XtAddCallback(me->stop_btn,  XmNactivateCallback,
                  StopCB,  (XtPointer)k);
    MotifKnobs_HookButton3(k, me->stop_btn);
    attachleft(me->stop_btn, me->start_btn, 0);

    return 0;
}


dataknob_knob_vmt_t motifknobs_cda_scenario_vmt =
{
    {DATAKNOB_KNOB, "scenario",
        sizeof(cda_scenario_privrec_t), text2cda_scenarioopts,
        0,
        ScenarioCreate_m, MotifKnobs_CommonDestroy_m,
        MotifKnobs_NoColorize_m, NULL},
    NULL, NULL
};
