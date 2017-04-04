#ifndef __PZFRAME_CPANEL_DECOR_H
#define __PZFRAME_CPANEL_DECOR_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/PushB.h>
#include <Xm/Form.h>
#include <Xm/Label.h>

#include "Xh.h"
#include "Xh_globals.h"

#include "Knobs.h"
#include "MotifKnobsP.h"
//#include "ChlI.h"


static void ShowHideSubwin(Widget box)
{
    if (XtIsManaged (box))
        XhStdDlgHide(ABSTRZE(box));
    else
        XhStdDlgShow(ABSTRZE(box));
}

static void SubwinCloseCB(Widget     w         __attribute__((unused)),
                          XtPointer  closure,
                          XtPointer  call_data __attribute__((unused)))
{
}

static void SubwinActivateCB(Widget     w         __attribute__((unused)),
                             XtPointer  closure,
                             XtPointer  call_data __attribute__((unused)))
{
  Widget               box = (Widget)                closure;

    ShowHideSubwin(box);
}

static void SubwinMouse3Handler(Widget     w  __attribute__((unused)),
                                XtPointer  closure,
                                XEvent    *event,
                                Boolean   *continue_to_dispatch)
{
  Widget               box = (Widget)                closure;
  XButtonPressedEvent *ev  = (XButtonPressedEvent *) event;

    if (ev->button != Button3) return;
    *continue_to_dispatch = False;
    //XmProcessTraversal(rec->btn, XmTRAVERSE_CURRENT);
    ShowHideSubwin(box);
}

static Widget MakeSubwin(Widget parent_grid, int grid_x, int grid_y,
                         const char *caption, const char *title)
{
  Widget    btn;
  Widget    box;
  XmString  s;

    btn = XtVaCreateManagedWidget("push_i",
                                  xmPushButtonWidgetClass,
                                  parent_grid,
                                  XmNtraversalOn, True,
                                  XmNlabelString, s = XmStringCreateLtoR(caption, xh_charset),
                                  NULL);
    XmStringFree(s);

    box = CNCRTZE(XhCreateStdDlg(XhWindowOf(ABSTRZE(parent_grid)), "-", title,
                                 "Close", NULL, XhStdDlgFOk | XhStdDlgFNoDefButton));

    XtAddCallback    (btn, XmNactivateCallback,    SubwinActivateCB,    box);
    XtAddEventHandler(btn, ButtonPressMask, False, SubwinMouse3Handler, box);
    XtAddCallback(XtParent(box), XmNpopdownCallback, SubwinCloseCB, NULL);

    XhGridSetChildPosition (ABSTRZE(btn),  grid_x,  grid_y);
    XhGridSetChildAlignment(ABSTRZE(btn),      -1,      -1);
    XhGridSetChildFilling  (ABSTRZE(btn),       0,       0);

    return box;
}

static Widget MakeAForm(Widget parent_grid, int grid_x, int grid_y)
{
  Widget  w;
    
    w = XtVaCreateManagedWidget("form", xmFormWidgetClass, parent_grid,
                                XmNshadowThickness, 0,
                                NULL);
    XhGridSetChildPosition (ABSTRZE(w),  grid_x,  grid_y);
    XhGridSetChildAlignment(ABSTRZE(w),      -1,      -1);
    XhGridSetChildFilling  (ABSTRZE(w),       0,       0);

    return w;
}

static void MakeALabel(Widget  parent_grid, int grid_x, int grid_y,
                       const char *caption)
{
  Widget    w;
  XmString  s;

    w = XtVaCreateManagedWidget("rowlabel", xmLabelWidgetClass, parent_grid,
                                XmNlabelString, s = XmStringCreateLtoR(caption, xh_charset),
                                NULL);
    XmStringFree(s);
  
    XhGridSetChildPosition (ABSTRZE(w), grid_x, grid_y);
    XhGridSetChildAlignment(ABSTRZE(w), -1,     -1);
    XhGridSetChildFilling  (ABSTRZE(w), 0,      0);
}

static DataKnob MakeAKnob(Widget parent_grid, int grid_x, int grid_y,
                          const char *spec,
                          simpleknob_cb_t  cb, void *privptr)
{
  DataKnob  k;

    k = CreateSimpleKnob(spec, 0, ABSTRZE(parent_grid), cb, privptr);
    if (k == NULL) return NULL;

    XhGridSetChildPosition (GetKnobWidget(k), grid_x, grid_y);
    XhGridSetChildAlignment(GetKnobWidget(k), -1,     -1);
    XhGridSetChildFilling  (GetKnobWidget(k), 0,      0);

    return k;
}


static Widget MakeStdCtl (Widget parent_grid, int grid_x, int grid_y,
                          pzframe_gui_t           *gui,
                          pzframe_gui_mkstdctl_t   mkstdctl,
                          int kind, int a, int b)
{
  Widget  w;
  
    w = mkstdctl(gui, parent_grid, kind, a, b);
    if (w == NULL) return NULL;
    
    XhGridSetChildPosition (ABSTRZE(w), grid_x, grid_y);
    XhGridSetChildAlignment(ABSTRZE(w), -1,     -1);
    XhGridSetChildFilling  (ABSTRZE(w), 0,      0);

    return w;
}

static Widget MakeParKnob(Widget parent_grid, int grid_x, int grid_y,
                          const char              *spec,
                          pzframe_gui_t           *gui,
                          pzframe_gui_mkparknob_t  mkparknob, int pn)
{
  Widget  w;

    w = mkparknob(gui, parent_grid, spec, pn);
    if (w == NULL) return NULL;
    
    XhGridSetChildPosition (ABSTRZE(w), grid_x, grid_y);
    XhGridSetChildAlignment(ABSTRZE(w), -1,     -1);
    XhGridSetChildFilling  (ABSTRZE(w), 0,      0);

    return w;
}


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __PZFRAME_CPANEL_DECOR_H */
