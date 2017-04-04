#include <stdlib.h>
#include <sys/time.h>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>

#define XK_MISCELLANY
#define XK_LATIN1
#include <X11/keysymdef.h>

#include "misc_macros.h"
#include "misclib.h"

#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "Xh.h"
#include "Xh_globals.h"
// For TMP: v2-style button style support
#include "Xh_fallbacks.h"
#include "findfilein.h"
#include <limits.h> /* Is PATH_MAX here? */


psp_lkp_t MotifKnobs_colors_lkp[] =
{
    {"default", -1},

    {"red",     XH_COLOR_JUST_RED},
    {"orange",  XH_COLOR_JUST_ORANGE},
    {"yellow",  XH_COLOR_JUST_YELLOW},
    {"green",   XH_COLOR_JUST_GREEN},
    {"blue",    XH_COLOR_JUST_BLUE},
    {"indigo",  XH_COLOR_JUST_INDIGO},
    {"violet",  XH_COLOR_JUST_VIOLET},
    
    {"black",   XH_COLOR_JUST_BLACK},
    {"white",   XH_COLOR_JUST_WHITE},
    {"gray",    XH_COLOR_JUST_GRAY},

    {"amber",   XH_COLOR_JUST_AMBER},
    {"brown",   XH_COLOR_JUST_BROWN},

    {"armed",   XH_COLOR_BG_TOOL_ARMED}, /*!!! #80ccf0 - "turquoise"? "cyan"? */

    {NULL, 0}
};

psp_lkp_t MotifKnobs_sizes_lkp [] =
{
    {"default", -1},
    {"tiny",     8},
    {"small",   10},
    {"normal",  12},
    {"large",   18},
    {"huge",    24},
    {"giant",   34},
    {NULL, 0}
};

psp_paramdescr_t text2_MotifKnobs_containeropts[] =
{
    PSP_P_FLAG   ("titleattop",    MotifKnobs_containeropts_t, title_side, MOTIFKNOBS_TITLE_SIDE_TOP,    1),
    PSP_P_FLAG   ("titleatbottom", MotifKnobs_containeropts_t, title_side, MOTIFKNOBS_TITLE_SIDE_BOTTOM, 0),
    PSP_P_FLAG   ("titleatleft",   MotifKnobs_containeropts_t, title_side, MOTIFKNOBS_TITLE_SIDE_LEFT,   0),
    PSP_P_FLAG   ("titleatright",  MotifKnobs_containeropts_t, title_side, MOTIFKNOBS_TITLE_SIDE_RIGHT,  0),

    PSP_P_FLAG   ("notitle",       MotifKnobs_containeropts_t, notitle,    1,    0),
    PSP_P_FLAG   ("nohline",       MotifKnobs_containeropts_t, nohline,    1,    0),
    PSP_P_FLAG   ("noshadow",      MotifKnobs_containeropts_t, noshadow,   1,    0),
    PSP_P_FLAG   ("nofold",        MotifKnobs_containeropts_t, nofold,     1,    0),
    PSP_P_FLAG   ("folded",        MotifKnobs_containeropts_t, folded,     1,    0),
    PSP_P_FLAG   ("fold_h",        MotifKnobs_containeropts_t, fold_h,     1,    0),
    PSP_P_FLAG   ("fold_v",        MotifKnobs_containeropts_t, fold_h,     0,    1),
    PSP_P_FLAG   ("sepfold",       MotifKnobs_containeropts_t, sepfold,    1,    0),
    PSP_P_END()
};


enum
{
    STYLE_SIZE_CHG_base  = -10,
    STYLE_SIZE_SMALLER   = STYLE_SIZE_CHG_base - 1,
    STYLE_SIZE_LARGER    = STYLE_SIZE_CHG_base + 1,

    STYLE_SIZE_NO_CHANGE = 0,
    STYLE_SIZE_TINY      = 1,  STYLE_SIZE_min = STYLE_SIZE_TINY,
    STYLE_SIZE_SMALL     = 2,
    STYLE_SIZE_NORMAL    = 3,  STYLE_SIZE_def = STYLE_SIZE_NORMAL,
    STYLE_SIZE_LARGE     = 4,
    STYLE_SIZE_HUGE      = 5,
    STYLE_SIZE_GIANT     = 6,  STYLE_SIZE_max = STYLE_SIZE_GIANT,
};
static psp_lkp_t style_sizes_lkp[] =
{
    {"tiny",    STYLE_SIZE_TINY},
    {"small",   STYLE_SIZE_SMALL},
    {"normal",  STYLE_SIZE_NORMAL},
    {"large",   STYLE_SIZE_LARGE},
    {"huge",    STYLE_SIZE_HUGE},
    {"giant",   STYLE_SIZE_GIANT},
    {"smaller", STYLE_SIZE_CHG_base-1},
    {"larger",  STYLE_SIZE_CHG_base+1},
    {"-1",      STYLE_SIZE_CHG_base-1},
    {"+1",      STYLE_SIZE_CHG_base+1},
    {"-2",      STYLE_SIZE_CHG_base-2},
    {"+2",      STYLE_SIZE_CHG_base+2},
};

/*** Standard/default knobs' methods ********************************/

void MotifKnobs_CommonDestroy_m (DataKnob k)
{
    if (k->w != NULL) XtDestroyWidget(CNCRTZE(k->w));
    k->w = NULL;
}

void MotifKnobs_CommonColorize_m(DataKnob k, knobstate_t newstate)
{
  Pixel           fg;
  Pixel           bg;
  
    MotifKnobs_ChooseKnobCs(k, newstate, &fg, &bg);
    XtVaSetValues(CNCRTZE(k->w),
                  XmNforeground, fg,
                  XmNbackground, bg,
                  NULL);
}

void MotifKnobs_NoColorize_m    (DataKnob k, knobstate_t newstate)
{
    /* This method is intentionally empty,
       to prevent any attempts to change colors */
}


static void DisplayAlarm(DataKnob k, int onoff)
{
  CxPixel     col = onoff? XhGetColor(XH_COLOR_JUST_RED) : k->defbg;
  WidgetList  children;
  Cardinal    numChildren;
  Cardinal    i;

    if (!XtIsComposite(CNCRTZE(k->w))) return;

    XtVaSetValues(CNCRTZE(k->w), XmNbackground, col, NULL);

    XtVaGetValues(CNCRTZE(k->w),
                  XmNchildren,    &children,
                  XmNnumChildren, &numChildren,
                  NULL);

    for (i = 0;  i < numChildren;  i++)
        XtVaSetValues(children[i], XmNbackground, col, NULL);
}

