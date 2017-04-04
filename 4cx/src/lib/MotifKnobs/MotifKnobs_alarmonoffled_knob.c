#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/ToggleB.h>

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_alarmonoffled_knob.h"


/* OpenMotif < 2.2.3 has "ONE_OF_MANY_NNN" behaviour bug (#1188/#1154) */
#define MOTIF_ONE_OF_MANY_NNN_BUG \
    ((XmVERSION*1000000 + XmREVISION * 1000 + XmUPDATE_LEVEL) < 20002003)


static inline int IsLit(double curv)
{
    return (((int)(curv)) & CX_VALUE_LIT_MASK) != 0;
}


enum
{
    SHAPE_DEFAULT = 0,
    SHAPE_CIRCLE,
    SHAPE_DIAMOND
};

enum
{
    LED_DEFAULT = -1,
    LED_GREEN   = XH_COLOR_JUST_GREEN,
    LED_RED     = XH_COLOR_JUST_RED,
    LED_BLUE    = XH_COLOR_JUST_BLUE,
};

typedef struct
{
    int       color;
    int       offcol;
    int       shape;
    int       panel;
    int       nomargins;
    char     *victim_name;
    DataKnob  victim_k;
    int       cur_set;
} alarmonoffled_privrec_t;

static psp_lkp_t shapes_lkp[] =
{
    {"default", SHAPE_DEFAULT},
    {"circle",  SHAPE_CIRCLE},
    {"diamond", SHAPE_DIAMOND},
    {NULL, 0}
};

static psp_paramdescr_t text2alarmonoffledopts[] =
{
    PSP_P_LOOKUP ("color",     alarmonoffled_privrec_t, color,       LED_DEFAULT,   MotifKnobs_colors_lkp),
    PSP_P_LOOKUP ("offcol",    alarmonoffled_privrec_t, offcol,      LED_DEFAULT,   MotifKnobs_colors_lkp),
    PSP_P_LOOKUP ("shape",     alarmonoffled_privrec_t, shape,       SHAPE_DEFAULT, shapes_lkp),
    PSP_P_FLAG   ("panel",     alarmonoffled_privrec_t, panel,       1,    0),
    PSP_P_FLAG   ("nomargins", alarmonoffled_privrec_t, nomargins,   1,    0),
    PSP_P_MSTRING("victim",    alarmonoffled_privrec_t, victim_name, NULL, 100),
    PSP_P_END()
};


static void BlockButton2Handler(Widget     w        __attribute__((unused)),
                                XtPointer  closure  __attribute__((unused)),
                                XEvent    *event,
                                Boolean   *continue_to_dispatch)
{
  XButtonEvent *ev  = (XButtonEvent *)event;
  
    if (ev->type == ButtonPress  &&  ev->button == Button2)
        *continue_to_dispatch = False;
}

static inline void SetVictimState(alarmonoffled_privrec_t *me, int state)
{
    if (me->victim_k != NULL  &&  me->victim_k->w != NULL)
        XtSetSensitive(CNCRTZE(me->victim_k->w), state);
}

static void ChangeCB(Widget     w,
                     XtPointer  closure,
                     XtPointer  call_data)
{
  DataKnob                      k = (DataKnob)closure;
  alarmonoffled_privrec_t      *me = (alarmonoffled_privrec_t *)k->privptr;
  XmToggleButtonCallbackStruct *info = (XmToggleButtonCallbackStruct *) call_data;
  
    if (k->is_rw)
    {
        XmToggleButtonSetState(w, IsLit(k->u.k.curv), False);
        /* ^^^ This extra line appeared somewhen between 2004-07-14 and 2004-10-26; its purpose a bit unknown, but, probably is to overcome Motif bugs #1154 and/or #1188 */

        me->cur_set = /*info->set*/-1;/*!!! There was smth buggy/unfinished with this cur_set... */
        MotifKnobs_SetControlValue(k, info->set == XmUNSET? 0 : CX_VALUE_LIT_MASK, 1);
        SetVictimState(me, info->set == XmUNSET? 0 : 1);
    }
    else
    {
        /* Restore correct value after user's click */
        XmToggleButtonSetState(w, IsLit(k->u.k.curv), False);
        /* And acknowledge alarm, if relevant */
        ack_knob_alarm(k);
    }
}

