#include <stdio.h>

#include <Xm/Xm.h>
#include <Xm/PushB.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/ScrollBar.h>

#include "Xh.h"
#include "Xh_fallbacks.h"
#include "Xh_globals.h"
#include "Xh_utils.h"
#include "Xh_viewport.h"


//// application-independent staff ///////////////////////////////////

static GC AllocPixGC(Widget w, CxPixel pix)
{
  XtGCMask   mask;  
  XGCValues  vals;
    
    mask = GCForeground | GCLineWidth;
    vals.foreground = pix;
    vals.line_width = 0;

    return XtGetGC(w, mask, &vals);
}

//////////////////////////////////////////////////////////////////////

static void DrawView(Xh_viewport_t *vp, int do_clear)
{
  Display   *dpy = XtDisplay(vp->view);
  Window     win = XtWindow (vp->view);

  Dimension  view_w;
  Dimension  view_h;

    if (win == 0) return; // A guard for resize-called-before-realized

    if (do_clear)
    {
        XtVaGetValues(vp->view, XmNwidth, &view_w, XmNheight, &view_h, NULL);
        XFillRectangle(dpy, win, vp->bkgdGC, 0, 0, view_w, view_h);
    }

    vp->draw(vp, XH_VIEWPORT_VIEW);
}
 
static void DrawAxis(Xh_viewport_t *vp, int do_clear)
{
  Display   *dpy = XtDisplay(vp->axis);
  Window     win = XtWindow (vp->axis);

  Dimension  axis_w;
  Dimension  axis_h;

    if (win == 0) return; // A guard for resize-called-before-realized

    if (do_clear)
    {
        XtVaGetValues(vp->axis, XmNwidth, &axis_w, XmNheight, &axis_h, NULL);
        XFillRectangle(dpy, win, vp->bkgdGC, 0, 0, axis_w, axis_h);
    }

    vp->draw(vp, XH_VIEWPORT_AXIS);
}

static int  SetHorzbarParams(Xh_viewport_t *vp)
{
  int  ret = 0;

  Dimension  v_span;
  int        n_maximum;
  int        n_slidersize;
  int        n_value;

    if (vp->horzbar == NULL) return ret;

    XtVaGetValues(vp->view, XmNwidth, &v_span, NULL);

    n_maximum = vp->horz_maximum;
    if (n_maximum < v_span) n_maximum = v_span;

    n_slidersize = v_span;

    if (n_maximum    == vp->o_horzbar_maximum  &&
        n_slidersize == vp->o_horzbar_slidersize)
        return ret;
    vp->o_horzbar_maximum    = n_maximum;
    vp->o_horzbar_slidersize = n_slidersize;

    XtVaGetValues(vp->horzbar, XmNvalue, &n_value, NULL);
    if (n_value > n_maximum - n_slidersize)
    {
        vp->horz_offset = n_value = n_maximum - n_slidersize;
        ret = 1;
    }

    XtVaSetValues(vp->horzbar,
                  XmNminimum,       0,
                  XmNmaximum,       n_maximum,
                  XmNsliderSize,    n_slidersize,
                  XmNvalue,         n_value,
                  XmNincrement,     1,
                  XmNpageIncrement, /*n_maximum / 10*/v_span,
                  NULL);

    return ret;
}

static int  SetVertbarParams(Xh_viewport_t *vp)
{
  int        ret = 0;

    if (vp->vertbar == NULL) return ret;

    return ret;
}

//////////////////////////////////////////////////////////////////////

static void CpanelOffCB(Widget     w         __attribute__((unused)),
                        XtPointer  closure,
                        XtPointer  call_data __attribute__((unused)))
{
  Xh_viewport_t *vp = closure;

    XhViewportSetCpanelState(vp, 0);
}

static void CpanelOnCB (Widget     w         __attribute__((unused)),
                        XtPointer  closure,
                        XtPointer  call_data __attribute__((unused)))
{
  Xh_viewport_t *vp = closure;

    XhViewportSetCpanelState(vp, 1);
}