static void CycleAlarm(DataKnob k)
{
  struct timeval timenow;
  cx_time_t      curtime;
  cx_time_t      curtime_1;

    gettimeofday(&timenow, NULL);
    curtime.sec  = timenow.tv_sec;
    curtime.nsec = timenow.tv_usec * 1000;
    curtime_1 = curtime; curtime_1.sec--;
//fprintf(stderr, "%d %p %ld %ld\n", CX_TIME_IS_AFTER(curtime_1, k->u.c.alarm_prev_time), k, k->u.c.alarm_prev_time.sec, k->u.c.alarm_prev_time.nsec);
    if (!CX_TIME_IS_AFTER(curtime_1, k->u.c.alarm_prev_time)) return;
    k->u.c.alarm_prev_time = curtime;

    if (!k->u.c.alarm_on  ||  k->u.c.alarm_acked) return;

    XBell(XtDisplay(CNCRTZE(k->w)), 100);
    k->u.c.alarm_flipflop = ! k->u.c.alarm_flipflop;
    if (k->u.c.alarm_flipflop)
        DisplayAlarm(k, 1);
    else
        DisplayAlarm(k, 0);
}

void MotifKnobs_CommonShowAlarm_m(DataKnob k, int onoff)
{
    if (k->type != DATAKNOB_CONT  &&
        k->type != DATAKNOB_GRPG) return;

    /*!!! Walk up while upper's ->ShowAlarm==MotifKnobs_CommonShowAlarm_m */
    while (k->uplink          != NULL  &&
           k->uplink->vmtlink != NULL  &&
           ((dataknob_cont_vmt_t*)(k->uplink->vmtlink))->
               ShowAlarm == MotifKnobs_CommonShowAlarm_m)
        k = k->uplink;

    if (onoff)
    {
        if (k->u.c.alarm_on++ == 0) k->u.c.alarm_flipflop = 0;
        k->u.c.alarm_acked = 0;
    }
    else
    {
        if (--k->u.c.alarm_on == 0) DisplayAlarm(k, 0);
    }
}

void MotifKnobs_CommonAckAlarm_m (DataKnob k)
{
    if (k->type != DATAKNOB_CONT  &&
        k->type != DATAKNOB_GRPG) return;

    /*!!! Walk up while emlink->AckAlarm==MotifKnobs_CommonAckAlarm_m */
    while (k->uplink          != NULL  &&
           k->uplink->vmtlink != NULL  &&
           ((dataknob_cont_vmt_t*)(k->uplink->vmtlink))->
               AckAlarm == MotifKnobs_CommonAckAlarm_m)
        k = k->uplink;

    if (k->u.c.alarm_on == 0  ||  k->u.c.alarm_acked) return;
    k->u.c.alarm_acked = 1;
    DisplayAlarm(k, 0);
}

void MotifKnobs_CommonAlarmNewData_m(DataKnob k, int synthetic)
{
    if (synthetic) return;

    CycleAlarm(k);
}

/*** Color management ***********************************************/

void MotifKnobs_ChooseColors(int colz_style, knobstate_t newstate,
                             CxPixel  deffg,  CxPixel  defbg,
                             Pixel   *newfg,  Pixel   *newbg)
{
  Pixel  fg = deffg;
  Pixel  bg = defbg;
  
    switch (colz_style)
    {
        case DATAKNOB_COLZ_HILITED: fg = XhGetColor(XH_COLOR_FG_HILITED);  break;
        case DATAKNOB_COLZ_VIC:     bg = XhGetColor(XH_COLOR_BG_VIC);      break;
        case DATAKNOB_COLZ_DIM:     fg = XhGetColor(XH_COLOR_FG_DIM);      break;
        case DATAKNOB_COLZ_HEADING: fg = XhGetColor(XH_COLOR_FG_HEADING);
                                    bg = XhGetColor(XH_COLOR_BG_HEADING);  break;
        default: ;
    }
    switch (newstate)
    {
        case KNOBSTATE_JUSTCREATED:
            bg = XhGetColor(XH_COLOR_BG_JUSTCREATED);
            break;
        
        case KNOBSTATE_YELLOW:
            bg = XhGetColor((colz_style == DATAKNOB_COLZ_IMPORTANT  ||
                             colz_style == DATAKNOB_COLZ_VIC)? XH_COLOR_BG_IMP_YELLOW
                                                             : XH_COLOR_BG_NORM_YELLOW);
            break;
            
        case KNOBSTATE_RED:
            bg = XhGetColor((colz_style == DATAKNOB_COLZ_IMPORTANT  ||
                             colz_style == DATAKNOB_COLZ_VIC)? XH_COLOR_BG_IMP_RED
                                                             : XH_COLOR_BG_NORM_RED);
            break;

        case KNOBSTATE_WEIRD:
            fg = XhGetColor(XH_COLOR_FG_WEIRD);
            bg = XhGetColor(XH_COLOR_BG_WEIRD);
            break;
            
        case KNOBSTATE_DEFUNCT:
            bg = XhGetColor(XH_COLOR_BG_DEFUNCT);
            break;
            
        case KNOBSTATE_HWERR:
            bg = XhGetColor(XH_COLOR_BG_HWERR);
            break;
            
        case KNOBSTATE_SFERR:
            bg = XhGetColor(XH_COLOR_BG_SFERR);
            break;
            
        case KNOBSTATE_NOTFOUND:
            bg = XhGetColor(XH_COLOR_BG_NOTFOUND);
            break;
            
        case KNOBSTATE_RELAX:
            bg = XhGetColor(XH_COLOR_JUST_GREEN);
            break;

        case KNOBSTATE_OTHEROP:
            bg = XhGetColor(XH_COLOR_BG_OTHEROP);
            break;

        case KNOBSTATE_PRGLYCHG:
            bg = XhGetColor(XH_COLOR_BG_PRGLYCHG);
            break;

        default:;
    }
    
    *newfg = fg;
    *newbg = bg;
}

void MotifKnobs_ChooseKnobCs(DataKnob k,  knobstate_t newstate,
                             Pixel   *newfg,  Pixel   *newbg)
{
    MotifKnobs_ChooseColors(k->colz_style, newstate,
                            k->deffg, k->defbg,
                            newfg, newbg);
}