#if MOTIF_ONE_OF_MANY_NNN_BUG
static void DisarmCB(Widget     w          __attribute__((unused)),
                     XtPointer  closure,
                     XtPointer  call_data  __attribute__((unused)))
{
  DataKnob       k = (DataKnob)closure;

    ack_knob_alarm(k);
}
#endif



static int CreateAlarmonoffled(DataKnob k, CxWidget parent,
                               int shape, int color)
{
  alarmonoffled_privrec_t *me = (alarmonoffled_privrec_t *)k->privptr;
  Widget                   w;
  XmString                 s;

  char                    *ls;
  int                      no_label;
  Dimension                isize;
  Dimension                mt;
  Dimension                mb;
  Dimension                lh;
  XmFontList               lfl;

  Dimension                t;
  
  int                      n;

    k->behaviour |= DATAKNOB_B_IS_LIGHT | DATAKNOB_B_STEP_FXD;
  
    me->cur_set = -1;

    no_label = !get_knob_label(k, &ls);
    if (no_label) ls = " ";
    
    w = XtVaCreateManagedWidget(k->is_rw? "onoff_i" : "onoff_o",
                                xmToggleButtonWidgetClass,
                                CNCRTZE(parent),
                                XmNlabelString, s = XmStringCreateLtoR(ls, xh_charset),
                                XmNtraversalOn, k->is_rw,
                                XmNset,         XmINDETERMINATE, // for value to be neither SET nor UNSET, so that it *always* changes on initial read
                                NULL);
    XmStringFree(s);
    k->w = ABSTRZE(w);

    if (no_label)
    {
        /* Temporary countermeasure against Motif Bug#1117 */
        XtAddEventHandler(w, ButtonPressMask, False, BlockButton2Handler, NULL);

        /* Size voodoism... */
        XtVaGetValues(w,
                      XmNindicatorSize, &isize,
                      XmNmarginTop,     &mt,
                      XmNmarginBottom,  &mb,
                      XmNfontList,      &lfl,
                      NULL);

        s  = XmStringCreate(" ", xh_charset);
        lh = XmStringHeight(lfl, s);
        XmStringFree(s);
        mt += lh / 2;
        mb += lh - lh / 2;
        
        XtVaSetValues(w, XmNindicatorSize, 1, NULL);
        XtVaSetValues(w,
                      XmNlabelString,   s = XmStringCreate("", xh_charset),
                      XmNindicatorSize, isize,
                      XmNmarginTop,     mt,
                      XmNmarginBottom,  mb,
                      XmNspacing,       0,
                      NULL);
        XmStringFree(s);
        if (me->nomargins  && 1)
        {
            XtVaSetValues(w,
                          XmNmarginLeft,   isize-2,
                          XmNmarginRight,  0,
                          XmNmarginTop,    isize-2,
                          XmNmarginBottom, 0,
                          XmNmarginHeight, 0,
                          XmNmarginWidth,  0,
                          XmNhighlightThickness, 0,
                          NULL);
        }
    }

    /* Resolve the victim reference, if any */
    if (me->victim_name != NULL  &&  me->victim_name[0] != '\0'  &&
        k->uplink  != NULL)
    {
        for (n = 0,                        me->victim_k =  NULL;
             n < k->uplink->u.c.count  &&  me->victim_k == NULL;
             n++)
            if (k->uplink->u.c.content[n].ident != NULL  &&
                strcasecmp(me->victim_name,
                           k->uplink->u.c.content[n].ident) == 0)
                me->victim_k = k->uplink->u.c.content + n;

        if (me->victim_k == NULL)
            fprintf(stderr, "%s(%s/\"%s\"): victim=\"%s\" not found\n",
                    __FUNCTION__, k->ident, k->label, me->victim_name);
    }

    /* Tune the appearance */
    /* a. Color */
    if (me->color == LED_DEFAULT) me->color = color;
    if (me->color != LED_DEFAULT)
        XtVaSetValues(w, XmNselectColor, XhGetColor(me->color), NULL);
    /* a.2. Off-color */
    if (me->offcol != LED_DEFAULT)
        XtVaSetValues(w, XmNunselectColor, XhGetColor(me->offcol), NULL);
    /* b. Shape */
    if (me->shape != SHAPE_DEFAULT) shape = me->shape;
    if (me->panel)
    {
        XtVaGetValues(w, XmNmarginHeight, &t, NULL);
        XtVaSetValues(w,
                      XmNfillOnSelect,    True,
                      XmNindicatorOn,     XmINDICATOR_NONE,
                      XmNmarginHeight,    t - 1,
                      XmNshadowThickness, 1,
                      XmNalignment,       XmALIGNMENT_CENTER,
                      NULL);
    }
    else
    {
        if (shape == SHAPE_CIRCLE)
            XtVaSetValues(w, XmNindicatorType, XmONE_OF_MANY_ROUND,   NULL);
        if (shape == SHAPE_DIAMOND)
            XtVaSetValues(w, XmNindicatorType, XmONE_OF_MANY_DIAMOND, NULL);
    }

    /* Now, the callbacks... */
    XtAddCallback(w, XmNvalueChangedCallback, ChangeCB, (XtPointer)k);
#if MOTIF_ONE_OF_MANY_NNN_BUG
    XtAddCallback(w, XmNdisarmCallback,       DisarmCB, (XtPointer)k);
#endif
    
    MotifKnobs_HookButton3 (k, CNCRTZE(k->w));
    MotifKnobs_DecorateKnob(k);
    
    return 0;
}

