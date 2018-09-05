#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Text.h>

#include "misc_macros.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_matrix_vect.h"


enum {MAX_CELLS = 100};

typedef struct
{
    Widget  nelems_w;
    Widget  send_b;
    Widget  cancel_b;
    Widget *cells;
    double *edata;
    int     cells_allocd;

    int     defcols_obtained;
    Pixel   inp_deffg;
    Pixel   inp_defbg;

    int     last_nelems;
    int     is_editing;

    int     number_cells;
    int     no_nelems_w;
} matrix_vect_privrec_t;

static psp_paramdescr_t text2matrixvectopts[] =
{
    PSP_P_FLAG("number_cells", matrix_vect_privrec_t, number_cells, 1, 0),
    PSP_P_FLAG("no_numbers",   matrix_vect_privrec_t, number_cells, 0, 1),
    PSP_P_FLAG("no_nelems",    matrix_vect_privrec_t, no_nelems_w,  1, 0),
    PSP_P_END()
};

//////////////////////////////////////////////////////////////////////
// MotifKnobs_internals alikes
#define XK_MISCELLANY
#define XK_LATIN1
#include <X11/keysymdef.h>

static void StartEditing(DataKnob k)
{
  matrix_vect_privrec_t *me = (matrix_vect_privrec_t *)k->privptr;

    if (me->is_editing) return;
    me->is_editing = 1;
    user_began_knob_editing(k);
    if (me->send_b   != NULL) XtSetSensitive(me->send_b,   True);
    if (me->cancel_b != NULL) XtSetSensitive(me->cancel_b, True);
}

static void StopEditing (DataKnob k)
{
  matrix_vect_privrec_t *me = (matrix_vect_privrec_t *)k->privptr;

    if (me->is_editing == 0) return;
    me->is_editing = 0;
    cancel_knob_editing(k);
    if (me->send_b   != NULL) XtSetSensitive(me->send_b,   False);
    if (me->cancel_b != NULL) XtSetSensitive(me->cancel_b, False);
}

static void VectUserCB(Widget     w __attribute__((unused)),
                       XtPointer  closure,
                       XtPointer  call_data)
{
  DataKnob                    k    = (DataKnob)                     closure;
  XmTextVerifyCallbackStruct *info = (XmTextVerifyCallbackStruct *) call_data;
    
    if (info->reason == XmCR_MOVING_INSERT_CURSOR ||
        info->event  != NULL)
        StartEditing(k);
}

//////////////////////////////////////////////////////////////////////

static void set_nelems(DataKnob k, int nelems, int zero_new)
{
  matrix_vect_privrec_t *me = (matrix_vect_privrec_t *)k->privptr;
  char                   buf[400]; // So that 1e308 fits
  const char            *p;
  int                    n;

    if (me->nelems_w != NULL)
    {
        sprintf(buf, k->is_rw? "%d" : "%d:", nelems);
        XmTextSetString(me->nelems_w, buf);
    }

    if      (nelems < me->last_nelems)
        for (n = nelems;  n < me->last_nelems;  n++)
        {
            XtSetSensitive (me->cells[n], 0);
            XmTextSetString(me->cells[n], "-");
        }
    else if (nelems > me->last_nelems)
        for (n = me->last_nelems;  n < nelems;  n++)
        {
            XtSetSensitive (me->cells[n], 1);
            if (zero_new)
            {
                me->edata[n] = 0.0;
                p = snprintf_dbl_trim(buf, sizeof(buf), k->u.v.dpyfmt, 0.0, k->is_rw);
                XmTextSetString(me->cells[n], p);
            }
        }

    me->last_nelems = nelems;
}

static void send_data(DataKnob k)
{
  matrix_vect_privrec_t *me = (matrix_vect_privrec_t *)k->privptr;
  int                    n;

    for (n = 0;  n < me->last_nelems;  n++)
        if (!MotifKnobs_ExtractDoubleValue(me->cells[n], me->edata + n)) return;

    set_knob_vectvalue(k, me->edata, me->last_nelems, 1);
}