void MotifKnobs_AllocStateGCs(Widget w, GC table, int norm_idx)
{
}


/*** User interface *************************************************/

void MotifKnobs_Mouse3Handler(Widget     w __attribute__((unused)),
                              XtPointer  closure,
                              XEvent    *event,
                              Boolean   *continue_to_dispatch)
{
  XButtonPressedEvent *ev = (XButtonPressedEvent *) event;
  DataKnob             k  = (DataKnob)              closure;

    if (ev->button != Button3) return;
    *continue_to_dispatch = False;
    
    if      (ev->state & ControlMask)
        show_knob_bigval  (k);
    else if (ev->state & ShiftMask)
        show_knob_histplot(k);
    else
        show_knob_props   (k);
}

static void MotifKnobs_CommonKbdHandler(Widget     w  __attribute__((unused)),
                                        XtPointer  closure,
                                        XEvent    *event,
                                        Boolean   *continue_to_dispatch)
{
  XKeyEvent           *ev = (XKeyEvent *)event;
  DataKnob             k  = (DataKnob)   closure;

  KeySym               ks;
  Modifiers            mr;

    XtTranslateKeycode(XtDisplay(w), ev->keycode, ev->state, &mr, &ks);
    
    if      (ks == XK_space  &&  (ev->state & ControlMask))
        show_knob_bigval  (k);
    else if (ks == XK_space  &&  (ev->state & ShiftMask))
        show_knob_histplot(k);
    else if (
             (
              (ev->state & Mod1Mask)  &&
              (ks == XK_Return  ||  ks == XK_KP_Enter  ||  ks == osfXK_SwitchDirection  ||  ks == osfXK_Activate)
             )
             ||
             (
              (ev->state & ShiftMask)  &&
              (ks == XK_F10  ||  ks == osfXK_Menu  ||  ks == osfXK_MenuBar)
             )
            )
        show_knob_props   (k);
    else
        return;

    *continue_to_dispatch = False;
}

static void MotifKnobs_ChangeHiliteState(DataKnob k, int onoff)
{
    if (onoff) XhShowHilite(k->w);
    else       XhHideHilite(k->w);
}

void MotifKnobs_HookButton3  (DataKnob  k, Widget w)
{
    XtInsertEventHandler(w, ButtonPressMask, False, MotifKnobs_Mouse3Handler,    (XtPointer)k, XtListHead);
    XtInsertEventHandler(w, KeyPressMask,    False, MotifKnobs_CommonKbdHandler, (XtPointer)k, XtListHead);
    set_knob_editstate_hook(MotifKnobs_ChangeHiliteState);
}

void MotifKnobs_DecorateKnob (DataKnob  k)
{
  char                 *tp;

    /* Obtain default colors and colorize */
    XtVaGetValues(CNCRTZE(k->w),
                  XmNforeground, &(k->deffg),
                  XmNbackground, &(k->defbg),
                  NULL);
    if (k->vmtlink != NULL  &&
        k->vmtlink->Colorize != NULL)
        k->vmtlink->Colorize(k, k->curstate);

    /* Attach tooltip */
    if (get_knob_tip(k, &tp))
        XhSetBalloon(k->w, tp);
}

int  MotifKnobs_IsDoubleClick(Widget   w,
                              XEvent  *event,
                              Boolean *continue_to_dispatch)
{
  XButtonEvent *ev  = (XButtonEvent *)event;
  int           ret;

  static Display *prev_d = NULL;
  static Widget   prev_w = NULL;
  static Time     prev_t = 0;

    if (ev->type != ButtonPress  ||  ev->button != Button1) return 0;

    ret =
        XtDisplay(w)      == prev_d  &&
        prev_w            == w       &&
        ev->time - prev_t <= XtGetMultiClickTime(XtDisplay(w));

    prev_d = XtDisplay(w);
    prev_w = w;
    prev_t = ev->time;
    if (ret) *continue_to_dispatch = False;

    return ret;
}


/*** SetControlValue interface **************************************/

void MotifKnobs_SetControlValue(DataKnob k, double v, int fromlocal)
{
    if (set_knob_controlvalue(k, v, fromlocal) > 0)
        XBell(XtDisplay(k->w), 100);
}


/*** Common-textfield interface *************************************/

static void TextSelectionCB(Widget     w         __attribute__((unused)),
                            XtPointer  closure   __attribute__((unused)),
                            XtPointer  call_data)
{
  XmTextVerifyCallbackStruct *info = (XmTextVerifyCallbackStruct *) call_data;
}

static void TextChangeCB(Widget     w,
                         XtPointer  closure,
                         XtPointer  call_data)
{
  DataKnob             k    = (DataKnob)              closure;
  XmAnyCallbackStruct *info = (XmAnyCallbackStruct *) call_data;
  double               v;
  
    /* Do a CANCEL if losing focus */
    if (info->reason != XmCR_ACTIVATE)
    {
        cancel_knob_editing(k);
        return;
    }

    if (MotifKnobs_ExtractDoubleValue(w, &v))
    {
        store_knob_undo_value (k);
        MotifKnobs_SetControlValue(k, v, 1);
    }
}

static void TextUserCB(Widget     w __attribute__((unused)),
                       XtPointer  closure,
                       XtPointer  call_data)
{
  DataKnob                    k    = (DataKnob)                     closure;
  XmTextVerifyCallbackStruct *info = (XmTextVerifyCallbackStruct *) call_data;
    
    if (info->reason == XmCR_MOVING_INSERT_CURSOR ||
        info->event  != NULL)
        user_began_knob_editing(k);
}