static int CreateAlarmKnob(DataKnob k, CxWidget parent)
{
    k->behaviour |= DATAKNOB_B_IS_ALARM;
    return CreateAlarmonoffled(k, parent, SHAPE_CIRCLE,  LED_RED);
}

static int CreateOnoffKnob(DataKnob k, CxWidget parent)
{
    return CreateAlarmonoffled(k, parent, SHAPE_DEFAULT, LED_DEFAULT);
}

static int CreateLedKnob(DataKnob k, CxWidget parent)
{
    return CreateAlarmonoffled(k, parent, SHAPE_CIRCLE,  LED_GREEN);
}

static int CreateRadiobtnKnob(DataKnob k, CxWidget parent)
{
    return CreateAlarmonoffled(k, parent, SHAPE_DIAMOND, LED_BLUE);
}

static void AlarmonoffledSetValue_m(DataKnob k, double v)
{
  alarmonoffled_privrec_t *me         = (alarmonoffled_privrec_t *)k->privptr;
  unsigned char            is_set     = IsLit(v) ? XmSET : XmUNSET;
  Boolean                  is_enabled = (((int)v) & CX_VALUE_DISABLED_MASK) == 0  &&  k->is_rw;

    if (is_set != me->cur_set)
    {
        XmToggleButtonSetState(CNCRTZE(k->w), is_set, False);
        me->cur_set = is_set;

        SetVictimState(me, IsLit(v));
    }

    if (k->is_rw  &&  XtIsSensitive(CNCRTZE(k->w)) != is_enabled)
        XtSetSensitive(CNCRTZE(k->w), is_enabled);
}

dataknob_knob_vmt_t motifknobs_alarm_vmt =
{
    {DATAKNOB_KNOB, "alarm",
        sizeof(alarmonoffled_privrec_t), text2alarmonoffledopts,
        0,
        CreateAlarmKnob, MotifKnobs_CommonDestroy_m, 
        MotifKnobs_CommonColorize_m, NULL},
    AlarmonoffledSetValue_m, NULL
};

dataknob_knob_vmt_t motifknobs_onoff_vmt =
{
    {DATAKNOB_KNOB, "onoff",
        sizeof(alarmonoffled_privrec_t), text2alarmonoffledopts,
        0,
        CreateOnoffKnob, MotifKnobs_CommonDestroy_m,
        MotifKnobs_CommonColorize_m, NULL},
    AlarmonoffledSetValue_m, NULL
};

dataknob_knob_vmt_t motifknobs_led_vmt =
{
    {DATAKNOB_KNOB, "led",
        sizeof(alarmonoffled_privrec_t), text2alarmonoffledopts,
        0,
        CreateLedKnob, MotifKnobs_CommonDestroy_m,
        MotifKnobs_CommonColorize_m, NULL},
    AlarmonoffledSetValue_m, NULL
};

dataknob_knob_vmt_t motifknobs_radiobtn_vmt =
{
    {DATAKNOB_KNOB, "radiobtn",
        sizeof(alarmonoffled_privrec_t), text2alarmonoffledopts,
        0,
        CreateRadiobtnKnob, MotifKnobs_CommonDestroy_m,
        MotifKnobs_CommonColorize_m, NULL},
    AlarmonoffledSetValue_m, NULL
};
