#include "Chl_includes.h"


enum {UPDATE_PERIOD_MS = 1000};


typedef bigvals_rec_t dlgrec_t;

static void CreateBigValsDialog(XhWindow  win, DataSubsys subsys, dlgrec_t *rec);

static dlgrec_t *ThisDialog(XhWindow win)
{
  chl_privrec_t *privrec = _ChlPrivOf(win);
  dlgrec_t      *rec     = &(privrec->bv);
  
    if (!rec->initialized) CreateBigValsDialog(win, privrec->subsys, rec);
    return rec;
}

//////////////////////////////////////////////////////////////////////

static void RenewBigvRow(dlgrec_t *rec, int  row, DataKnob  k)
{
  char     *label;
  XmString  s;

    rec->target[row] = k;

    label = k->label;
    if (label == NULL) label = k->ident;
    if (label == NULL) label = "";
    XtVaSetValues(rec->label[row],
                  XmNlabelString, s = XmStringCreateLtoR(label, xh_charset),
                  NULL);
    XmStringFree(s);
    
    XtVaSetValues(rec->val_dpy[row],
                  XmNcolumns, GetTextColumns(k->u.k.dpyfmt, k->u.k.units),
                  NULL);
    rec->colstate_s[row] = KNOBSTATE_UNINITIALIZED;
}

static void UpdateBigvRow(dlgrec_t *rec, int  row)
{
  DataKnob  k = rec->target[row];
  Pixel     fg;
  Pixel     bg;
    
    MotifKnobs_SetTextString(k, rec->val_dpy[row], k->u.k.curv);

    if (k->curstate != rec->colstate_s[row])
    {
        MotifKnobs_ChooseColors(k->colz_style, k->curstate,
                               rec->deffg, rec->defbg,
                               &fg,        &bg);
        XtVaSetValues(rec->val_dpy[row],
                      XmNforeground, fg,
                      XmNbackground, bg,
                      NULL);
        rec->colstate_s[row] = k->curstate;
    }
}

static void SetBigvCount(dlgrec_t *rec, int count)
{
  int  row;
    
    rec->rows_used = count;
    
    if (count > 0)
    {
        XtManageChildren(rec->label,   count);
        XtManageChildren(rec->val_dpy, count);
        XtManageChildren(rec->b_up,    count);
        XtManageChildren(rec->b_dn,    count);
        XtManageChildren(rec->b_rm,    count);
    }

    if (count < MAX_BIGVALS)
    {
        XtUnmanageChildren(rec->label   + count, MAX_BIGVALS - count);
        XtUnmanageChildren(rec->val_dpy + count, MAX_BIGVALS - count);
        XtUnmanageChildren(rec->b_up    + count, MAX_BIGVALS - count);
        XtUnmanageChildren(rec->b_dn    + count, MAX_BIGVALS - count);
        XtUnmanageChildren(rec->b_rm    + count, MAX_BIGVALS - count);
    }
    
    for (row = 0;  row < count;  row++)
    {
        XhGridSetChildPosition(rec->label  [row], 0, row);
        XhGridSetChildPosition(rec->val_dpy[row], 1, row);
        XhGridSetChildPosition(rec->b_up   [row], 2, row);
        XhGridSetChildPosition(rec->b_dn   [row], 3, row);
        XhGridSetChildPosition(rec->b_rm   [row], 4, row);
    }

    XhGridSetSize(rec->grid, 5, count);
}

static void DisplayBigVal(dlgrec_t *rec)
{
  int  row;
  
    for (row = 0;  row < rec->rows_used;  row++)
        UpdateBigvRow(rec, row);
}


static void BigUpCB(Widget     w,
                    XtPointer  closure,
                    XtPointer  call_data  __attribute__((unused)))
{
  dlgrec_t *rec = ThisDialog(XhWindowOf(w));
  int       row = ptr2lint(closure);
  DataKnob  k_this;
  DataKnob  k_prev;
  
    if (row == 0) return;
    k_this = rec->target[row];
    k_prev = rec->target[row - 1];
    RenewBigvRow (rec, row,     k_prev);
    RenewBigvRow (rec, row - 1, k_this);
    UpdateBigvRow(rec, row);
    UpdateBigvRow(rec, row - 1);
}

static void BigDnCB(Widget     w,
                    XtPointer  closure,
                    XtPointer  call_data  __attribute__((unused)))
{
  dlgrec_t *rec = ThisDialog(XhWindowOf(w));
  int       row = ptr2lint(closure);
  DataKnob  k_this;
  DataKnob  k_next;

    if (row == rec->rows_used - 1) return;
    k_this = rec->target[row];
    k_next = rec->target[row + 1];
    RenewBigvRow (rec, row,     k_next);
    RenewBigvRow (rec, row + 1, k_this);
    UpdateBigvRow(rec, row);
    UpdateBigvRow(rec, row + 1);
}

static void BigRmCB(Widget     w,
                    XtPointer  closure,
                    XtPointer  call_data  __attribute__((unused)))
{
  dlgrec_t *rec = ThisDialog(XhWindowOf(w));
  int       row = ptr2lint(closure);
  int       y;
  
    if (rec->rows_used == 1) XhStdDlgHide(rec->box);

    for (y = row;  y < rec->rows_used - 1;  y++)
    {
        RenewBigvRow (rec, y, rec->target[y + 1]);
        UpdateBigvRow(rec, y);
    }
    SetBigvCount(rec, rec->rows_used - 1);
}

