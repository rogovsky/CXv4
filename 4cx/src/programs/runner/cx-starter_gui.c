#include <stdio.h>
#include <math.h>

#include <X11/Intrinsic.h>
#include <Xm/DrawingA.h>

#include "misc_macros.h"
#include "Xh.h"
#include "Xh_globals.h"

#include "cx-starter_gui.h"


enum
{
    FLASH_COUNT  = 3,     // Number of flashes
    FLASH_PERIOD = 1000,  // Full period of flash (on+off)
};

static Widget        flash_shell = NULL;
static Widget        flash_arrow = NULL;
static GC            arrow_GC;
static XtIntervalId  flash_tid   = 0;
static int           flash_ctr;
static double        arrow_sin;
static double        arrow_cos;

static int arrow_coords[][2] =
{
#if 1
    {95, 50},
    {65, 20*1+35*0},
    {65, 35},
    {10, 35},
    {10, 65},
    {65, 65},
    {65, 80},
#else
    {10, 35},
    {90, 35},
    {90, 65},
    {10, 65},
#endif
};

static void ArrowExposeCB(Widget     w,
                          XtPointer  closure    __attribute__((unused)),
                          XtPointer  call_data  __attribute__((unused)))
{
  Dimension  my_w, my_h;
  int        size;
  XPoint     points[countof(arrow_coords)];
  int        n;
  double        x, y;
  
    XtVaGetValues(w, XmNwidth, &my_w, XmNheight, &my_h, NULL);
    size = my_w;
    if (size > my_h) size = my_h;

    for (n = 0;  n < countof(points);  n++)
    {
        x = (arrow_coords[n][0] - 50) * size / 100;
        y = (arrow_coords[n][1] - 50) * size / 100;
        points[n].x = my_w / 2 + (x * arrow_cos - y * arrow_sin);
        points[n].y = my_h / 2 + (x * arrow_sin + y * arrow_cos);
    }

    XFillPolygon(XtDisplay(w), XtWindow(w), arrow_GC, points, countof(points), Nonconvex, CoordModeOrigin);
}

static void FlashProc(XtPointer     closure __attribute__((unused)),
                      XtIntervalId *id      __attribute__((unused)))
{
    flash_ctr--;
    if ((flash_ctr & 1) != 0  &&  flash_ctr > 0)
    {
        XtManageChild  (flash_shell);
        XtManageChild  (flash_arrow);
        //XRaiseWindow(XtDisplay(flash_arrow), XtWindow(flash_arrow));
    }
    else
    {
        XtUnmanageChild(flash_shell);
        XtUnmanageChild(flash_arrow);
    }

    if (flash_ctr > 0)
        flash_tid = XtAppAddTimeOut(xh_context, FLASH_PERIOD / 2, FlashProc, NULL);
    else
        flash_tid = 0;
}

static GC AllocXhFgGC(Widget w, int idx)
{
  XGCValues  vals;

    vals.foreground = XhGetColor(idx);
    return XtGetGC(w, GCForeground, &vals);
}

void PointToTargetWindow(Widget my_parent, Widget parent_shell, Window win)
{
  Display      *dpy     = XtDisplay(parent_shell);
  Window        my_root = RootWindowOfScreen(XtScreenOfObject(my_parent));
    
  Window        twin_root;
  int           twin_x, twin_y;
  unsigned int  twin_w, twin_h;
  unsigned int  twin_bw, twin_d;
  int           root_x, root_y;
  Window        child;

  int           my_x0, my_y0;
  Dimension     my_w,  my_h;
  int           delta_x, delta_y;
  double        angle;

    if (!XGetGeometry(dpy, win,
                      &twin_root,
                      &twin_x, &twin_y, &twin_w, &twin_h,
                      &twin_bw, &twin_d)) return;
    ////fprintf(stderr, "target: %d,%d %d*%d\n", twin_x, twin_y, twin_w, twin_h);

    if (!XTranslateCoordinates(dpy, win,
                               my_root, 0, 0,
                               &root_x, &root_y, &child)) return;
    ////fprintf(stderr, "\t%d,%d\n", root_x, root_y);

    if (flash_shell == NULL)
    {
        flash_shell = XtVaCreatePopupShell
            ("flashShell", overrideShellWidgetClass, parent_shell,
             XmNbackground,  XhGetColor(XH_COLOR_JUST_AMBER),
             XmNborderWidth, 0,
             NULL);
    }

    if (flash_arrow == NULL)
    {
        flash_arrow = XtVaCreateWidget("flashArrow",
                                       xmDrawingAreaWidgetClass,
                                       my_parent,
                                       XmNbackgroundPixmap, None,
                                       XmNleftAttachment,   XmATTACH_FORM,
                                       XmNrightAttachment,  XmATTACH_FORM,
                                       XmNtopAttachment,    XmATTACH_FORM,
                                       XmNbottomAttachment, XmATTACH_FORM,
                                       NULL);
        arrow_GC = AllocXhFgGC(flash_arrow, XH_COLOR_JUST_BLUE);
        XtAddCallback(flash_arrow, XmNexposeCallback, ArrowExposeCB, NULL);
    }

    XTranslateCoordinates(dpy, XtWindow(parent_shell),
                          my_root, 0, 0,
                          &my_x0, &my_y0, &child);
    ////fprintf(stderr, "%d %d\n", my_x0, my_y0);
    XtVaGetValues(my_parent, XmNwidth, &my_w, XmNheight, &my_h, NULL);
    delta_x = (root_x + twin_w / 2) - (my_x0 + my_w / 2);
    delta_y = (root_y + twin_h / 2) - (my_y0 + my_h / 2);
    if (delta_x == 0  &&  delta_y == 0)
        angle = 0;
    else
        angle = atan2(delta_y, delta_x);//*0+M_PI/40;
    arrow_sin = sin(angle);
    arrow_cos = cos(angle);
    ////fprintf(stderr, "%d,%d %4.3f %5.3f %5.3f\n", delta_x, delta_y, angle, arrow_sin, arrow_cos);
    
    XtVaSetValues(flash_shell,
                  XmNx,      root_x,
                  XmNy,      root_y,
                  XmNwidth,  twin_w,
                  XmNheight, twin_h,
                  NULL);
    flash_ctr = FLASH_COUNT * 2;
    if (flash_tid != 0) XtRemoveTimeOut(flash_tid);
    FlashProc(NULL, NULL);
}