static void HandleTextUpDown(Widget        w,
                             DataKnob      k,
                             unsigned int  state,
                             Boolean       up)
{
  int           factor;
  Boolean       immed;
  double        v;
  double        incdec_step;

  int           x;
  DataKnob      victim;
  double        grpcoeff;

    factor = 100;
    if (state & MOD_FINEGRAIN_MASK) factor = 10;
    if (state & MOD_BIGSTEPS_MASK)  factor = 1000;
    immed = (state & MOD_NOSEND_MASK) == 0;

    if (!MotifKnobs_ExtractDoubleValue(w, &v)) return;

    if (k->u.k.num_params > DATAKNOB_PARAM_STEP)
        incdec_step = k->u.k.params[DATAKNOB_PARAM_STEP].value;
    else
        incdec_step = 1.0;
    
    if (up)
        v += (incdec_step * factor) / 100;
    else
        v -= (incdec_step * factor) / 100;
    
    if (immed)
    {
        store_knob_undo_value(k);
        MotifKnobs_SetControlValue(k, v, 1);

        if (k->u.k.is_ingroup  &&  k->uplink != NULL)
            for (x = 0, victim = k->uplink->u.c.content;
                 x < k->uplink->u.c.count;
                 x++,   victim++)
                if (victim != k  &&
                    victim->type == DATAKNOB_KNOB  &&
                    victim->is_rw                  &&
                    victim->u.k.is_ingroup)
                {
                    grpcoeff = 1.0;
                    if (victim->u.k.num_params > DATAKNOB_PARAM_GRPCOEFF)
                        grpcoeff = victim->u.k.params[DATAKNOB_PARAM_GRPCOEFF].value;
                    if (grpcoeff == 0.0) grpcoeff = 1.0;
                    MotifKnobs_SetControlValue(victim,
                                               (
                                                (victim->u.k.userval_valid? victim->u.k.userval : victim->u.k.curv) +
                                                (incdec_step * factor) / 100
                                                * grpcoeff
                                                * (up? +1 : -1)
                                               ),
                                               1);
                }
    }
    else
    {
        user_began_knob_editing(k);
      
        /* Set the value in input line */
        if (((dataknob_knob_vmt_t *)(k->vmtlink)) != NULL  &&
            ((dataknob_knob_vmt_t *)(k->vmtlink))          &&
            ((dataknob_knob_vmt_t *)(k->vmtlink))->SetValue != NULL)
            ((dataknob_knob_vmt_t *)(k->vmtlink))->SetValue(k, v);
    }
}

static void TextKbdHandler(Widget     w,
                           XtPointer  closure,
                           XEvent    *event,
                           Boolean   *continue_to_dispatch)
{
  DataKnob      k  = (DataKnob)    closure;
  XKeyEvent    *ev = (XKeyEvent *) event;

  KeySym        ks;
  Modifiers     mr;

  Boolean       up;

    if (event->type != KeyPress) return;
    XtTranslateKeycode(XtDisplay(w), ev->keycode, ev->state, &mr, &ks);

    /* [Cancel] */
    if ((ks == XK_Escape  ||  ks == osfXK_Cancel)  &&
        (ev->state & (ShiftMask | ControlMask | Mod1Mask)) == 0
        &&  k->usertime != 0)
    {
        cancel_knob_editing(k);
        *continue_to_dispatch = False;
        return;
    }

    /* [Undo] */
    if (
        ((ks == XK_Z  ||  ks == XK_z)  &&  (ev->state & ControlMask) != 0)
        ||
        ((ks == XK_BackSpace  ||  ks == osfXK_BackSpace)  &&  (ev->state & Mod1Mask) != 0)
       )
    {
        perform_knob_undo(k);
        *continue_to_dispatch = False;
        return;
    }
    
    /* [Up] or [Down] */
    if      (ks == XK_Up    ||  ks == XK_KP_Up    ||  ks == osfXK_Up)   up = True;
    else if (ks == XK_Down  ||  ks == XK_KP_Down  ||  ks == osfXK_Down) up = False;
    else return;

    *continue_to_dispatch = False;

    HandleTextUpDown(w, k, ev->state, up);
}

static void TextWheelHandler(Widget     w,
                             XtPointer  closure,
                             XEvent    *event,
                             Boolean   *continue_to_dispatch)
{
  DataKnob             k  = (DataKnob)              closure;
  XButtonPressedEvent *ev = (XButtonPressedEvent *) event;
  Boolean              up;

    if      (ev->button == Button4) up = True;
    else if (ev->button == Button5) up = False;
    else return;

    *continue_to_dispatch = False;

    HandleTextUpDown(w, k, ev->state, up);
}

Widget MotifKnobs_MakeTextWidget (Widget parent, Boolean rw, int columns)
{
  Widget  w;
    
    w = XtVaCreateManagedWidget(rw? "text_i" : "text_o",
                                xmTextWidgetClass,
                                parent,
                                XmNvalue,                 rw? "": "--------",
                                XmNcursorPositionVisible, False,
                                XmNcolumns,               columns,
                                XmNscrollHorizontal,      False,
                                XmNeditMode,              XmSINGLE_LINE_EDIT,
                                XmNeditable,              rw,
                                XmNtraversalOn,           rw,
                                NULL);

    XtAddCallback(w, XmNgainPrimaryCallback, TextSelectionCB, NULL);

    return w;
}

Widget MotifKnobs_CreateTextValue(DataKnob k, Widget parent)
{
  Widget  w;
  int     columns = GetTextColumns(k->u.k.dpyfmt, k->u.k.units);

    w = MotifKnobs_MakeTextWidget(parent, False, columns);
    
    return w;
}

Widget MotifKnobs_CreateTextInput(DataKnob k, Widget parent)
{
  Widget  w;
  int     columns = GetTextColumns(k->u.k.dpyfmt, NULL) + 1;

    w = MotifKnobs_MakeTextWidget(parent, True,  columns);

    XtAddCallback(w, XmNactivateCallback,     TextChangeCB, (XtPointer)k);
    XtAddCallback(w, XmNlosingFocusCallback,  TextChangeCB, (XtPointer)k);
    
    XtAddCallback(w, XmNmodifyVerifyCallback, TextUserCB,   (XtPointer)k);
    XtAddCallback(w, XmNmotionVerifyCallback, TextUserCB,   (XtPointer)k);
    
    MotifKnobs_SetTextCursorCallback(w);
    
    XtAddEventHandler(w, KeyPressMask,    False, TextKbdHandler,   (XtPointer)k);
    XtAddEventHandler(w, ButtonPressMask, False, TextWheelHandler, (XtPointer)k);

    return w;
}

void   MotifKnobs_SetTextString  (DataKnob k, Widget w, double v)
{
  char      buf[1000];
  int       len;
  int       frsize;
  char     *p;
  Boolean   ed = XmTextGetEditable(w);

    len = snprintf(buf, sizeof(buf), k->u.k.dpyfmt, v);
    if (len < 0  ||  (size_t)len > sizeof(buf)-1) buf[len = sizeof(buf)-1] = '\0';
    p = buf + len;
    while (p > buf  &&  *(p - 1) == ' ') *--p = '\0'; /* Trim trailing spaces */
    if (!ed  &&  k->u.k.units != NULL)
    {
        frsize = sizeof(buf)-1 - len;
        if (frsize > 0) snprintf(buf + len, frsize, "%s", k->u.k.units);
    }
    
    p = buf;
    if (ed)
        while (*p == ' ') p++; /* Skip spaces */
    XmTextSetString(w, p);
}

