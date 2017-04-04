#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/Text.h>
#include <Xm/ToggleB.h>
#include "IncDecB.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_text_knob.h"


typedef struct
{
    int     has_label;
    int     is_gpbl;
    int     no_incdec;
    int     no_units;
    int     is_compact;
    
    Widget  label;
    Widget  grpmrk;
    Widget  text;
    Widget  incdec;
    Widget  units;

    int     defcols_obtained;
    Pixel   label_deffg;
    Pixel   label_defbg;
    Pixel   grpmrk_deffg;
    Pixel   grpmrk_defbg;
    Pixel   text_deffg;
    Pixel   text_defbg;
    Pixel   incdec_deffg;
    Pixel   incdec_defbg;
    Pixel   units_deffg;
    Pixel   units_defbg;
} text_knob_privrec_t;

static psp_paramdescr_t text2textknobopts[] =
{
    PSP_P_FLAG("withlabel", text_knob_privrec_t, has_label,  1, 0),
    PSP_P_FLAG("nolabel",   text_knob_privrec_t, has_label,  0, 1),
    
    PSP_P_FLAG("noincdec",  text_knob_privrec_t, no_incdec,  1, 0),
    PSP_P_FLAG("withunits", text_knob_privrec_t, no_units,   0, 1),
    PSP_P_FLAG("nounits",   text_knob_privrec_t, no_units,   1, 0),
    
    PSP_P_FLAG("groupable", text_knob_privrec_t, is_gpbl,    1, 0),
    
    PSP_P_FLAG("compact",   text_knob_privrec_t, is_compact, 1, 0),

    PSP_P_END()
};

static void MarkChangeCB(Widget     w  __attribute__((unused)),
                         XtPointer  closure,
                         XtPointer  call_data)
{
  DataKnob                      k    = (DataKnob)                       closure;
  XmToggleButtonCallbackStruct *info = (XmToggleButtonCallbackStruct *) call_data;

    k->u.k.is_ingroup = info->set;
}

static void BlockButton2Handler(Widget     w        __attribute__((unused)),
                                XtPointer  closure  __attribute__((unused)),
                                XEvent    *event,
                                Boolean   *continue_to_dispatch)
{
  XButtonEvent *ev  = (XButtonEvent *)event;
  
    if (ev->type == ButtonPress  &&  ev->button == Button2)
        *continue_to_dispatch = False;
}