static void ViewExposureCB(Widget     w,
                           XtPointer  closure,
                           XtPointer  call_data)
{
  Xh_viewport_t *vp = closure;

    if (vp->ignore_view_expose) return;
    XhRemoveExposeEvents(ABSTRZE(w));
    DrawView(vp, False);
}

static void AxisExposureCB(Widget     w,
                           XtPointer  closure,
                           XtPointer  call_data)
{
  Xh_viewport_t *vp = closure;

    if (vp->ignore_axis_expose) return;
    XhRemoveExposeEvents(ABSTRZE(w));
    DrawAxis(vp, False);
}

static void ViewResizeCB(Widget     w,
                         XtPointer  closure,
                         XtPointer  call_data)
{
  Xh_viewport_t *vp = closure;

    XhAdjustPreferredSizeInForm(ABSTRZE(vp->view));
    XhAdjustPreferredSizeInForm(ABSTRZE(vp->axis)); 
    if (vp->horzbar != NULL) XhAdjustPreferredSizeInForm(ABSTRZE(vp->horzbar));
    if (vp->vertbar != NULL) XhAdjustPreferredSizeInForm(ABSTRZE(vp->vertbar));

    if (XhCompressConfigureEvents(ABSTRZE(w)) == 0)
    {
        vp->ignore_view_expose = 0;
        SetHorzbarParams(vp);
        SetVertbarParams(vp);
        DrawView(vp, True); 
    }
    else
        vp->ignore_view_expose = 1;
}

static void AxisResizeCB(Widget     w,
                         XtPointer  closure,
                         XtPointer  call_data)
{
  Xh_viewport_t *vp = closure;

    if (XhCompressConfigureEvents(ABSTRZE(w)) == 0)
    {
        vp->ignore_axis_expose = 0;
        DrawAxis(vp, True);
    }
    else
        vp->ignore_axis_expose = 1;
}

static void HorzScrollCB(Widget     w,
                         XtPointer  closure,
                         XtPointer  call_data)
{
  Xh_viewport_t *vp = closure;

  XmScrollBarCallbackStruct *info = (XmScrollBarCallbackStruct *)call_data;

    vp->horz_offset = info->value;
    DrawView(vp, True);
    DrawAxis(vp, True);
}

static void VertScrollCB(Widget     w,
                         XtPointer  closure,
                         XtPointer  call_data)
{
  Xh_viewport_t *vp = closure;

  XmScrollBarCallbackStruct *info = (XmScrollBarCallbackStruct *)call_data;

}

static void PointerHandler(Widget     w,
                           XtPointer  closure,
                           XEvent    *event,
                           Boolean   *continue_to_dispatch)
{
  Xh_viewport_t *vp = closure;

  XMotionEvent      *mev = (XMotionEvent      *) event;
  XButtonEvent      *bev = (XButtonEvent      *) event;
  XCrossingEvent    *cev = (XEnterWindowEvent *) event;

  int                ev_type;
  int                state;
  int                mod_mask;
  int                bn;
  int                x;
  int                y;

    if (vp->msev == NULL) return;

    /* Get parameters of this event */
    bn = -1;
    if      (event->type == MotionNotify)
    {
        ev_type = XH_VIEWPORT_MSEV_MOVE;
        state   = mev->state;
        x       = mev->x;
        y       = mev->y;
    }
    else if (event->type == ButtonPress  ||  event->type == ButtonRelease)
    {
        ev_type = event->type == ButtonPress? XH_VIEWPORT_MSEV_PRESS : XH_VIEWPORT_MSEV_RELEASE;
        state   = bev->state;
        x       = bev->x;
        y       = bev->y;
        bn      = bev->button - Button1;
    }
    else if (event->type == EnterNotify  ||  event->type == LeaveNotify)
    {
        ev_type = event->type == EnterNotify? XH_VIEWPORT_MSEV_ENTER : XH_VIEWPORT_MSEV_LEAVE;
        state   = cev->state;
        x       = cev->x;
        y       = cev->y;
    }
    else
    {
        return;
    }

    mod_mask = 0;
    if (state & ShiftMask)   mod_mask |= XH_VIEWPORT_MSEV_MASK_SHIFT;
    if (state & ControlMask) mod_mask |= XH_VIEWPORT_MSEV_MASK_CTRL;
    if (state & Mod1Mask)    mod_mask |= XH_VIEWPORT_MSEV_MASK_ALT;

    vp->msev(vp, ev_type, mod_mask, bn, x, y);
}

