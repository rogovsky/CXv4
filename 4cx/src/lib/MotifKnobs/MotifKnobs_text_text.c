#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/Form.h>
#include <Xm/Text.h>

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_text_text.h"


enum {DEFAULT_COLUMNS = 10};

typedef struct
{
    int     columns;
} text_text_privrec_t;

static psp_paramdescr_t text2texttextopts[] =
{
    PSP_P_INT("columns", text_text_privrec_t, columns,  DEFAULT_COLUMNS, 1, 1000),

    PSP_P_END()
};

//////////////////////////////////////////////////////////////////////
// MotifKnobs_internals alikes
#define XK_MISCELLANY
#define XK_LATIN1
#include <X11/keysymdef.h>

static void TextTextChangeCB(Widget     w,
                             XtPointer  closure,
                             XtPointer  call_data)
{
  DataKnob             k    = (DataKnob)              closure;
  XmAnyCallbackStruct *info = (XmAnyCallbackStruct *) call_data;
  char                *text;
  
    /* Do a CANCEL if losing focus */
    if (info->reason != XmCR_ACTIVATE)
    {
        cancel_knob_editing(k);
        return;
    }

    text = XmTextGetString(w);
    /*!!! UNDO: store_knob_undo_value (k);*/
    set_knob_textvalue(k, text, 1);
    XtFree(text);
}

static void TextTextUserCB(Widget     w __attribute__((unused)),
                           XtPointer  closure,
                           XtPointer  call_data)
{
  DataKnob                    k    = (DataKnob)                     closure;
  XmTextVerifyCallbackStruct *info = (XmTextVerifyCallbackStruct *) call_data;
    
    if (info->reason == XmCR_MOVING_INSERT_CURSOR ||
        info->event  != NULL)
        user_began_knob_editing(k);
}

static void TextTextKbdHandler(Widget     w,
                               XtPointer  closure,
                               XEvent    *event,
                               Boolean   *continue_to_dispatch __attribute__((unused)))
{
  DataKnob      k  = (DataKnob)    closure;
  XKeyEvent    *ev = (XKeyEvent *) event;

  KeySym        ks;
  Modifiers     mr;

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
        /*!!! UNDO: perform_knob_undo(k);*/
        *continue_to_dispatch = False;
        return;
    }
}
//////////////////////////////////////////////////////////////////////

static int CreateTextText(DataKnob k, CxWidget parent)
{
  text_text_privrec_t *me = (text_text_privrec_t *)k->privptr;

    if (me->columns < 1) me->columns = DEFAULT_COLUMNS;
    if (k->is_rw)        me->columns++;

    k->w = MotifKnobs_MakeTextWidget(parent, k->is_rw, me->columns);

    if (k->is_rw)
    {
        XtAddCallback(k->w, XmNactivateCallback,     TextTextChangeCB, (XtPointer)k);
        XtAddCallback(k->w, XmNlosingFocusCallback,  TextTextChangeCB, (XtPointer)k);

        XtAddCallback(k->w, XmNmodifyVerifyCallback, TextTextUserCB,   (XtPointer)k);
        XtAddCallback(k->w, XmNmotionVerifyCallback, TextTextUserCB,   (XtPointer)k);

        MotifKnobs_SetTextCursorCallback(k->w);

        XtAddEventHandler(k->w, KeyPressMask,    False, TextTextKbdHandler,   (XtPointer)k);
    }

    MotifKnobs_TuneTextKnob(k, k->w, 0);
    MotifKnobs_DecorateKnob(k);

    return 0;
}

static void TextSetText_m(DataKnob k, const char *s)
{
  text_text_privrec_t *me = (text_text_privrec_t *)k->privptr;

    XmTextSetString(k->w, s);
}

dataknob_text_vmt_t motifknobs_text_text_vmt =
{
    {DATAKNOB_TEXT, "text",
        sizeof(text_text_privrec_t), text2texttextopts,
        0,
        CreateTextText, MotifKnobs_CommonDestroy_m,
        MotifKnobs_CommonColorize_m, NULL},
    TextSetText_m
};

dataknob_text_vmt_t motifknobs_def_text_vmt =
{
    {DATAKNOB_TEXT, DEFAULT_KNOB_LOOK_NAME,
        sizeof(text_text_privrec_t), NULL,
        0,
        CreateTextText, MotifKnobs_CommonDestroy_m,
        MotifKnobs_CommonColorize_m, NULL},
    TextSetText_m
};