static int CreateTextKnob(DataKnob k, CxWidget parent)
{
  text_knob_privrec_t *me = (text_knob_privrec_t *)k->privptr;

  char                *ls;

  Widget               top;
  Widget               left;
  XmString             s;
  
  Dimension            isize;
  Dimension            mt;
  Dimension            mb;
  Dimension            lh;
  XmFontList           lfl;

    /* Manage flags */
    if (!k->is_rw)
    {
        me->is_gpbl   = 0;
        me->no_incdec = 1;
        me->no_units  = 1;
    }

    if (me->is_gpbl) k->behaviour |= DATAKNOB_B_IS_GROUPABLE;
    if ((k->behaviour & DATAKNOB_B_IS_GROUPABLE) != 0  &&  k->is_rw)
        me->is_gpbl   = 1;

    if (k->u.k.units == NULL  ||  k->u.k.units[0] == '\0')
        me->no_units = 1;

    /* Container form... */
    k->w =
        ABSTRZE(XtVaCreateManagedWidget("textForm",
                                        xmFormWidgetClass,
                                        CNCRTZE(parent),
                                        XmNshadowThickness, 0,
                                        NULL));
    MotifKnobs_HookButton3(k, CNCRTZE(k->w));
    
    top = left = NULL;

    /* A label */
    if (me->has_label  &&  get_knob_label(k, &ls))
    {
        me->label = XtVaCreateManagedWidget("rowlabel",
                                            xmLabelWidgetClass,
                                            CNCRTZE(k->w),
                                            XmNlabelString,  s = XmStringCreateLtoR(ls, xh_charset),
                                            XmNalignment,    XmALIGNMENT_BEGINNING,
                                            NULL);
        XmStringFree(s);
        MotifKnobs_HookButton3(k, me->label);

        if (me->is_compact) top  = me->label;
        else                left = me->label;
    }

    /* "Group" mark */
    if (me->is_gpbl)
    {
        k->behaviour |= DATAKNOB_B_IS_GROUPABLE;
        
        /* Create a non-traversable checkbox */
        me->grpmrk = XtVaCreateManagedWidget("onoff_i",
                                             xmToggleButtonWidgetClass,
                                             CNCRTZE(k->w),
                                             XmNlabelString, s = XmStringCreateLtoR(" ", xh_charset),
                                             XmNtraversalOn, False,
                                             XmNvalue,       k->u.k.is_ingroup,
                                             XmNselectColor, XhGetColor(XH_COLOR_INGROUP),
                                             XmNdetailShadowThickness, 1,
                                             NULL);
        XmStringFree(s);
        if (top  != NULL) attachtop (me->grpmrk, top,  0);
        if (left != NULL) attachleft(me->grpmrk, left, 0);
        XtAddCallback(me->grpmrk, XmNvalueChangedCallback,
                      MarkChangeCB, (XtPointer)k);
        XtAddEventHandler(me->grpmrk, ButtonPressMask, False, BlockButton2Handler, NULL); // Temporary countermeasure against Motif Bug#1117

        /* Do all the voodoism with its height */
        XtVaGetValues(me->grpmrk,
                      XmNindicatorSize, &isize,
                      XmNmarginTop,     &mt,
                      XmNmarginBottom,  &mb,
                      XmNfontList,      &lfl,
                      NULL);
        
        s = XmStringCreate(" ", xh_charset);
        lh = XmStringHeight(lfl, s);
        XmStringFree(s);
        mt += lh / 2;
        mb += lh - lh / 2;
        
        XtVaSetValues(me->grpmrk, XmNindicatorSize, 1, NULL);
        XtVaSetValues(me->grpmrk,
                      XmNlabelString,   s = XmStringCreate("", xh_charset),
                      XmNindicatorSize, isize,
                      XmNmarginTop,     mt,
                      XmNmarginBottom,  mb,
                      XmNspacing,       0,
                      NULL);
        XmStringFree(s);

        MotifKnobs_HookButton3(k, me->grpmrk);

        left = me->grpmrk;
    }
    
    /* Text... */
    me->text = (k->is_rw? MotifKnobs_CreateTextInput
                        : MotifKnobs_CreateTextValue)(k, CNCRTZE(k->w));
    if (top  != NULL) attachtop (me->text, top,  0);
    if (left != NULL) attachleft(me->text, left, 0);
    left = me->text;

    MotifKnobs_HookButton3(k, me->text);
    
    /* Increment/decrement arrows */
    if (!(me->no_incdec))
    {
        me->incdec = XtVaCreateManagedWidget("textIncDec",
                                             xmIncDecButtonWidgetClass,
                                             CNCRTZE(k->w),
                                             XmNclientWidget,     me->text,
                                             XmNleftAttachment,   XmATTACH_WIDGET,
                                             XmNleftOffset,       0,
                                             XmNleftWidget,       me->text,
                                             XmNtopAttachment,    XmATTACH_OPPOSITE_WIDGET,
                                             XmNtopWidget,        me->text,
                                             XmNtopOffset,        0,
                                             NULL);

        left = me->incdec;
    }
    
    /* Optional "units" post-label for rw knobs */
    if (!(me->no_units))
    {
        me->units = XtVaCreateManagedWidget("rowlabel", xmLabelWidgetClass,
                                            CNCRTZE(k->w),
                                            XmNlabelString,  s = XmStringCreateLtoR(k->u.k.units, xh_charset),
                                            XmNalignment,    XmALIGNMENT_BEGINNING,
                                            XmNleftAttachment,   XmATTACH_WIDGET,
                                            XmNleftOffset,       0,
                                            XmNleftWidget,       left,
                                            XmNtopAttachment,    XmATTACH_OPPOSITE_WIDGET,
                                            XmNtopWidget,        left,
                                            XmNtopOffset,        0,
                                            NULL);
        XmStringFree(s);

        MotifKnobs_HookButton3(k, me->units);

        left = me->units;
    }

    MotifKnobs_TuneTextKnob(k, me->text, 0);
    MotifKnobs_DecorateKnob(k);

    return 0;
}

static int CreateNormalText(DataKnob k, CxWidget parent)
{
    return CreateTextKnob(k, parent);
}

static int CreateDefKnob   (DataKnob k, CxWidget parent)
{
    return CreateTextKnob(k, parent);
}

static void TextSetValue_m(DataKnob k, double v)
{
  text_knob_privrec_t *me = (text_knob_privrec_t *)k->privptr;

    MotifKnobs_SetTextString(k, me->text, v);
}