static void TextFocusCursorCB(Widget     w,
                              XtPointer  closure,
                              XtPointer  call_data __attribute__((unused)))
{
    XtVaSetValues(w, XmNcursorPositionVisible, (int)closure, NULL);
}

void   MotifKnobs_SetTextCursorCallback(Widget w)
{
    XtAddCallback(w, XmNfocusCallback,
                  TextFocusCursorCB, (XtPointer)True);
    XtAddCallback(w, XmNlosingFocusCallback,
                  TextFocusCursorCB, (XtPointer)False);
}

int    MotifKnobs_ExtractDoubleValue   (Widget w, double *result)
{
  double  v;
  char   *err;
  char   *text = XmTextGetString(w);
  int     ret;

  char   *p;
  int     minprio;
  char    op;
  int     prio;
  double  v2;

    v = strtod(text, &err);
#if 0
    ret = !(err == text  ||  *err != '\0');
#else
    ret = !(err == text);

    for (p = err, minprio = 1;
         ret != 0;
         p = err)
    {
        while (*p != '\0'  &&  isspace(*p)) p++;
        if (*p == '\0') break;

        err = p;

        op = *p;
        if (op != '+'  &&  op != '-'  &&  op != '*'  &&  op != '/')
        {
            ret = 0;
            goto END_PARSE;
        }

        /* Check priority -- against "a+b*c" (=>"(a+b)*c") */
        prio = (op == '*'  ||  op == '/');
        if (prio > minprio)
        {
            ret = 0;
            goto END_PARSE;
        }
        minprio = prio;

        p++;
        v2 = strtod(p, &err);
        ret = !(err == p);

        if      (!ret)      break;
        else if (op == '+') v += v2;
        else if (op == '-') v -= v2;
        else if (op == '*') v *= v2;
        else if (op == '/') v /= v2;
    }

 END_PARSE:
#endif
    if (ret)
    {
        *result = v;
    }
    else
    {
       XBell(XtDisplay(w), 100);
       XmTextSetInsertionPosition(w, err - text);
       XmProcessTraversal(w, XmTRAVERSE_CURRENT);
    }
    
    XtFree(text);

    return ret;
}

int    MotifKnobs_ExtractIntValue      (Widget w, int    *result)
{
  int     v;
  char   *err;
  char   *text = XmTextGetString(w);
  int     ret;

    v = strtol(text, &err, 10);
    ret = !(err == text  ||  *err != '\0');

    if (ret)
    {
        *result = v;
    }
    else
    {
       XBell(XtDisplay(w), 100);
       XmTextSetInsertionPosition(w, err - text);
       XmProcessTraversal(w, XmTRAVERSE_CURRENT);
    }
    
    XtFree(text);

    return ret;
}


/*** Common "container" interface ***********************************/

static void TitleClickHandler(Widget     w,
                              XtPointer  closure,
                              XEvent    *event,
                              Boolean   *continue_to_dispatch)
{
  Widget  victim = (Widget) closure;
    
    if (MotifKnobs_IsDoubleClick(w, event, continue_to_dispatch))
        XhSwitchContentFoldedness(ABSTRZE(victim));
}

