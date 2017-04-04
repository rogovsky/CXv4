#include "Xh_includes.h"

#include <Xm/VendorSP.h>


enum
{
    DEF_WS_MARGINS = 7,
};


static XhWindow WindowList = NULL;


static void addToWindowList(XhWindow window) 
{
  XhWindow  temp;

    temp = WindowList;
    WindowList = window;
    window->next = temp;
}

static void removeFromWindowList(XhWindow window)
{
  XhWindow  temp;

    if (WindowList == window)
        WindowList = window->next;
    else
    {
        for (temp = WindowList; temp != NULL; temp = temp->next)
        {
            if (temp->next == window)
            {
                temp->next = window->next;
                break;
            }
        }
    }
}


static void AddMotifCloseCallback(Widget shell, XtCallbackProc closeProc, void *arg)
{
  static   Atom wmpAtom, dwAtom = 0;
  Display *display = XtDisplay(shell);

    /* deactivate the built in delete response of killing the application */
    XtVaSetValues(shell, XmNdeleteResponse, XmDO_NOTHING, 0);

    /* add a delete window protocol callback instead */
    if (dwAtom == 0) {
        wmpAtom = XmInternAtom(display, "WM_PROTOCOLS",     TRUE);
        dwAtom  = XmInternAtom(display, "WM_DELETE_WINDOW", TRUE);
    }
    XmAddProtocolCallback(shell, wmpAtom, dwAtom, closeProc, arg);
}

/* Copied from NEdit-5.2.2/util/misc.c */
/*
** This routine resolves a window manager protocol incompatibility between
** the X toolkit and several popular window managers.  Using this in place
** of XtRealizeWidget will realize the window in a way which allows the
** affected window managers to apply their own placement strategy to the
** window, as opposed to forcing the window to a specific location.
**
** One of the hints in the WM_NORMAL_HINTS protocol, PPlacement, gets set by
** the X toolkit (probably part of the Core or Shell widget) when a shell
** widget is realized to the value stored in the XmNx and XmNy resources of the
** Core widget.  While callers can set these values, there is no "unset" value
** for these resources.  On systems which are more Motif aware, a PPosition
** hint of 0,0, which is the default for XmNx and XmNy, is interpreted as,
** "place this as if no hints were specified".  Unfortunately the fvwm family
** of window managers, which are now some of the most popular, interpret this
** as "place this window at (0,0)".  This routine intervenes between the
** realizing and the mapping of the window to remove the inappropriate
** PPlacement hint.
*/ 
static void RealizeWithoutForcingPosition(Widget shell)
{
    XSizeHints *hints = XAllocSizeHints();
    long suppliedHints;
    Boolean mappedWhenManaged;
    
    /* Temporarily set value of XmNmappedWhenManaged
       to stop the window from popping up right away */
    XtVaGetValues(shell, XmNmappedWhenManaged, &mappedWhenManaged, NULL);
    XtVaSetValues(shell, XmNmappedWhenManaged, False, NULL);
    
    /* Realize the widget in unmapped state */
    XtRealizeWidget(shell);
    
    /* Get rid of the incorrect WMNormal hint */
    if (XGetWMNormalHints(XtDisplay(shell), XtWindow(shell), hints,
    	    &suppliedHints)) {
	hints->flags &= ~PPosition;
	XSetWMNormalHints(XtDisplay(shell), XtWindow(shell), hints);
    }
    XFree(hints);
    
    /* Map the widget */
    XtMapWidget(shell);
    
    /* Restore the value of XmNmappedWhenManaged */
    XtVaSetValues(shell, XmNmappedWhenManaged, mappedWhenManaged, NULL);
}


static void closeCB(Widget w            __attribute__((unused)),
                    XhWindow window,
                    XtPointer callData  __attribute__((unused)))
{
    if (window->commandproc != NULL  &&  window->close_cmd != NULL)
    {
        window->commandproc(window, window->close_cmd, 0);
        return;
    }
    
    if (WindowList->next == NULL) {
        XhDeleteWindow(window);
        exit(0);
    } else
    	XhDeleteWindow(window);
}