//////////////////////////////////////////////////////////////////////

static void SendCB  (Widget     w __attribute__((unused)),
                     XtPointer  closure,
                     XtPointer  call_data)
{
  DataKnob             k  = (DataKnob)             closure;

    send_data(k);
}

static void CancelCB(Widget     w __attribute__((unused)),
                     XtPointer  closure,
                     XtPointer  call_data)
{
  DataKnob             k  = (DataKnob)             closure;

    StopEditing(k);
}

static void NelsActivateCB(Widget     w,
                           XtPointer  closure,
                           XtPointer  call_data)
{
  DataKnob               k  = (DataKnob)             closure;
  matrix_vect_privrec_t *me = (matrix_vect_privrec_t *)k->privptr;
  int                    iv;

    StartEditing(k);
    if (!MotifKnobs_ExtractIntValue(w, &iv)) return;
    if (iv < 0)                {iv = 0;                XBell(XtDisplay(w), 100);};
    if (iv > me->cells_allocd) {iv = me->cells_allocd; XBell(XtDisplay(w), 100);}
    set_nelems(k, iv, 1);
}

static void CellActivateCB(Widget     w,
                           XtPointer  closure,
                           XtPointer  call_data)
{
  DataKnob               k  = (DataKnob)             closure;
  matrix_vect_privrec_t *me = (matrix_vect_privrec_t *)k->privptr;
  int                    n;

    for (n = 0;  n < me->cells_allocd;  n++)
        if (me->cells[n] == w) break;
    if (n >= me->cells_allocd) return;

    StartEditing(k);
    if (!MotifKnobs_ExtractDoubleValue(me->cells[n], me->edata + n)) return;
}

static void VectKbdHandler(Widget     w  __attribute__((unused)),
                           XtPointer  closure,
                           XEvent    *event,
                           Boolean   *continue_to_dispatch)
{
  XKeyEvent             *ev = (XKeyEvent *)event;
  DataKnob               k  = (DataKnob)   closure;
  matrix_vect_privrec_t *me = (matrix_vect_privrec_t *)k->privptr;

  KeySym                 ks;
  Modifiers              mr;

    XtTranslateKeycode(XtDisplay(w), ev->keycode, ev->state, &mr, &ks);

    if      (me->is_editing             &&
             (ev->state & ControlMask)  &&
             (ks == XK_Return  ||  ks == XK_KP_Enter  ||  ks == osfXK_SwitchDirection  ||  ks == osfXK_Activate))
    {
        if (w == me->nelems_w) NelsActivateCB(w, k, NULL);
        send_data(k);
    }
    else if (me->is_editing             &&
             (ev->state & (ShiftMask | ControlMask | Mod1Mask)) == 0  &&
             (ks == XK_Escape  ||  ks == osfXK_Cancel))
        StopEditing(k);
    else
        return;

    *continue_to_dispatch = False;
}

