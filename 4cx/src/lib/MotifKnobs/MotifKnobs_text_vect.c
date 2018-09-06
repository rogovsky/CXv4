#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/Text.h>

#include "misc_macros.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_text_vect.h"


enum {DEFAULT_COLUMNS  = 20};
enum {MAX_NELEMS_LIMIT = 100};

typedef struct
{
    int       columns;

    int       nelems_limit;
    double   *edata;
    char     *tbuf;
    Cardinal  tbuf_allocd;
} text_vect_privrec_t;

static psp_paramdescr_t text2textvectopts[] =
{
    PSP_P_INT("columns", text_vect_privrec_t, columns,  DEFAULT_COLUMNS, 1, 1000),
    PSP_P_END()
};

//////////////////////////////////////////////////////////////////////
// MotifKnobs_internals alikes
#define XK_MISCELLANY
#define XK_LATIN1
#include <X11/keysymdef.h>


static void TextVectChangeCB(Widget     w,
                             XtPointer  closure,
                             XtPointer  call_data)
{
  DataKnob             k    = (DataKnob)              closure;
  text_vect_privrec_t *me = (text_vect_privrec_t *)k->privptr;
  XmAnyCallbackStruct *info = (XmAnyCallbackStruct *) call_data;
  char                *text;
  int                  stage;
  const char          *p;
  int                  n;
  double               v;
  const char          *endptr;
  
    /* Do a CANCEL if losing focus */
    if (info->reason != XmCR_ACTIVATE)
    {
        cancel_knob_editing(k);
        return;
    }

    text = XmTextGetString(w);
    /*!!! UNDO: store_knob_undo_value (k);*/
    for (stage = 0;  stage < 2;  stage++)
    {
        for (p = text, n = 0;  /*???*/;  n++)
        {
            /* Skip spaces and commas */
            while (*p != '\0'  &&  (isspace(*p)  ||  *p == ',')) p++;
            if (*p == '\0') break;
            /* Check nelems */
            if (n >= me->nelems_limit)
            {
                XBell(XtDisplay(w), 100);
                XmTextSetInsertionPosition(w, p - text);
                goto ERREXIT;
            }
            /*  */
            v = strtod(p, &endptr);
            if (endptr == p)
            {
                XBell(XtDisplay(w), 100);
                XmTextSetInsertionPosition(w, p - text);
                goto ERREXIT;
            }
            p = endptr;
            if (stage != 0) me->edata[n] = v;
        }
    }
    set_knob_vectvalue(k, me->edata, n, 1);
 ERREXIT:
    XtFree(text);
}

static void TextVectUserCB(Widget     w __attribute__((unused)),
                           XtPointer  closure,
                           XtPointer  call_data)
{
  DataKnob                    k    = (DataKnob)                     closure;
  XmTextVerifyCallbackStruct *info = (XmTextVerifyCallbackStruct *) call_data;
    
    if (info->reason == XmCR_MOVING_INSERT_CURSOR ||
        info->event  != NULL)
        user_began_knob_editing(k);
}