XhWindow  XhCreateWindow(const char    *title,
                         const char    *app_name, const char *app_class,
                         int            contents,
                         xh_actdescr_t *menus,
                         xh_actdescr_t *tools)
{
  XhWindow     window;
  Arg          al[20];
  int          ac;
  Dimension    fst;
  Widget       upper, lower;
  int          wsOffset;
  
    /* Allocate some memory for the new window data structure */
    window = (XhWindow )XtMalloc(sizeof(*window));
    bzero(window, sizeof(*window));

    /* Create a new toplevel shell to hold the window */
    ac = 0;
    XtSetArg(al[ac], XmNtitle,            title);      ac++;
    XtSetArg(al[ac], XmNiconName,         title);      ac++;
    XtSetArg(al[ac], XmNallowShellResize, True);       ac++;

    if (contents & XhWindowUnresizableMask)
    {
        XtSetArg(al[ac], XmNmwmDecorations, MWM_DECOR_TITLE | MWM_DECOR_MENU | MWM_DECOR_MINIMIZE); ac++;
        XtSetArg(al[ac], XmNmwmFunctions,   MWM_FUNC_MOVE | MWM_FUNC_MINIMIZE | MWM_FUNC_CLOSE);    ac++;
    }
    if (contents & XhWindowSecondaryMask)
    {
        XtSetArg(al[ac], XmNtransient,      True); ac++;
    }

    window->shell = XtAppCreateShell(app_name, app_class,
                                     applicationShellWidgetClass,
                                     TheDisplay, al, ac);
    _XhAllocateColors(window->shell);
    
    XhInitializeBalloon(window);

    /* add the window to the global window list */
    addToWindowList(window);

    AddMotifCloseCallback(window->shell, (XtCallbackProc)closeCB, window);
    
    /* Create a main form to manage the menubar, toolbar etc. */
    window->mainForm = XtVaCreateManagedWidget("mainForm",
                                               xmFormWidgetClass,
                                               window->shell,
                                               NULL);
    XtVaGetValues(window->mainForm, XmNshadowThickness, &fst, NULL);

    /* MUST create hilite widgets first, for them to be topmost */
    XhCreateHilite(window);

    /* Populate the window */
    if (contents & XhWindowToolbarMask)
        XhCreateToolbar(window, tools, (contents & XhWindowToolbarMiniMask) != 0);

    if (contents & XhWindowStatuslineMask)
        XhCreateStatusline(window);
    
    /* Create a workspace */
    wsOffset = DEF_WS_MARGINS;
    if (contents & XhWindowCompactMask)
        wsOffset = 0;

    switch (contents & XhWindowWorkspaceTypeMask)
    {
        case XhWindowRowColumnMask:
            window->workSpace = XtVaCreateManagedWidget
                ("workSpace",
                 xmRowColumnWidgetClass,
                 window->mainForm,
                 XmNmarginWidth,  0,
                 XmNmarginHeight, 0,
                 NULL);
            break;

        case XhWindowGridMask:
            window->workSpace = CNCRTZE(XhCreateGrid(ABSTRZE(window->mainForm), "workSpace"));
            break;
            
        default: /* Usually XhWindowFormMask */
            window->workSpace = XtVaCreateManagedWidget
                ("workSpace",
                 xmFormWidgetClass,
                 window->mainForm,
                 NULL);
    }

    upper = window->toolBar;
    if (upper == NULL) upper = window->menuBar;

    lower = window->statsForm;

    attachleft  (window->workSpace, NULL,  wsOffset + fst);
    attachright (window->workSpace, NULL,  wsOffset + fst);
    attachtop   (window->workSpace, upper, wsOffset + (upper == NULL? fst : 0));
    attachbottom(window->workSpace, lower, wsOffset + (lower == NULL? fst : 0));

    return window;
}


void        XhDeleteWindow(XhWindow window)
{
    /* Should call higher-level destructor here, if any */
    
    /* remove and deallocate all of the widgets associated with window */
    XtDestroyWidget(window->shell);
    
    /* remove the window from the global window list */
    removeFromWindowList(window);

    /* deallocate the window data structure */
    XtFree((char *)window);
}

void        XhShowWindow  (XhWindow window)
{
    RealizeWithoutForcingPosition(window->shell);
    XtMapWidget(window->shell);
}


void        XhHideWindow  (XhWindow window)
{
    XtUnmapWidget(window->shell);
}


void        XhSetWindowIcon(XhWindow window, char **pix)
{
  Pixmap          iconPixmap, maskPixmap;
  
    if (pix == NULL) return;
  
    XpmCreatePixmapFromData(XtDisplay(window->shell),
                            XRootWindowOfScreen(XtScreen(window->shell)),
                            pix, &iconPixmap, &maskPixmap, NULL);
    
    XtVaSetValues(window->shell,
                  XmNiconPixmap, iconPixmap,
                  XmNiconMask,   maskPixmap,
                  NULL);
}


void        XhSetWindowCmdProc(XhWindow window, XhCommandProc proc)
{
    window->commandproc = proc;
}

void        XhSetWindowCloseBh(XhWindow window, const char *cmd)
{
    window->close_cmd = cmd;
}

void        XhGetWindowSize   (XhWindow window, int *wid, int *hei)
{
  Dimension  fw, fh;
  
    XtVaGetValues(window->mainForm,
                  XmNwidth,  &fw,
                  XmNheight, &fh,
                  NULL);
    
    *wid = fw;
    *hei = fh;
}

void        XhSetWindowLimits (XhWindow window,
                               int  min_w, int min_h,
                               int  max_w, int max_h)
{
  enum
  {
      WID_INC = 1,
      HEI_INC = 1,
  };
    
    XSync(XtDisplay(window->shell), False);
    XtVaSetValues(window->shell,
                  XmNminWidth,   min_w,
                  XmNminHeight,  min_h,
                  XmNmaxWidth,   max_w,
                  XmNmaxHeight,  max_h,
                  XmNwidthInc,   WID_INC,
                  XmNheightInc,  HEI_INC,
                  XmNbaseWidth,  0, //min_w - WID_INC,
                  XmNbaseHeight, 0, //min_h - HEI_INC,
                  NULL);
}

XhWindow XhWindowOf(CxWidget w)
{
  Widget      shell  = CNCRTZE(XhTopmost(w));
  XhWindow    window = WindowList;
  
    while (window != NULL)
    {
        if (window->shell == shell) return window;
        window = window->next;
    }

    return NULL;
}

CxWidget    XhGetWindowShell    (XhWindow window)
{
    return ABSTRZE(window->shell);
}

CxWidget    XhGetWindowWorkspace(XhWindow window)
{
    return ABSTRZE(window->workSpace);
}

CxWidget    XhGetWindowAlarmleds(XhWindow window)
{
    return ABSTRZE(window->alarmLeds);
}

void       *XhGetHigherPrivate  (XhWindow window)
{
    return window->higher_private;
}

void        XhSetHigherPrivate  (XhWindow window, void *privptr)
{
    window->higher_private = privptr;
}