static int  CreateMatrixVect(DataKnob k, CxWidget parent)
{
  matrix_vect_privrec_t *me = (matrix_vect_privrec_t *)k->privptr;
  CxWidget               grid;
  int                    x0;
  int                    y0;
  int                    n;
  int                    n_w;
  char                   buf[10];
  XmString               s;
  int                    columns = GetTextColumns(k->u.v.dpyfmt, NULL) + 1;
  int                    nel_cols;
  Widget                 top_cell;

    me->cells_allocd = k->u.v.src.max_nelems;
    if (me->cells_allocd < 1) return -1;
    if (me->cells_allocd > MAX_CELLS) me->cells_allocd = MAX_CELLS;

    me->cells = (void *)(XtCalloc(me->cells_allocd, sizeof(me->cells[0])));
    if (me->cells == NULL) return -1;
    if (k->is_rw)
    {
        me->edata = (void *)(XtCalloc(me->cells_allocd, sizeof(me->edata[0])));
        if (me->edata == NULL)
        {
            XtFree((void *)(me->cells)); me->cells = NULL;
            return -1;
        }
    }

    x0 = me->number_cells? 1 : 0;
    y0 = me->no_nelems_w?  0 : 1;

    // A copy from MotifKnobs_grid_cont.c::GridMaker()
    k->w = grid = XhCreateGrid(parent, "grid");
    XhGridSetGrid   (grid, -1*0,     -1*0);
    XhGridSetSpacing(grid, /*me->hspc*/MOTIFKNOBS_INTERKNOB_H_SPACING, /*me->vspc*/MOTIFKNOBS_INTERKNOB_V_SPACING);
    XhGridSetPadding(grid,  1*0,      1*0);

    XhGridSetSize(grid, x0 + 1, y0 + me->cells_allocd);

    if (me->no_nelems_w == 0)
    {
        if      (me->cells_allocd < 10)   nel_cols = 1;
        else if (me->cells_allocd < 100)  nel_cols = 2;
        else if (me->cells_allocd < 1000) nel_cols = 3;
        else                              nel_cols = 4;

        if (k->is_rw)
        {
            top_cell = XtVaCreateManagedWidget("topCellForm",
                                               xmFormWidgetClass,
                                               CNCRTZE(grid),
                                               XmNshadowThickness, 0,
                                               NULL);
            // [nnn]
            me->nelems_w = MotifKnobs_MakeTextWidget(top_cell,      1, nel_cols + 1);
            MotifKnobs_SetTextCursorCallback(me->nelems_w);
            XtAddCallback(me->nelems_w, XmNactivateCallback,     NelsActivateCB, (XtPointer)k);
            XtAddCallback(me->nelems_w, XmNmodifyVerifyCallback, VectUserCB,     (XtPointer)k);
            XtAddCallback(me->nelems_w, XmNmotionVerifyCallback, VectUserCB,     (XtPointer)k);
            XtAddEventHandler(me->nelems_w, KeyPressMask,    False, VectKbdHandler,   (XtPointer)k);
            // [=]
            me->send_b   = XtVaCreateManagedWidget("push_i",
                                                   xmPushButtonWidgetClass,
                                                   top_cell,
                                                   XmNlabelString, s = XmStringCreateLtoR("=", xh_charset),
                                                   XmNtraversalOn, False,
                                                   XmNsensitive,   False,
                                                   NULL);
            XmStringFree(s);
            XtAddCallback(me->send_b,   XmNactivateCallback, SendCB,   k);
            attachleft(me->send_b,   me->nelems_w, MOTIFKNOBS_INTERKNOB_H_SPACING*0);
            MotifKnobs_TuneButtonKnob(k, me->send_b,   0);
            // [X]
            me->cancel_b = XtVaCreateManagedWidget("push_i",
                                                   xmPushButtonWidgetClass,
                                                   top_cell,
                                                   XmNlabelString, s = XmStringCreateLtoR("X", xh_charset),
                                                   XmNtraversalOn, False,
                                                   XmNsensitive,   False,
                                                   NULL);
            XmStringFree(s);
            XtAddCallback(me->cancel_b, XmNactivateCallback, CancelCB, k);
            attachleft(me->cancel_b, me->send_b,   MOTIFKNOBS_INTERKNOB_H_SPACING*0);
            MotifKnobs_TuneButtonKnob(k, me->cancel_b, 0);
        }
        else
        {
            top_cell =
            me->nelems_w = MotifKnobs_MakeTextWidget(CNCRTZE(grid), 0, nel_cols + 1);
        }
        XhGridSetChildPosition (top_cell, x0, 0);
        XhGridSetChildAlignment(top_cell, -1, 0);

        MotifKnobs_TuneTextKnob(k, me->nelems_w, 0);
    }

    for (n = 0;  n < me->cells_allocd;  n++)
    {
        if (me->number_cells)
        {
            sprintf(buf, "%d", n);
            n_w = XtVaCreateManagedWidget("rowlabel",
                                          xmLabelWidgetClass,
                                          CNCRTZE(grid),
                                          XmNlabelString, s = XmStringCreateLtoR(buf, xh_charset),
                                          NULL);
            XmStringFree(s);
            XhGridSetChildPosition (n_w, 0, y0 + n);
            XhGridSetChildAlignment(n_w, +1, 0);
        }

        me->cells[n] = MotifKnobs_MakeTextWidget(CNCRTZE(grid), k->is_rw, columns);
        XtSetSensitive (me->cells[n], 0);
        XmTextSetString(me->cells[n], "-");
        if (k->is_rw)
        {
            MotifKnobs_SetTextCursorCallback(me->cells[n]);
            XtAddCallback(me->cells[n], XmNactivateCallback,     CellActivateCB, (XtPointer)k);
            XtAddCallback(me->cells[n], XmNmodifyVerifyCallback, VectUserCB,     (XtPointer)k);
            XtAddCallback(me->cells[n], XmNmotionVerifyCallback, VectUserCB,     (XtPointer)k);
            XtAddEventHandler(me->cells[n], KeyPressMask,    False, VectKbdHandler,   (XtPointer)k);
        }
        XhGridSetChildPosition (me->cells[n], x0, y0 + n);

        MotifKnobs_TuneTextKnob(k, me->cells[n], 0);
    }

    MotifKnobs_DecorateKnob(k);

    return 0;
}