static void TextVectKbdHandler(Widget     w,
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

static int  CreateTextVect(DataKnob k, CxWidget parent)
{
  text_vect_privrec_t *me = (text_vect_privrec_t *)k->privptr;
  int                  cpe;

    if (me->columns < 1) me->columns = DEFAULT_COLUMNS;
    if (k->is_rw)        me->columns++;

    me->nelems_limit = k->u.v.src.max_nelems;
    if (me->nelems_limit < 1) return -1;
    if (me->nelems_limit > MAX_NELEMS_LIMIT)
        me->nelems_limit = MAX_NELEMS_LIMIT;

    cpe = GetTextColumns(k->u.v.dpyfmt, ",");
    me->tbuf_allocd = cpe * me->nelems_limit;
    me->tbuf = XtMalloc(me->tbuf_allocd);
    if (me->tbuf == NULL) return -1;
    if (k->is_rw)
    {
        me->edata = (void *)(XtCalloc(me->nelems_limit, sizeof(me->edata[0])));
        if (me->edata == NULL)
        {
            XtFree((void *)(me->tbuf)); me->tbuf = NULL;
            return -1;
        }
    }

    k->w = MotifKnobs_MakeTextWidget(parent, k->is_rw, me->columns);

    if (k->is_rw)
    {
        XtAddCallback(k->w, XmNactivateCallback,     TextVectChangeCB, (XtPointer)k);
        XtAddCallback(k->w, XmNlosingFocusCallback,  TextVectChangeCB, (XtPointer)k);

        XtAddCallback(k->w, XmNmodifyVerifyCallback, TextVectUserCB,   (XtPointer)k);
        XtAddCallback(k->w, XmNmotionVerifyCallback, TextVectUserCB,   (XtPointer)k);

        MotifKnobs_SetTextCursorCallback(k->w);

        XtAddEventHandler(k->w, KeyPressMask,    False, TextVectKbdHandler,   (XtPointer)k);
    }

    MotifKnobs_TuneTextKnob(k, k->w, 0);
    MotifKnobs_DecorateKnob(k);

    return 0;
}

static void DestroyTextVect_m(DataKnob k)
{
  text_vect_privrec_t *me = (text_vect_privrec_t *)k->privptr;

    MotifKnobs_CommonDestroy_m(k);
    if (me->tbuf  != NULL) XtFree((void *)(me->tbuf));  me->tbuf  = NULL;
    if (me->edata != NULL) XtFree((void *)(me->edata)); me->edata = NULL;
}

static void TextSetVect_m(DataKnob k, const double *data, int nelems)
{
  text_vect_privrec_t *me = (text_vect_privrec_t *)k->privptr;
  char                 buf[400]; // So that 1e308 fits
  const char          *p;
  int                  n;
  int                  ofs;
  size_t               buflen;
  size_t               seglen;
  size_t               new_size;
  char                *new_tbuf;

    if (nelems < 0)                nelems = 0;
    if (nelems > me->nelems_limit) nelems = me->nelems_limit;

    for (n = 0, ofs = 0, me->tbuf[ofs] = '\0';  n < nelems;  n++)
    {
        p = snprintf_dbl_trim(buf, sizeof(buf), k->u.v.dpyfmt, data[n], k->is_rw);
        seglen = buflen = strlen(p);
        if (n > 0) seglen++; // Take "," separator into account

        if (ofs + seglen + 1 > me->tbuf_allocd)
        {
            new_size = ofs + seglen + 1 + 100;
            new_tbuf = XtRealloc(me->tbuf, new_size);
            if (new_tbuf == NULL) goto END_PREPARE;
            me->tbuf = new_tbuf;
            me->tbuf_allocd = new_size;
        }

        if (n > 0) me->tbuf[ofs++] = ',';
        memcpy(me->tbuf + ofs, p, buflen + 1);
        ofs += buflen;
        
    }
 END_PREPARE:;
    if (me->edata != NULL) memcpy(me->edata, data, nelems * sizeof(data[0]));
    XmTextSetString(k->w, me->tbuf);
}

dataknob_vect_vmt_t motifknobs_text_vect_vmt =
{
    {DATAKNOB_VECT, "text",
        sizeof(text_vect_privrec_t), text2textvectopts,
        0,
        CreateTextVect, DestroyTextVect_m,
        MotifKnobs_CommonColorize_m, NULL},
    TextSetVect_m
};

dataknob_vect_vmt_t motifknobs_def_vect_vmt =
{
    {DATAKNOB_VECT, DEFAULT_KNOB_LOOK_NAME,
        sizeof(text_vect_privrec_t), NULL,
        0,
        CreateTextVect, DestroyTextVect_m,
        MotifKnobs_CommonColorize_m, NULL},
    TextSetVect_m
};