static void BigvMouse3Handler(Widget     w,
                              XtPointer  closure,
                              XEvent    *event,
                              Boolean   *continue_to_dispatch)
{
  dlgrec_t            *rec = ThisDialog(XhWindowOf(w));
  int                  row = ptr2lint(closure);
  DataKnob             k   = rec->target[row];

    MotifKnobs_Mouse3Handler(w, (XtPointer)k, event, continue_to_dispatch);
}

static CxWidget MakeBigvButton(CxWidget grid, int x, int row,
                               const char *caption, XtCallbackProc  callback)
{
  CxWidget  b;
  XmString  s;
  
    b = XtVaCreateWidget("push_o", xmPushButtonWidgetClass, CNCRTZE(grid),
                         XmNtraversalOn, False,
                         XmNsensitive,   1,
                         XmNlabelString, s = XmStringCreateLtoR(caption, xh_charset),
                         NULL);
    XhGridSetChildPosition (b,  x,  row);
    XhGridSetChildFilling  (b,  0,  0);
    XhGridSetChildAlignment(b, -1, -1);
    XtAddCallback(b, XmNactivateCallback, callback, lint2ptr(row));

    return b;
}

//--------------------------------------------------------------------

static void Update1HZProc(XtPointer     closure,
                          XtIntervalId *id      __attribute__((unused)));

static void CreateBigValsDialog(XhWindow  win, DataSubsys subsys, dlgrec_t *rec)
{
  char        buf[100];
  const char *sysname = subsys->sysname;

  XmString    s;
  
  int         row;

    snprintf(buf, sizeof(buf),
             "%s: Big values", sysname != NULL? sysname : "???SYSNAME???");
    rec->box =
        CNCRTZE(XhCreateStdDlg(win, "bigv", buf, NULL, NULL,
                               XhStdDlgFOk));

    rec->grid = XhCreateGrid(rec->box, "grid");
    XhGridSetGrid   (rec->grid, 0,                       0);
    XhGridSetSpacing(rec->grid, /*CHL_INTERKNOB_H_SPACING*/0, /*CHL_INTERKNOB_V_SPACING*/0);
    XhGridSetPadding(rec->grid, 0,                       0);

    for (row = 0;  row < MAX_BIGVALS;  row++)
    {
        rec->label[row] =
            XtVaCreateWidget("rowlabel", xmLabelWidgetClass, rec->grid,
                             XmNlabelString, s = XmStringCreateLtoR("", xh_charset),
                             NULL);
        XmStringFree(s);
        XhGridSetChildPosition (rec->label[row],  0,  row);
        XhGridSetChildFilling  (rec->label[row],  0,  0);
        XhGridSetChildAlignment(rec->label[row], -1, -1);

        rec->val_dpy[row] =
            XtVaCreateWidget("text_o", xmTextWidgetClass, rec->grid,
                             XmNscrollHorizontal,      False,
                             XmNcursorPositionVisible, False,
                             XmNeditable,              False,
                             XmNtraversalOn,           False,
                             NULL);
        XhGridSetChildPosition (rec->val_dpy[row],  1,  row);
        XhGridSetChildFilling  (rec->val_dpy[row],  0,  0);
        XhGridSetChildAlignment(rec->val_dpy[row], -1, -1);
        
        XtInsertEventHandler(rec->label  [row], ButtonPressMask, False, BigvMouse3Handler, lint2ptr(row), XtListHead);
        XtInsertEventHandler(rec->val_dpy[row], ButtonPressMask, False, BigvMouse3Handler, lint2ptr(row), XtListHead);

        rec->b_up[row] = MakeBigvButton(rec->grid, 2, row, "^", BigUpCB);
        rec->b_dn[row] = MakeBigvButton(rec->grid, 3, row, "v", BigDnCB);
        rec->b_rm[row] = MakeBigvButton(rec->grid, 4, row, "-", BigRmCB);
    }

    XtVaGetValues(rec->val_dpy[0],
                  XmNforeground, &(rec->deffg),
                  XmNbackground, &(rec->defbg),
                  NULL);

    rec->initialized = 1;
    
    rec->upd_timer = XtAppAddTimeOut(xh_context, UPDATE_PERIOD_MS, Update1HZProc, (XtPointer)rec);
}

//////////////////////////////////////////////////////////////////////

void _ChlShowBigVal_m(DataKnob k)
{
  XhWindow    win = XhWindowOf(k->w);
  dlgrec_t   *rec = ThisDialog(win);

  int         row;

    /* Check if this knob is alreany in a list */
    for (row = 0;
         row < rec->rows_used  &&  rec->target[row] != k;
         row++);
  
    if (row >= rec->rows_used)
    {
    
        if (rec->rows_used >= MAX_BIGVALS)
        {
            for (row = 0;  row < rec->rows_used - 1;  row++)
            {
                RenewBigvRow (rec, row, rec->target[row + 1]);
                UpdateBigvRow(rec, row);
            }
            rec->rows_used--;
        }

        row = rec->rows_used;

        RenewBigvRow (rec, row, k);
        UpdateBigvRow(rec, row);
        SetBigvCount (rec, row + 1);
    }
    
    XhStdDlgShow(rec->box);
}

static void Update1HZProc(XtPointer     closure,
                          XtIntervalId *id      __attribute__((unused)))
{
  dlgrec_t   *rec = (dlgrec_t *)closure;

    rec->upd_timer = XtAppAddTimeOut(xh_context, UPDATE_PERIOD_MS, Update1HZProc, (XtPointer)rec);
    if (XtIsManaged(rec->box)) DisplayBigVal(rec);
}