int    MotifKnobs_CreateContainer(DataKnob k, CxWidget parent,
                                  motifknobs_cpopulator_t populator, void *privptr,
                                  int flags)
{
  Dimension  fst;
  XmString   s;
  char       buf[1000];
  char      *ls;
  char      *tip_p;
  Widget     ttl_cont;
  Widget     top, bot, lft, rgt;
  int        title_side = (flags & MOTIFKNOBS_CF_TITLE_SIDE_MASK) >> MOTIFKNOBS_CF_TITLE_SIDE_shift;
  int        title_vert = (title_side == MOTIFKNOBS_TITLE_SIDE_LEFT  ||
                           title_side == MOTIFKNOBS_TITLE_SIDE_RIGHT);
  Widget    *side_p;
    
  enum
  {
      FOLD_SEP_LEN = 60,
      FOLD_SEP_THK = 4,
      FOLD_SEP_TYP = XmSHADOW_ETCHED_IN_DASH,
  };
    
    /* Sanitize parameters first */
    if (flags & (MOTIFKNOBS_CF_NOTITLE | MOTIFKNOBS_CF_NOFOLD))
        flags &=~ MOTIFKNOBS_CF_FOLDED;
    
    k->w = ABSTRZE(XtVaCreateManagedWidget("elementForm",
                                           xmFormWidgetClass,
                                           CNCRTZE(parent),
                                           NULL));

    if (flags & MOTIFKNOBS_CF_NOSHADOW) XtVaSetValues(CNCRTZE(k->w),
                                                      XmNshadowThickness, 0,
                                                      NULL);

    XtVaGetValues(CNCRTZE(k->w),
                  XmNbackground,      &(k->defbg),
                  XmNshadowThickness, &fst,
                  NULL);

    if (!get_knob_label(k, &ls)) ls = "(UNLABELED)";

    top = bot = lft = rgt = NULL;
    
    if ((flags & MOTIFKNOBS_CF_NOTITLE) == 0)
    {
        if      (title_side == MOTIFKNOBS_TITLE_SIDE_TOP)    side_p = &top;
        else if (title_side == MOTIFKNOBS_TITLE_SIDE_BOTTOM) side_p = &bot;
        else if (title_side == MOTIFKNOBS_TITLE_SIDE_LEFT)   side_p = &lft;
        else                                                 side_p = &rgt;

        /* Around-title container */
        ttl_cont = ABSTRZE(XtVaCreateManagedWidget("titleForm",
                                                   xmFormWidgetClass,
                                                   CNCRTZE(k->w),
                                                   XmNshadowThickness, 0,
                                                   NULL));
        if (title_vert)
        {
            XtVaSetValues(ttl_cont,
                          XmNtopAttachment,        XmATTACH_FORM,
                          XmNtopOffset,            fst,
                          XmNbottomAttachment,     XmATTACH_FORM,
                          XmNbottomOffset,         fst,
                          NULL);
            if (title_side == MOTIFKNOBS_TITLE_SIDE_LEFT)
                XtVaSetValues(ttl_cont,
                              XmNleftAttachment,   XmATTACH_FORM,
                              XmNleftOffset,       fst,
                              NULL);
            else
                XtVaSetValues(ttl_cont,
                              XmNrightAttachment,  XmATTACH_FORM,
                              XmNrightOffset,      fst,
                              NULL);
        }
        else
        {
            XtVaSetValues(ttl_cont,
                          XmNleftAttachment,       XmATTACH_FORM,
                          XmNleftOffset,           fst,
                          XmNrightAttachment,      XmATTACH_FORM,
                          XmNrightOffset,          fst,
                          NULL);
            if (title_side == MOTIFKNOBS_TITLE_SIDE_TOP)
                XtVaSetValues(ttl_cont,
                              XmNtopAttachment,    XmATTACH_FORM,
                              XmNtopOffset,        fst,
                              NULL);
            else
                XtVaSetValues(ttl_cont,
                              XmNbottomAttachment, XmATTACH_FORM,
                              XmNbottomOffset,     fst,
                              NULL);
        }
        *side_p = CNCRTZE(ttl_cont);

        /* Toolbar container */
        if (flags & MOTIFKNOBS_CF_TOOLBAR)
        {
            k->u.c.toolbar = ABSTRZE(XtVaCreateManagedWidget("elemToolbar",
                                                             xmFormWidgetClass,
                                                             CNCRTZE(ttl_cont),
                                                             XmNshadowThickness, 0,
                                                             XmNtraversalOn,     False,
                                                             NULL));
            if (title_vert) attachtop   (k->u.c.toolbar, NULL, 0);
            else            attachleft  (k->u.c.toolbar, NULL, 0);
        }

        /* Attitle container */
        if (flags & MOTIFKNOBS_CF_ATTITLE)
        {
            k->u.c.attitle = ABSTRZE(XtVaCreateManagedWidget("elemAttitle",
                                                             xmFormWidgetClass,
                                                             CNCRTZE(ttl_cont),
                                                             XmNshadowThickness, 0,
                                                             NULL));
            if (title_vert) attachbottom(k->u.c.attitle, NULL, 0);
            else            attachright (k->u.c.attitle, NULL, 0);
        }
        /* The label itself... */
        k->u.c.caption =
            /*ABSTRZE(*/XtVaCreateManagedWidget("elemCaption",
                                            xmLabelWidgetClass,
                                            CNCRTZE(ttl_cont),
                                            XmNlabelString,     s = XmStringCreateLtoR(ls, xh_charset),
#if 1
                                            XmNalignment,       XmALIGNMENT_CENTER,
#else
                                            XmNleftAttachment,  XmATTACH_FORM,
                                            XmNleftOffset,      fst,
                                            XmNrightAttachment, XmATTACH_FORM,
                                            XmNrightOffset,     fst,
                                            XmNtopAttachment,   XmATTACH_FORM,
                                            XmNtopOffset,       fst,
#endif
                                            NULL)/*)*/;
        XmStringFree(s);
#if 1
        if (title_vert) XhAssignVertLabel(k->u.c.caption, ls, 0);
        if (title_vert)
        {
            attachtop   (k->u.c.caption, k->u.c.toolbar, 0);
            attachbottom(k->u.c.caption, k->u.c.attitle, 0);
        }
        else
        {
            attachleft  (k->u.c.caption, k->u.c.toolbar, 0);
            attachright (k->u.c.caption, k->u.c.attitle, 0);
        }
#else
        top = CNCRTZE(k->u.c.caption);
#endif
        /* ...and add a tip, if present */
        if (get_knob_tip(k, &tip_p))
            XhSetBalloon(k->u.c.caption, tip_p);

        if ((flags & MOTIFKNOBS_CF_NOHLINE) == 0)
        {
#if 1
            k->u.c.hline =
                ABSTRZE(XtVaCreateManagedWidget("hline",
                                                xmSeparatorWidgetClass,
                                                CNCRTZE(k->w),
                                                XmNorientation,     title_vert? XmVERTICAL
                                                                              : XmHORIZONTAL,
                                                NULL));
            *side_p = CNCRTZE(k->u.c.hline);

            if (title_vert)
            {
                XtVaSetValues(k->u.c.hline,
                              XmNtopAttachment,        XmATTACH_FORM,
                              XmNtopOffset,            fst,
                              XmNbottomAttachment,     XmATTACH_FORM,
                              XmNbottomOffset,         fst,
                              NULL);
                if (title_side == MOTIFKNOBS_TITLE_SIDE_LEFT)
                    XtVaSetValues(k->u.c.hline,
                                  XmNleftAttachment,   XmATTACH_WIDGET,
                                  XmNleftWidget,       ttl_cont,
                                  NULL);
                else
                    XtVaSetValues(k->u.c.hline,
                                  XmNrightAttachment,  XmATTACH_WIDGET,
                                  XmNrightWidget,      ttl_cont,
                                  NULL);
            }
            else
            {
                XtVaSetValues(k->u.c.hline,
                              XmNleftAttachment,       XmATTACH_FORM,
                              XmNleftOffset,           fst,
                              XmNrightAttachment,      XmATTACH_FORM,
                              XmNrightOffset,          fst,
                              NULL);
                if (title_side == MOTIFKNOBS_TITLE_SIDE_TOP)
                    XtVaSetValues(k->u.c.hline,
                                  XmNtopAttachment,    XmATTACH_WIDGET,
                                  XmNtopWidget,        ttl_cont,
                                  NULL);
                else
                    XtVaSetValues(k->u.c.hline,
                                  XmNbottomAttachment, XmATTACH_WIDGET,
                                  XmNbottomWidget,     ttl_cont,
                                  NULL);
            }
#else
            k->u.c.hline =
                ABSTRZE(XtVaCreateManagedWidget("hline",
                                                xmSeparatorWidgetClass,
                                                CNCRTZE(k->w),
                                                XmNorientation,     XmHORIZONTAL,
                                                XmNleftAttachment,  XmATTACH_FORM,
                                                XmNleftOffset,      fst,
                                                XmNrightAttachment, XmATTACH_FORM,
                                                XmNrightOffset,     fst,
                                                XmNtopAttachment,   XmATTACH_WIDGET,
                                                XmNtopWidget,       k->u.c.caption,
                                                NULL));
            top = CNCRTZE(k->u.c.hline);
#endif
        }
        
        if ((flags & MOTIFKNOBS_CF_NOFOLD) == 0)
        {
            if ((flags & MOTIFKNOBS_CF_SEPFOLD) == 0)
            {
                snprintf(buf, sizeof(buf), "{%s}", ls);
                k->u.c.compact =
                    ABSTRZE(XtVaCreateWidget("caption2",
                                             xmLabelWidgetClass,
                                             CNCRTZE(k->w),
                                             XmNlabelString, s = XmStringCreateLtoR(buf, xh_charset),
                                             NULL));
                XmStringFree(s);
                
                if ((flags & MOTIFKNOBS_CF_FOLD_H) == 0)
                {
                    XhAssignVertLabel(k->u.c.compact, buf, 0);
                }
            }
            else
            {
                if ((flags & MOTIFKNOBS_CF_FOLD_H) == 0)
                    k->u.c.compact =
                        ABSTRZE(XtVaCreateWidget("caption2",
                                                 xmSeparatorWidgetClass,
                                                 CNCRTZE(k->w),
                                                 XmNorientation,      XmVERTICAL,
                                                 XmNheight,           FOLD_SEP_LEN,
                                                 XmNshadowThickness,  FOLD_SEP_THK,
                                                 XmNseparatorType,    FOLD_SEP_TYP,
                                                 NULL));
                else
                    k->u.c.compact =
                        ABSTRZE(XtVaCreateWidget("caption2",
                                                 xmSeparatorWidgetClass,
                                                 CNCRTZE(k->w),
                                                 XmNorientation,      XmHORIZONTAL,
                                                 XmNwidth,            FOLD_SEP_LEN,
                                                 XmNshadowThickness,  FOLD_SEP_THK,
                                                 XmNseparatorType,    FOLD_SEP_TYP,
                                                 NULL));
            }
        
            XhSetBalloon(k->u.c.compact, "Double-click to expand");

            XtVaSetValues(CNCRTZE(k->u.c.compact),
                          XmNleftAttachment,   XmATTACH_FORM,
                          XmNleftOffset,       fst,
                          XmNrightAttachment,  XmATTACH_FORM,
                          XmNrightOffset,      fst,
                          XmNtopAttachment,    XmATTACH_FORM,
                          XmNtopOffset,        fst,
                          XmNbottomAttachment, XmATTACH_FORM,
                          XmNbottomOffset,     fst,
                          NULL);

            XtAddEventHandler(CNCRTZE(k->u.c.caption), ButtonPressMask, False,
                              TitleClickHandler, (XtPointer)k->w);
            XtAddEventHandler(CNCRTZE(k->u.c.compact), ButtonPressMask, False,
                              TitleClickHandler, (XtPointer)k->w);
        }
    }

    k->u.c.innage = ABSTRZE(populator(k, k->w, privptr));
    if (k->u.c.innage == NULL) return -1;
#if 1
    attachleft  (k->u.c.innage, lft, lft == NULL? fst : 0);
    attachright (k->u.c.innage, rgt, rgt == NULL? fst : 0);
    attachtop   (k->u.c.innage, top, top == NULL? fst : 0);
    attachbottom(k->u.c.innage, bot, bot == NULL? fst : 0);
#else
    XtVaSetValues(CNCRTZE(k->u.c.innage),
                  XmNleftAttachment,   XmATTACH_FORM,
                  XmNleftOffset,       fst,
                  XmNrightAttachment,  XmATTACH_FORM,
                  XmNrightOffset,      fst,
                  XmNtopAttachment,    XmATTACH_WIDGET,
                  XmNtopWidget,        top,
                  XmNtopOffset,        top == NULL? fst : 0,
                  XmNbottomAttachment, XmATTACH_FORM,
                  XmNbottomOffset,     fst,
                  NULL);
#endif

    if (flags & MOTIFKNOBS_CF_FOLDED) XhSwitchContentFoldedness(k->w);

    return 0;
}