//////////////////////////////////////////////////////////////////////

static
void SetAttachs          (Xh_viewport_t *vp);

int  XhViewportCreate    (Xh_viewport_t *vp, Widget parent, 
                          void *privptr,
                          Xh_viewport_draw_t draw, Xh_viewport_msev_t msev,
                          int parts,
                          int init_w, int init_h, int bg_idx,
                          int m_lft, int m_rgt, int m_top, int m_bot)
{
  int       cpanel_loc = parts & XH_VIEWPORT_CPANEL_LOC_MASK;
  Widget    at_l, at_r, at_t, at_b;
  XmString  s;
  CxPixel   bg = XhGetColor(bg_idx);

    bzero(vp, sizeof(*vp));

    vp->privptr = privptr;
    vp->draw    = draw;
    vp->msev    = msev;

    vp->m_lft = m_lft;  vp->m_rgt = m_rgt;
    vp->m_top = m_top;  vp->m_bot = m_bot;

    vp->bkgdGC = AllocPixGC(parent, XhGetColor(bg_idx));

    /* Create containers */
    vp->container  = XtVaCreateManagedWidget("container", xmFormWidgetClass,
                                             parent,
                                             XmNshadowThickness, 0,
                                             NULL);
    if (parts & XH_VIEWPORT_CPANEL)
    {
        vp->sidepanel = XtVaCreateManagedWidget("sidepanel", xmFormWidgetClass,
                                                vp->container,
                                                XmNshadowThickness, 0,
                                                NULL);
        vp->cpanel = XtVaCreateManagedWidget   ("cpanel", xmFormWidgetClass,
                                                vp->sidepanel,
                                                XmNshadowThickness, 0,
                                                NULL);
    }
    vp->gframe     = XtVaCreateManagedWidget("gframe", xmFrameWidgetClass,
                                             vp->container,
                                             XmNshadowType,      XmSHADOW_IN,
                                             XmNshadowThickness, 1,
                                             XmNtraversalOn,     False,
                                             NULL);
    vp->gform      = XtVaCreateManagedWidget("gform", xmFormWidgetClass,
                                             vp->gframe,
                                             XmNbackground,      bg,
                                             XmNshadowThickness, 0,
                                             NULL);

    /* Create drawables... */
    /* Note:
           Here we use the fact that widgets are displayed top-to-bottom
           in the order of creation: 'view' is displayed above 'axis'
           because it is created earlier.
           And [-]/[+] buttons are even more early.
    */
    if (vp->cpanel != NULL  &&
        (parts & XH_VIEWPORT_NOFOLD) == 0)
    {
        vp->btn_cpanel_off = 
            XtVaCreateManagedWidget("push_i", xmPushButtonWidgetClass,
                                    vp->gform,
                                    XmNlabelString, s = XmStringCreateLtoR("-", xh_charset),
                                    XtVaTypedArg, XmNfontList,
                                        XmRString,
                                        XH_TINY_FIXED_FONT,
                                        strlen(XH_TINY_FIXED_FONT) + 1,
                                    XmNtraversalOn, False,
                                    NULL);
        XmStringFree(s);
        XtAddCallback(vp->btn_cpanel_off, XmNactivateCallback, CpanelOffCB, vp);
        vp->btn_cpanel_on = 
            XtVaCreateWidget       ("push_i", xmPushButtonWidgetClass,
                                    vp->gform,
                                    XmNlabelString, s = XmStringCreateLtoR("+", xh_charset),
                                    XtVaTypedArg, XmNfontList,
                                        XmRString,
                                        XH_TINY_FIXED_FONT,
                                        strlen(XH_TINY_FIXED_FONT) + 1,
                                    XmNtraversalOn, False,
                                    NULL);
        XmStringFree(s);
        XtAddCallback(vp->btn_cpanel_on,  XmNactivateCallback, CpanelOnCB,  vp);
    }
    vp->angle = XtVaCreateWidget("angle", xmFormWidgetClass,
                                 vp->gform,
                                 XmNbackground,  bg*0+1*XhGetColor(XH_COLOR_JUST_GREEN),
                                 NULL);
    vp->view = XtVaCreateManagedWidget("plot_graph", xmDrawingAreaWidgetClass,
                                       vp->gform,
                                       XmNbackground,  bg*1+0*XhGetColor(XH_COLOR_JUST_GREEN),
                                       XmNwidth,       init_w,
                                       XmNheight,      init_h,
                                       NULL);
    XtAddCallback(vp->view, XmNexposeCallback, ViewExposureCB, vp);
    XtAddCallback(vp->view, XmNresizeCallback, ViewResizeCB,   vp);
    vp->axis = XtVaCreateManagedWidget("plot_axis", xmDrawingAreaWidgetClass,
                                       vp->gform,
                                       XmNbackground,  bg,
                                       XmNwidth,       m_lft + init_w + m_rgt,
                                       XmNheight,      m_top + init_h + m_bot,
                                       NULL);
    XtAddCallback(vp->axis,  XmNexposeCallback, AxisExposureCB,  vp);
    XtAddCallback(vp->axis,  XmNresizeCallback, AxisResizeCB,    vp);
    XtAddEventHandler(vp->view,
                      EnterWindowMask | LeaveWindowMask | PointerMotionMask |
                      ButtonPressMask | ButtonReleaseMask |
                      Button1MotionMask | Button2MotionMask | Button3MotionMask,
                      False, PointerHandler, vp);

    /* Scrollbars, if required */
    if (parts & XH_VIEWPORT_HORZBAR)
    {
        vp->horzbar = XtVaCreateManagedWidget("horzbar", xmScrollBarWidgetClass,
                                              vp->gform,
                                              XmNbackground,         bg,
                                              XmNorientation,        XmHORIZONTAL,
                                              XmNwidth,              20,
                                              XmNhighlightThickness, 0,
                                              NULL);
        SetHorzbarParams(vp);
        XtAddCallback(vp->horzbar, XmNdragCallback,         HorzScrollCB, vp);
        XtAddCallback(vp->horzbar, XmNvalueChangedCallback, HorzScrollCB, vp);
    }
    if (parts & XH_VIEWPORT_VERTBAR)
    {
        vp->vertbar = XtVaCreateManagedWidget("vertbar", xmScrollBarWidgetClass,
                                              vp->gform,
                                              XmNbackground,         bg,
                                              XmNorientation,        XmVERTICAL,
                                              XmNheight,             20,
                                              XmNhighlightThickness, 0,
                                              NULL);
        SetVertbarParams(vp);
        XtAddCallback(vp->vertbar, XmNdragCallback,         VertScrollCB, vp);
        XtAddCallback(vp->vertbar, XmNvalueChangedCallback, VertScrollCB, vp);
    }

    /* Perform data-display/control-panel inter-attachments */
    at_l = at_r = at_t = at_b = NULL;
    if (vp->cpanel != NULL)
    {
        if      (cpanel_loc == XH_VIEWPORT_CPANEL_AT_LEFT)
        {
            at_l = vp->sidepanel;
            attachleft  (at_l, NULL, 0);

            if ((parts & XH_VIEWPORT_NOFOLD) == 0)
            {
                attachleft  (vp->btn_cpanel_off, NULL, 0);
                attachtop   (vp->btn_cpanel_off, NULL, 0);
                attachleft  (vp->btn_cpanel_on,  NULL, 0);
                attachtop   (vp->btn_cpanel_on,  NULL, 0);
            }
        }
        else if (cpanel_loc == XH_VIEWPORT_CPANEL_AT_RIGHT)
        {
            at_r = vp->sidepanel;
            attachright (at_r, NULL, 0);

            if ((parts & XH_VIEWPORT_NOFOLD) == 0)
            {
                attachright (vp->btn_cpanel_off, NULL, 0);
                attachtop   (vp->btn_cpanel_off, NULL, 0);
                attachright (vp->btn_cpanel_on,  NULL, 0);
                attachtop   (vp->btn_cpanel_on,  NULL, 0);
            }
        }
        else if (cpanel_loc == XH_VIEWPORT_CPANEL_AT_TOP)
        {
            at_t = vp->sidepanel;
            attachtop   (at_t, NULL, 0);

            if ((parts & XH_VIEWPORT_NOFOLD) == 0)
            {
                attachleft  (vp->btn_cpanel_off, NULL, 0);
                attachtop   (vp->btn_cpanel_off, NULL, 0);
                attachleft  (vp->btn_cpanel_on,  NULL, 0);
                attachtop   (vp->btn_cpanel_on,  NULL, 0);
            }
        }
        else                /* XH_VIEWPORT_CPANEL_AT_BOTTOM */
        {
            at_b = vp->sidepanel;
            attachbottom(at_b, NULL, 0);

            if ((parts & XH_VIEWPORT_NOFOLD) == 0)
            {
                attachleft  (vp->btn_cpanel_off, NULL, 0);
                attachbottom(vp->btn_cpanel_off, NULL, 0);
                attachleft  (vp->btn_cpanel_on,  NULL, 0);
                attachbottom(vp->btn_cpanel_on,  NULL, 0);
            }
        }
    }
    if      (cpanel_loc == XH_VIEWPORT_CPANEL_AT_LEFT)
    {
        attachleft  (vp->angle, NULL, 0);
        attachbottom(vp->angle, NULL, 0);
    }
    else if (cpanel_loc == XH_VIEWPORT_CPANEL_AT_RIGHT)
    {
        attachright (vp->angle, NULL, 0);
        attachbottom(vp->angle, NULL, 0);
    }
    else if (cpanel_loc == XH_VIEWPORT_CPANEL_AT_TOP)
    {
        attachright (vp->angle, NULL, 0);
        attachtop   (vp->angle, NULL, 0);
    }
    else                /* XH_VIEWPORT_CPANEL_AT_BOTTOM */
    {
        attachright (vp->angle, NULL, 0);
        attachbottom(vp->angle, NULL, 0);
    }
    
    attachleft  (vp->gframe, at_l, 0);
    attachright (vp->gframe, at_r, 0);
    attachtop   (vp->gframe, at_t, 0);
    attachbottom(vp->gframe, at_b, 0);

    /* And set data-display sizes/attachments */
    SetAttachs(vp);

    return 0;
}