static void DestroyMatrixVect_m(DataKnob k)
{
  matrix_vect_privrec_t *me = (matrix_vect_privrec_t *)k->privptr;

    MotifKnobs_CommonDestroy_m(k);
    if (me->cells != NULL) XtFree((void *)(me->cells)); me->cells = NULL;
    if (me->edata != NULL) XtFree((void *)(me->edata)); me->edata = NULL;
}

static void MatrixVectColorize_m(DataKnob k, knobstate_t newstate)
{
  matrix_vect_privrec_t *me = (matrix_vect_privrec_t *)k->privptr;
  Pixel                  fg;
  Pixel                  bg;
  int                    n;

    if (!me->defcols_obtained)
    {
        XtVaGetValues(me->cells[0],
                      XmNforeground, &(me->inp_deffg),
                      XmNbackground, &(me->inp_defbg),
                      NULL);
        me->defcols_obtained = 1;
    }

    MotifKnobs_CommonColorize_m(k, newstate);

    MotifKnobs_ChooseColors(k->colz_style, newstate,
                            me->inp_deffg, me->inp_defbg, &fg, &bg);

    if (me->nelems_w != NULL)
        XtVaSetValues(me->nelems_w,
                      XmNforeground, fg,
                      XmNbackground, bg,
                      NULL);
    for (n = 0;  n < me->cells_allocd;  n++)
        XtVaSetValues(me->cells[n],
                      XmNforeground, fg,
                      XmNbackground, bg,
                      NULL);
}

static void MatrixSetVect_m(DataKnob k, const double *data, int nelems)
{
  matrix_vect_privrec_t *me = (matrix_vect_privrec_t *)k->privptr;
  char                   buf[400]; // So that 1e308 fits
  const char            *p;
  int                    n;

    if (nelems < 0)                nelems = 0;
    if (nelems > me->cells_allocd) nelems = me->cells_allocd;

    for (n = 0;  n < nelems;  n++)
    {
        p = snprintf_dbl_trim(buf, sizeof(buf), k->u.v.dpyfmt, data[n], k->is_rw);
        XmTextSetString(me->cells[n], p);
    }
    set_nelems(k, nelems, 0);
    if (me->edata != NULL) memcpy(me->edata, data, nelems * sizeof(data[0]));
    StopEditing(k);
}

dataknob_vect_vmt_t motifknobs_matrix_vect_vmt =
{
    {DATAKNOB_VECT, "matrix",
        sizeof(matrix_vect_privrec_t), text2matrixvectopts,
        0,
        CreateMatrixVect, DestroyMatrixVect_m,
        MatrixVectColorize_m, NULL},
    MatrixSetVect_m
};
