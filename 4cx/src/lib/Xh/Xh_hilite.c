#include "Xh_includes.h"

enum {HILITE_THK = 3};
#ifndef USE_OVERRIDE
  #define USE_OVERRIDE 1
#endif

void XhCreateHilite(XhWindow window)
{
  int  n;
    
    for (n = 0;  n < countof(window->hilites);  n++)
        window->hilites[n] =
#if USE_OVERRIDE
            XtVaCreatePopupShell("hilite",
                                 overrideShellWidgetClass,
                                 CNCRTZE(XhGetWindowShell(window)),
                                 XmNbackground,  XhGetColor(XH_COLOR_JUST_GREEN),
                                 XmNborderWidth, 0,
                                 XmNsensitive,   True,
                                 NULL);
#else
            XtVaCreateManagedWidget("hilite", widgetClass, window->mainForm,
                                    XmNsensitive,      False,
                                    XmNborderWidth,    0,
                                    XmNbackground,     XhGetColor(XH_COLOR_JUST_GREEN),
                                    XmNleftAttachment, XmATTACH_FORM,
                                    XmNtopAttachment,  XmATTACH_FORM,
                                    NULL);
#endif
}

#if !USE_OVERRIDE
static int IsAncestorWidgetOf(Widget top, Widget w)
{
    while (w != NULL)
    {
        if (top == w) return 1;
        
        w = XtParent(w);
    }

    return 0;
}
#endif

void XhShowHilite(CxWidget w)
{
  Widget    xw     = CNCRTZE(w);
  XhWindow  window;
  Position  wx, wy;
  Dimension ww, wh;
  Position  rx, ry;
  Position  fx, fy;
  int       n;
  
    if (w == NULL) return;
    window = XhWindowOf(w);
  
    if (window == NULL)                            return;
    if (window->hilites[0] == NULL)                return;
#if !USE_OVERRIDE
    if (!IsAncestorWidgetOf(window->mainForm, xw)) return;
#endif
    
    if (window->hilite_shown)
    {
#if USE_OVERRIDE
        for (n = 0;  n < countof(window->hilites);  n++)
            XtPopdown(window->hilites[n]);
#else
        XtUnmanageChildren(window->hilites, countof(window->hilites));
#endif
    }

    XtVaGetValues(xw, XmNwidth, &ww, XmNheight, &wh, NULL);
    if (ww == 0  ||  wh == 0) return;

#if USE_OVERRIDE
    XtTranslateCoords(xw, 0, 0, &rx, &ry);

    XtVaSetValues(window->hilites[0],
                  XmNx,      rx - HILITE_THK,
                  XmNy,      ry - HILITE_THK,
                  XmNwidth,  ww + HILITE_THK * 2,
                  XmNheight, HILITE_THK,
                  NULL);
    XtVaSetValues(window->hilites[1],
                  XmNx,      rx - HILITE_THK,
                  XmNy,      ry + wh,
                  XmNwidth,  ww + HILITE_THK * 2,
                  XmNheight, HILITE_THK,
                  NULL);
    XtVaSetValues(window->hilites[2],
                  XmNx,      rx - HILITE_THK,
                  XmNy,      ry,
                  XmNwidth,  HILITE_THK,
                  XmNheight, wh,
                  NULL);
    XtVaSetValues(window->hilites[3],
                  XmNx,      rx + ww,
                  XmNy,      ry,
                  XmNwidth,  HILITE_THK,
                  XmNheight, wh,
                  NULL);

    for (n = 0;  n < countof(window->hilites);  n++)
        XtPopup(window->hilites[n], XtGrabNone);
#else
    XtTranslateCoords(xw,               0, 0, &rx, &ry);
    XtTranslateCoords(window->mainForm, 0, 0, &fx, &fy);
    wx = rx - fx;
    wy = ry - fy;
    
    XtVaSetValues(window->hilites[0],
                  XmNleftOffset, wx - HILITE_THK,
                  XmNtopOffset,  wy - HILITE_THK,
                  XmNwidth,      ww + HILITE_THK * 2,
                  XmNheight,     HILITE_THK,
                  NULL);
    XtVaSetValues(window->hilites[1],
                  XmNleftOffset, wx - HILITE_THK,
                  XmNtopOffset,  wy + wh,
                  XmNwidth,      ww + HILITE_THK * 2,
                  XmNheight,     HILITE_THK,
                  NULL);
    XtVaSetValues(window->hilites[2],
                  XmNleftOffset, wx - HILITE_THK,
                  XmNtopOffset,  wy,
                  XmNwidth,      HILITE_THK,
                  XmNheight,     wh,
                  NULL);
    XtVaSetValues(window->hilites[3],
                  XmNleftOffset, wx + ww,
                  XmNtopOffset,  wy,
                  XmNwidth,      HILITE_THK,
                  XmNheight,     wh,
                  NULL);
    
    XtManageChildren(window->hilites, countof(window->hilites));
#endif

    window->hilite_shown = 1;
}

void XhHideHilite(CxWidget w)
{
  XhWindow  window;
  int       n;

    if (w == NULL) return;
    window = XhWindowOf(w);
  
    if (window == NULL)             return;
    if (window->hilites[0] == NULL) return;
    if (window->hilite_shown == 0)  return;

#if USE_OVERRIDE
    for (n = 0;  n < countof(window->hilites);  n++)
        XtPopdown(window->hilites[n]);
#else
    XtUnmanageChildren(window->hilites, countof(window->hilites));
#endif
    
    window->hilite_shown = 0;
}