/* The following duo should be kept in sync */

static
void SetAttachs          (Xh_viewport_t *vp)
{
    if (vp->horzbar != NULL)
    {
        attachleft  (vp->horzbar, NULL, vp->m_lft);
        attachright (vp->horzbar, NULL, vp->m_rgt);
        attachbottom(vp->horzbar, NULL, 0);
    }

    if (vp->vertbar != NULL)
    {
    }

    attachleft  (vp->view, NULL,        vp->m_lft);
    attachright (vp->view, NULL,        vp->m_rgt);
    attachtop   (vp->view, NULL,        vp->m_top);
    attachbottom(vp->view, vp->horzbar, vp->m_bot);

    attachleft  (vp->axis, NULL,        0);
    attachright (vp->axis, NULL,        0);
    attachtop   (vp->axis, NULL,        0);
    attachbottom(vp->axis, vp->horzbar, 0);
}

void XhViewportSetMargins(Xh_viewport_t *vp, 
                          int m_lft, int m_rgt, int m_top, int m_bot)
{
  Dimension  bw, bh;
  Dimension  aw, ah;
  Dimension  fw, fh;

    vp->m_lft = m_lft;  vp->m_rgt = m_rgt;
    vp->m_top = m_top;  vp->m_bot = m_bot;

    XtVaGetValues(vp->view,
                  XmNwidth,  &bw,
                  XmNheight, &bh,
                  NULL);

    XtVaSetValues(vp->view,
                  XmNleftOffset,   vp->m_lft,
                  XmNrightOffset,  vp->m_rgt,
                  XmNtopOffset,    vp->m_top,
                  XmNbottomOffset, vp->m_bot,
                  NULL);

    XtVaGetValues(vp->view,
                  XmNwidth,  &aw,
                  XmNheight, &ah,
                  NULL);

    if (aw != bw)
    {
        XtVaGetValues(XtParent(vp->view),
                      XmNwidth,  &fw,
                      XmNheight, &fh,
                      NULL);
        XtVaSetValues(XtParent(vp->view),
                      XmNwidth, fw - (aw - bw),
                      NULL);
        XtVaSetValues(vp->view,
                      XmNwidth,  bw,
                      NULL);
    };

    if (vp->horzbar != NULL)
        XtVaSetValues(vp->horzbar,
                      XmNleftOffset,  vp->m_lft,
                      XmNrightOffset, vp->m_rgt,
                      NULL);
}