int    MotifKnobs_PopulateContainerAttitle(DataKnob k, int nattl, int title_side)
{
  int       n;
  CxWidget  prev;
  DataKnob  ck;

    for (n = 0,      prev = NULL,   ck = k->u.c.content;
         n < nattl;
         n++,        prev = ck->w,  ck++)
    {
        if (CreateKnob(ck, k->u.c.attitle) < 0) return -1;
        if (title_side == MOTIFKNOBS_TITLE_SIDE_TOP  ||
            title_side == MOTIFKNOBS_TITLE_SIDE_BOTTOM)
            attachleft(ck->w, prev, prev == NULL? 0 : MOTIFKNOBS_INTERKNOB_H_SPACING);
        else
            attachtop (ck->w, prev, prev == NULL? 0 : MOTIFKNOBS_INTERKNOB_H_SPACING);
    }

    return 0;
}

/* ((( TMP: v2-style button style support */
// Part 1: MaybeAssignPixmap()
typedef struct
{
    char   *buf;
    size_t  bufsize;
} maybeassign_info_t;

static void *maybeassign_check_proc(const char *name,
                                    const char *path,
                                    void       *privptr)
{
  maybeassign_info_t *info = privptr;
  char                plen = strlen(path);

    check_snprintf(info->buf, info->bufsize, "%s%s%s.xpm",
                   path,
                   plen == 0  ||  path[plen-1] == '/'? "" : "/",
                   name);

    /* Note: use of F_OK and not R_OK is intentional -- for cases when
             a file with "this" name exists but isn't readable or of
             wrong type, an error would occur, helping the user to
             locate and solve a problem (while just skipping the non-R_OK
             file would tangle the situation). */
    return access(info->buf, F_OK) == 0? info->buf : NULL;
}