static void TextKnobColorize_m(DataKnob k, knobstate_t newstate)
{
  text_knob_privrec_t *me = (text_knob_privrec_t *)k->privptr;
  Pixel                fg;
  Pixel                bg;
  
    if (!me->defcols_obtained)
    {
        if (me->label != NULL)
            XtVaGetValues(me->label,
                          XmNforeground, &(me->label_deffg),
                          XmNbackground, &(me->label_defbg),
                          NULL);
        if (me->grpmrk != NULL)
            XtVaGetValues(me->grpmrk,
                          XmNforeground, &(me->grpmrk_deffg),
                          XmNbackground, &(me->grpmrk_defbg),
                          NULL);
        XtVaGetValues(me->text,
                      XmNforeground, &(me->text_deffg),
                      XmNbackground, &(me->text_defbg),
                      NULL);
        if (me->incdec != NULL)
            XtVaGetValues(me->incdec,
                          XmNforeground, &(me->incdec_deffg),
                          XmNbackground, &(me->incdec_defbg),
                          NULL);
        if (me->units != NULL)
            XtVaGetValues(me->units,
                          XmNforeground, &(me->units_deffg),
                          XmNbackground, &(me->units_defbg),
                          NULL);
        
        me->defcols_obtained = 1;
    }

    MotifKnobs_ChooseColors(k->colz_style, newstate,
                            k->deffg, k->defbg, &fg, &bg);
    XtVaSetValues(CNCRTZE(k->w),
                  XmNforeground, fg,
                  XmNbackground, bg,
                  NULL);
    if (me->label != NULL)
    {
        MotifKnobs_ChooseColors(k->colz_style, newstate,
                                me->label_deffg, me->label_defbg, &fg, &bg);
        XtVaSetValues(me->label,
                      XmNforeground, fg,
                      XmNbackground, bg,
                      NULL);
    }
    if (me->grpmrk != NULL)
    {
        MotifKnobs_ChooseColors(k->colz_style, newstate,
                                me->grpmrk_deffg, me->grpmrk_defbg, &fg, &bg);
        XtVaSetValues(me->grpmrk,
                      XmNforeground, fg,
                      XmNbackground, bg,
                      NULL);
    }
    MotifKnobs_ChooseColors(k->colz_style, newstate,
                            me->text_deffg, me->text_defbg, &fg, &bg);
    XtVaSetValues(me->text,
                  XmNforeground, fg,
                  XmNbackground, bg,
                  NULL);
    if (me->incdec != NULL)
    {
        MotifKnobs_ChooseColors(k->colz_style, newstate,
                                me->incdec_deffg, me->incdec_defbg, &fg, &bg);
        XtVaSetValues(me->incdec,
                      XmNforeground, fg,
                      XmNbackground, bg,
                      NULL);
    }
    if (me->units != NULL)
    {
        MotifKnobs_ChooseColors(k->colz_style, newstate,
                                me->units_deffg, me->units_defbg, &fg, &bg);
        XtVaSetValues(me->units,
                      XmNforeground, fg,
                      XmNbackground, bg,
                      NULL);
    }
}

static void TextPropsChg_m(DataKnob k, DataKnob old_k  __attribute__((unused)))
{
  text_knob_privrec_t *me = (text_knob_privrec_t *)k->privptr;

    if (k->behaviour & DATAKNOB_B_IS_GROUPABLE  &&
        me->grpmrk != NULL                      &&
        XmToggleButtonGetState(me->grpmrk) != (k->u.k.is_ingroup != 0))
        XmToggleButtonSetState(me->grpmrk,     k->u.k.is_ingroup != 0, False);
}


dataknob_knob_vmt_t motifknobs_text_knob_vmt =
{
    {DATAKNOB_KNOB, "text",
        sizeof(text_knob_privrec_t), text2textknobopts,
        0,
        CreateNormalText, MotifKnobs_CommonDestroy_m,
        TextKnobColorize_m, NULL},
    TextSetValue_m, TextPropsChg_m
};

dataknob_knob_vmt_t motifknobs_def_knob_vmt =
{
    {DATAKNOB_KNOB, DEFAULT_KNOB_LOOK_NAME,
        sizeof(text_knob_privrec_t), NULL,
        0,
        CreateDefKnob, MotifKnobs_CommonDestroy_m,
        TextKnobColorize_m, NULL},
    TextSetValue_m, TextPropsChg_m
};