int  XhViewportSetHorzbarParams(Xh_viewport_t *vp)
{
    return SetHorzbarParams(vp);
}

void XhViewportUpdate    (Xh_viewport_t *vp, int parts)
{
    DrawView(vp, True);
    if (parts & XH_VIEWPORT_AXIS)   DrawAxis(vp, True);
    if (parts & XH_VIEWPORT_STATS)  vp->draw(vp, XH_VIEWPORT_STATS);
}

void XhViewportSetCpanelState(Xh_viewport_t *vp, int state)
{
    if (vp->cpanel == NULL) return;

    if (state == 0)
    {
        XtUnmanageChild                                (vp->cpanel);
        if (vp->btn_cpanel_off != NULL) XtUnmanageChild(vp->btn_cpanel_off);
        if (vp->btn_cpanel_on  != NULL) XtManageChild  (vp->btn_cpanel_on);
    }
    else
    {
        XtManageChild                                  (vp->cpanel);
        if (vp->btn_cpanel_off != NULL) XtManageChild  (vp->btn_cpanel_off);
        if (vp->btn_cpanel_on  != NULL) XtUnmanageChild(vp->btn_cpanel_on);
    }
}

void XhViewportSetBG     (Xh_viewport_t *vp, CxPixel bg)
{
  GC  new_bkgdGC;

    new_bkgdGC = AllocPixGC(XtParent(vp->container), bg);
    if (new_bkgdGC != NULL)
    {
        XtReleaseGC(XtParent(vp->container), vp->bkgdGC);
        vp->bkgdGC = new_bkgdGC;
    }
    
    XtVaSetValues(vp->gform, XmNbackground, bg, NULL);
    XtVaSetValues(vp->view,  XmNbackground, bg, NULL);
    XtVaSetValues(vp->axis,  XmNbackground, bg, NULL);
    if (vp->horzbar != NULL)
        XtVaSetValues(vp->horzbar, XmNbackground, bg, NULL);
    if (vp->vertbar != NULL)
        XtVaSetValues(vp->vertbar, XmNbackground, bg, NULL);
}