static void MaybeAssignPixmap(Widget w, const char *name)
{
  char                buf[PATH_MAX];
  maybeassign_info_t  buf_info;
    
    if (name == NULL  ||  *name == '\0') return;
  
    buf_info.buf     = buf;
    buf_info.bufsize = sizeof(buf);
    
    if (findfilein(name,
#if OPTION_HAS_PROGRAM_INVOCATION_NAME
                   program_invocation_name,
#else
                   NULL,
#endif /* OPTION_HAS_PROGRAM_INVOCATION_NAME */
                   maybeassign_check_proc,
                   &buf_info,
                   /*!!!DIRSTRUCT*/
#define PIX_DIRLIST(dir) "!/../"dir \
    FINDFILEIN_SEP "$PULT_ROOT/"dir \
    FINDFILEIN_SEP "~/4pult/"dir
                   "./"
                       FINDFILEIN_SEP "!/"
                       FINDFILEIN_SEP PIX_DIRLIST("lib/icons")
                       FINDFILEIN_SEP PIX_DIRLIST("include/pixmaps")) != NULL)
        XhAssignPixmapFromFile(ABSTRZE(w), buf);
}
// Part 2: button-tuning itself
typedef struct
{
    int   color;
    int   bg;
    int   size;
    int   bold;
    char *icon;
} buttonopts_t;
static psp_paramdescr_t text2buttonopts[] =
{
    PSP_P_LOOKUP ("color",  buttonopts_t, color, -1,                MotifKnobs_colors_lkp),
    PSP_P_LOOKUP ("bg",     buttonopts_t, bg,    -1,                MotifKnobs_colors_lkp),
    PSP_P_LOOKUP ("size",   buttonopts_t, size,  -1,                MotifKnobs_sizes_lkp),
    PSP_P_FLAG   ("bold",   buttonopts_t, bold,  1,                 0),
    PSP_P_FLAG   ("medium", buttonopts_t, bold,  0,                 1),
    PSP_P_MSTRING("icon",   buttonopts_t, icon,  NULL,              100),
    PSP_P_END()
};

void MotifKnobs_TuneButtonKnob(DataKnob k, CxWidget w, int flags)
{
  buttonopts_t    opts;

  CxPixel         bg;
  Colormap        cmap;
  CxPixel         junk;
  CxPixel         ts;
  CxPixel         bs;
  CxPixel         am;

  char            fontspec[100];

  const char     *rqd_prefix = "V2STYLE:";

    if (k        == NULL  ||
        k->style == NULL  ||
        memcmp(k->style, rqd_prefix, strlen(rqd_prefix)) != 0)
        return;

    bzero(&opts, sizeof(opts));
    if (psp_parse(k->style + strlen(rqd_prefix), NULL,
                  &opts,
                  Knobs_wdi_equals_c, Knobs_wdi_separators, Knobs_wdi_terminators,
                  text2buttonopts) != PSP_R_OK)
    {
        fprintf(stderr, "%s: [%s/\"%s\"].style: %s \n",
                __FUNCTION__, k->ident, k->label, psp_error());
        psp_free(&opts, text2buttonopts);
        return;
    }

    if (opts.color > 0)
        XtVaSetValues(w,
                      XmNforeground, XhGetColor(opts.color),
                      NULL);
    if (opts.bg > 0)
    {
        bg = XhGetColor(opts.bg);
        XtVaGetValues(XtParent(w), XmNcolormap, &cmap, NULL);
        XmGetColors(XtScreen(XtParent(w)), cmap, bg, &junk, &ts, &bs, &am);

        XtVaSetValues(w,
                      XmNbackground,        bg,
                      XmNtopShadowColor,    ts,
                      XmNbottomShadowColor, bs,
                      XmNarmColor,          am,
                      NULL);
    }

    if (opts.size > 0  &&  (flags & TUNE_BUTTON_KNOB_F_NO_FONT) == 0)
    {
        check_snprintf(fontspec, sizeof(fontspec),
                       "-*-" XH_FONT_PROPORTIONAL_FMLY "-%s-r-*-*-%d-*-*-*-*-*-*-*",
                       opts.bold? XH_FONT_BOLD_WGHT : XH_FONT_MEDIUM_WGHT,
                       opts.size);
        XtVaSetValues(w,
                      XtVaTypedArg, XmNfontList, XmRString, fontspec, strlen(fontspec)+1,
                      NULL);
    }

    psp_free(&opts, text2buttonopts);
}

void MotifKnobs_TuneTextKnob  (DataKnob k, CxWidget w, int flags)
{
  buttonopts_t    opts;

  CxPixel         bg;
  Colormap        cmap;
  CxPixel         junk;
  CxPixel         ts;
  CxPixel         bs;
  CxPixel         am;

  char            fontspec[100];

  const char     *rqd_prefix = "V2STYLE:";

    if (k        == NULL  ||
        k->style == NULL  ||
        memcmp(k->style, rqd_prefix, strlen(rqd_prefix)) != 0)
        return;

    bzero(&opts, sizeof(opts));
    if (psp_parse(k->style + strlen(rqd_prefix), NULL,
                  &opts,
                  Knobs_wdi_equals_c, Knobs_wdi_separators, Knobs_wdi_terminators,
                  text2buttonopts) != PSP_R_OK)
    {
        fprintf(stderr, "%s: [%s/\"%s\"].style: %s \n",
                __FUNCTION__, k->ident, k->label, psp_error());
        psp_free(&opts, text2buttonopts);
        return;
    }

    if (opts.color > 0)
        XtVaSetValues(w,
                      XmNforeground, XhGetColor(opts.color),
                      NULL);
    if (opts.bg > 0)
    {
        bg = XhGetColor(opts.bg);
        XtVaGetValues(XtParent(w), XmNcolormap, &cmap, NULL);
        XmGetColors(XtScreen(XtParent(w)), cmap, bg, &junk, &ts, &bs, &am);

        XtVaSetValues(w,
                      XmNbackground,        bg,
                      XmNtopShadowColor,    ts,
                      XmNbottomShadowColor, bs,
                      XmNarmColor,          am,
                      NULL);
    }

    if (opts.size > 0  &&  (flags & TUNE_BUTTON_KNOB_F_NO_FONT) == 0)
    {
        check_snprintf(fontspec, sizeof(fontspec),
                       "-*-" XH_FONT_FIXED_FMLY "-%s-r-*-*-%d-*-*-*-*-*-*-*",
                       opts.bold? XH_FONT_BOLD_WGHT : XH_FONT_MEDIUM_WGHT,
                       opts.size);
fprintf(stderr, "<%s>\n", fontspec);
        XtVaSetValues(w,
                      XtVaTypedArg, XmNfontList, XmRString, fontspec, strlen(fontspec)+1,
                      NULL);
    }

    if ((flags & TUNE_BUTTON_KNOB_F_NO_PIXMAP) == 0)
        MaybeAssignPixmap(w, opts.icon);

    psp_free(&opts, text2buttonopts);
}
/* ))) TMP: v2-style button style support */
