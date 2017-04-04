#include <stdio.h>

#include <Xm/ManagerP.h>

#include "IncDecBP.h"
#define XK_MISCELLANY
#include <X11/keysymdef.h>

enum {
    id_IncPressed = 1,
    id_DecPressed = 2
};
    

/********    Static Function Declarations    ********/

static void ClassPartInitialize( 
                        WidgetClass wc);
static void Initialize( 
                        Widget rw,
                        Widget nw,
                        ArgList args,
                        Cardinal *num_args);
static void Redisplay( 
                        Widget wid,
                        XEvent *event,
                        Region region);
static void Destroy(Widget callw);
static Boolean SetValues( 
                        Widget cw,
                        Widget rw,
                        Widget nw,
                        ArgList args,
                        Cardinal *num_args);
static void IncDecBArm(Widget   callw, XEvent   *event,
                       String   *params, Cardinal *num_params);
static void IncDecBDisarm(Widget   callw, XEvent   *event,
                          String   *params, Cardinal *num_params);
static void IncDecBRightClick(Widget   callw, XEvent   *event,
                              String   *params, Cardinal *num_params);

static void Realize(Widget                w,
                    XtValueMask          *value_mask,
                    XSetWindowAttributes *attributes);

/********    End Static Function Declarations    ********/

/*  Default translation table and action list  */

_XmConst char defaultTranslations[] = "\
<Btn1Down>:     IncDecBArm()\n\
<Btn1Up>:       IncDecBDisarm()\n\
<Btn3Down>:     IncDecBRightClick()\n";

static XtActionsRec actionsList[] =
{
    {"IncDecBArm",        IncDecBArm},
    {"IncDecBDisarm",     IncDecBDisarm},
    {"IncDecBRightClick", IncDecBRightClick},
};

/* from lib/Xm/ColorI.h */
extern void _XmSelectColorDefault( 
                        Widget widget,
                        int offset,
                        XrmValue *value) ;

static XtResource resources[] =
{
    {
        XmNarmColor, XmCArmColor,
        XmRPixel, sizeof (Pixel),
        XtOffsetOf(XmIncDecButtonRec, incdecbutton.arm_color),
        XmRCallProc, (XtPointer) _XmSelectColorDefault
    },
    {
        XmNclientWidget, XmCClientWidget,
        XmRWidget, sizeof(Widget),
        XtOffsetOf(XmIncDecButtonRec, incdecbutton.client_widget),
        XmRImmediate, (XtPointer)NULL,
    },
    {
        XmNinitialDelay, XmCInitialDelay, XmRInt, sizeof (int),
        XtOffsetOf(XmIncDecButtonRec, incdecbutton.initial_delay),
        XmRImmediate, (XtPointer) 250
    },
    {
        XmNrepeatDelay, XmCRepeatDelay, XmRInt, sizeof (int),
        XtOffsetOf(XmIncDecButtonRec, incdecbutton.repeat_delay),
        XmRImmediate, (XtPointer) 50
    },
};

static XmSyntheticResource syn_resources[] =
{
};

/*  The IncDecButton class record definition  */
externaldef (xmincdecbuttonclassrec) XmIncDecButtonClassRec xmIncDecButtonClassRec=
{
  { /* Core fields */
    (WidgetClass) &xmPrimitiveClassRec, /* superclass            */	
    "XmIncDecButton",			/* class_name	         */
    sizeof(XmIncDecButtonRec),		/* widget_size	         */
    (XtProc)NULL,			/* class_initialize      */    
    ClassPartInitialize,		/* class_part_initialize */
    FALSE,				/* class_inited          */	
    Initialize,				/* initialize	         */	
    (XtArgsProc)NULL,			/* initialize_hook       */
    /*XtInheritRealize*/Realize,			/* realize	         */
    actionsList,			/* actions               */	
    XtNumber(actionsList),		/* num_actions    	 */	
    resources,				/* resources	         */	
    XtNumber(resources),		/* num_resources         */	
    NULLQUARK,				/* xrm_class	         */	
    TRUE,				/* compress_motion       */	
    XtExposeCompressMaximal,		/* compress_exposure     */	
    TRUE,				/* compress_enterleave   */
    FALSE,				/* visible_interest      */	
    Destroy,				/* destroy               */	
    NULL,				/* resize                */
    Redisplay,				/* expose                */	
    SetValues,				/* set_values	         */	
    (XtArgsFunc)NULL,			/* set_values_hook       */
    XtInheritSetValuesAlmost,		/* set_values_almost     */
    (XtArgsProc)NULL,			/* get_values_hook       */
    (XtAcceptFocusProc)NULL,		/* accept_focus	         */	
    XtVersion,				/* version               */
    (XtPointer)NULL,			/* callback private      */
    defaultTranslations,		/* tm_table              */
    (XtGeometryHandler)NULL,		/* query_geometry        */
    (XtStringProc)NULL,			/* display_accelerator   */
    (XtPointer)NULL			/* extension             */
  },

  { /* XmPrimitive fields */
    XmInheritBorderHighlight,		/* Primitive border_highlight   */
    XmInheritBorderUnhighlight,		/* Primitive border_unhighlight */
    XtInheritTranslations,		/* translations                 */
    (XtPointer)NULL,			/* arm_and_activate             */
    syn_resources,         		/* syn_resources */
    XtNumber(syn_resources),	        /* num_syn_resources */
    (XtPointer) NULL         		/* extension                    */
  },

  { /* XmArrowButtonWidget fields */
    (XtPointer) NULL			/* extension			*/
  }
};

externaldef(xmincdecbuttonwidgetclass) WidgetClass xmIncDecButtonWidgetClass =
			  (WidgetClass) &xmIncDecButtonClassRec;

static void ClassPartInitialize(
                               WidgetClass wc)
{
}

static void GetNormalGC(XmIncDecButtonWidget idb)
{
  XGCValues values;
  XtGCMask  valueMask;
  
    valueMask = GCForeground | GCBackground | GCFillStyle;
  
    values.foreground = idb->primitive.foreground;
    values.background = idb->core.background_pixel;
    values.fill_style = FillSolid;
  
    idb->incdecbutton.normal_gc = XtGetGC((Widget) idb, valueMask, &values);

    valueMask |= GCFillStyle | GCStipple;
    values.fill_style = FillOpaqueStippled;
    values.stipple    = _XmGetInsensitiveStippleBitmap((Widget) idb);

    idb->incdecbutton.insensitive_gc = XtGetGC((Widget) idb, valueMask, &values);
}

static void GetBackgroundGC(XmIncDecButtonWidget idb)
{
  XGCValues values;
  XtGCMask  valueMask;
  
    valueMask = GCForeground | GCBackground | GCFillStyle;
  
    values.foreground = idb->core.background_pixel;
    values.background = idb->primitive.foreground;
    values.fill_style = FillSolid;
  
    idb->incdecbutton.background_gc = XtGetGC((Widget) idb, valueMask, &values);
}

static void GetFillGC(XmIncDecButtonWidget idb)
{
  XGCValues values;
  XtGCMask  valueMask;
  
    valueMask = GCForeground | GCBackground | GCFillStyle;
  
    values.foreground = idb->incdecbutton.arm_color;
    values.background = idb->core.background_pixel;
    values.fill_style = FillSolid;
  
    idb->incdecbutton.fill_gc = XtGetGC((Widget) idb, valueMask, &values);
}

static void Initialize(
                       Widget rw,
                       Widget nw,
                       ArgList args,		/* unused */
                       Cardinal *num_args)	/* unused */
{
  XmIncDecButtonWidget request = (XmIncDecButtonWidget) rw ;
  XmIncDecButtonWidget new_w   = (XmIncDecButtonWidget) nw ;

    /*  Set up a geometry for the widget if it is currently 0.  */
    if (request->core.width == 0)
        new_w->core.width  += 15;
    if (request->core.height == 0)
        new_w->core.height += 15;

    ////fprintf(stderr, "%s: w=%d, h=%d\n", __FUNCTION__, new_w->core.width, new_w->core.height);
    
    /* Force traversal */
    if (XtClass(new_w) == xmIncDecButtonWidgetClass)
        new_w->primitive.traversal_on =
        new_w->primitive.highlight_on_enter = FALSE;
    
    /* Set internal variables */
    new_w->incdecbutton.armed_parts = 0;
    new_w->incdecbutton.key_up = XKeysymToKeycode(XtDisplay(new_w), XK_Up);
    new_w->incdecbutton.key_dn = XKeysymToKeycode(XtDisplay(new_w), XK_Down);
    GetNormalGC(new_w);
    GetBackgroundGC(new_w);
    GetFillGC(new_w);
    new_w->incdecbutton.timer = 0;
}

static void FixXFillPolygon(Display *display, Drawable d, GC gc,
                            XPoint *points, int npoints)
{
    XFillPolygon(display, d, gc, points, npoints, Convex, CoordModeOrigin);
    XDrawLines  (display, d, gc, points, npoints, CoordModeOrigin);
    XDrawLine   (display, d, gc,
                 points[0].x, points[0].y, points[npoints-1].x, points[npoints-1].y);
}

#define SWAP_GCS(gc1, gc2) do {GC tmp = gc1; gc1 = gc2; gc2 = tmp;} while(0)

static void Redisplay(
                      Widget callw,
                      XEvent *event,
                      Region region)
{
  XmIncDecButtonWidget  selfw = (XmIncDecButtonWidget)callw;
  int x0, y0, ow, oh;
  int ars, ax, ay;
  int sh;
  int st = selfw->primitive.shadow_thickness;
  GC  incTopGC, incBotGC, decTopGC, decBotGC;
  GC  arrowsGC;
  Display *dpy = XtDisplay(callw);
  Window   win = XtWindow(callw);
  XPoint   incArrow[3], decArrow[3];
  XPoint   incBody[3], decBody[3];

    x0 = y0 = selfw->primitive.highlight_thickness;
    ow = selfw->core.width  - x0*2;
    oh = selfw->core.height - y0*2;

    ars = ow / 7;
    ax = ow / 3;
    ay = ow / 5;

#define E0 1
#define E1 2
#define E2 0
#define POLY_TYPE Complex
    
    decArrow[0].x = x0 + ax;              decArrow[0].y = x0 + oh - ay;
    decArrow[1].x = decArrow[0].x - ars;  decArrow[1].y = decArrow[0].y - ars;
    decArrow[2].x = decArrow[0].x + ars;  decArrow[2].y = decArrow[0].y - ars;

    incArrow[0].x = x0 + ow - ax;         incArrow[0].y = y0 + ay;
    incArrow[1].x = incArrow[0].x + ars;  incArrow[1].y = incArrow[0].y + ars;
    incArrow[2].x = incArrow[0].x - ars;  incArrow[2].y = incArrow[0].y + ars;
    
    incTopGC = decTopGC = selfw->primitive.top_shadow_GC;
    incBotGC = decBotGC = selfw->primitive.bottom_shadow_GC;

    if (selfw->incdecbutton.armed_parts & id_IncPressed)
        SWAP_GCS(incTopGC, incBotGC);
    if (selfw->incdecbutton.armed_parts & id_DecPressed)
        SWAP_GCS(decTopGC, decBotGC);
    
//    XFillRectangle(XtDisplay(callw), XtWindow(callw), selfw->incdecbutton.fill_gc,
//                   x0, y0, ow, oh);

    incBody[0].x = x0+1;    incBody[0].y = y0;
    incBody[1].x = x0+ow-1; incBody[1].y = y0;
    incBody[2].x = x0+ow-1; incBody[2].y = y0+oh-2;

    decBody[0].x = x0;      decBody[0].y = y0+1;
    decBody[1].x = x0+ow-2; decBody[1].y = y0+oh-1;
    decBody[2].x = x0;      decBody[2].y = y0+oh-1;

    XDrawLine(dpy, win,
              ((XmManagerWidget)(selfw->core.parent))->manager.background_GC,
              x0, y0, x0+ow-1, y0+oh-1);
    
    FixXFillPolygon(dpy, win,
                    (selfw->incdecbutton.armed_parts & id_IncPressed)?
                        selfw->incdecbutton.fill_gc : selfw->incdecbutton.background_gc,
                    incBody, 3);
    FixXFillPolygon(dpy, win,
                    (selfw->incdecbutton.armed_parts & id_DecPressed)?
                        selfw->incdecbutton.fill_gc : selfw->incdecbutton.background_gc,
                    decBody, 3);

#if 0
    XmeDrawPolygonShadow(dpy, win, incTopGC, incBotGC, incBody, 3, st, XmSHADOW_OUT);
    XmeDrawPolygonShadow(dpy, win, decTopGC, decBotGC, decBody, 3, st, XmSHADOW_OUT);
#else
    for (sh = 0; sh < st; sh++)
    {
        XDrawLine(dpy, win, incTopGC, x0+sh+1,    y0+sh,      x0+ow-sh-1, y0+sh);
        XDrawLine(dpy, win, decTopGC, x0+sh,      y0+sh+1,    x0+sh,      y0+oh-sh-1);

        XDrawLine(dpy, win, incBotGC, x0+ow-sh-1, y0+sh+1,    x0+ow-sh-1, y0+oh-sh-2);
        XDrawLine(dpy, win, decBotGC, x0+sh+1,    y0+oh-sh-1, x0+ow-sh-2, y0+oh-sh-1);

        #if 0
        XDrawLine(dpy, win, incBotGC, x0+sh+st+1, y0+st,      x0+ow-st-1, y0+oh-sh-st-2);
        XDrawLine(dpy, win, decTopGC, x0+st,      y0+sh+st+1, x0+ow-sh-st-2, y0+oh-st-1);
        #else
        //XDrawLine(dpy, win, incBotGC, x0+sh+2,    y0+sh*1+1,    x0+ow-sh*1-2, y0+oh-sh-3);
        //XDrawLine(dpy, win, decTopGC, x0+sh*1+1,    y0+sh+2,    x0+ow-sh-3, y0+oh-sh*1-2);
        #endif
    }

    if (st != 0)
    {
        XDrawLine(dpy, win, incBotGC, x0+1+st-st/2,    y0+st-st/2,    x0+ow-st-1, y0+oh-st-2);
        XDrawLine(dpy, win, decTopGC, x0+st,    y0+st+1,    x0+ow-(st-st/2+2), y0+oh-(st-st/2+1));
    }

#endif

    arrowsGC = XtIsSensitive(callw) ? selfw->incdecbutton.normal_gc : selfw->incdecbutton.insensitive_gc;
    FixXFillPolygon(dpy, win, arrowsGC, incArrow, 3);
    FixXFillPolygon(dpy, win, arrowsGC, decArrow, 3);
    
//    XDrawLine(dpy, win, selfw->primitive.highlight_GC,
//              x0, y0, x0+ow, y0+oh);
    
    /* Envelop our superclass expose method */
    (*(xmPrimitiveClassRec.core_class.expose))(callw, event, region);
}

static void Destroy(Widget callw)
{
  XmIncDecButtonWidget selfw = (XmIncDecButtonWidget) callw;
  
    XtReleaseGC(callw, selfw->incdecbutton.normal_gc);
    XtReleaseGC(callw, selfw->incdecbutton.insensitive_gc);
    XtReleaseGC(callw, selfw->incdecbutton.background_gc);
    XtReleaseGC(callw, selfw->incdecbutton.fill_gc);
    if (selfw->incdecbutton.timer != 0)
    {
        XtRemoveTimeOut (selfw->incdecbutton.timer);
        selfw->incdecbutton.timer = 0;
    }
    
}

static Boolean SetValues(
                         Widget cw,
                         Widget rw,
                         Widget nw,
                         ArgList args,           /* unused */
                         Cardinal *num_args)     /* unused */
{
  XmIncDecButtonWidget current = (XmIncDecButtonWidget) cw;
  XmIncDecButtonWidget new_w   = (XmIncDecButtonWidget) nw;
  Boolean  redisplayFlag = False;

    if (new_w->primitive.foreground != current->primitive.foreground)
    {
        redisplayFlag = True;
        XtReleaseGC((Widget)new_w, new_w->incdecbutton.normal_gc);
        XtReleaseGC((Widget)new_w, new_w->incdecbutton.insensitive_gc);
        GetNormalGC(new_w);
    }

    if (XtIsSensitive(nw) != XtIsSensitive(cw))
    {
        new_w->incdecbutton.armed_parts = 0;
        if (new_w->incdecbutton.timer != 0)
        {
            XtRemoveTimeOut(new_w->incdecbutton.timer);
            new_w->incdecbutton.timer = 0;
        }
        redisplayFlag = True;
    }
    
    if (new_w->core.background_pixel != current->core.background_pixel)
    {
        redisplayFlag = True;
        XtReleaseGC((Widget)new_w, new_w->incdecbutton.background_gc);
        GetBackgroundGC(new_w);
    }

    if (new_w->incdecbutton.arm_color != current->incdecbutton.arm_color)
    {
        if (new_w->incdecbutton.armed_parts != 0) redisplayFlag = True;
        XtReleaseGC((Widget)new_w, new_w->incdecbutton.fill_gc);
        GetFillGC(new_w);
    }

    /* Force traversal */
    if (XtClass(new_w) == xmIncDecButtonWidgetClass)
        new_w->primitive.traversal_on =
        new_w->primitive.highlight_on_enter = FALSE;
    
    return redisplayFlag;
}

static void SendEventToClient(XmIncDecButtonWidget w, int action_inc, XButtonEvent *ev)
{
  XKeyPressedEvent  se;
  Widget            client = w->incdecbutton.client_widget;
  
    if (client == NULL) return;
  
    bzero(&se, sizeof(se));
    se.type        = KeyPress;
    se.serial      = ev->serial;
    se.send_event  = True;
    se.display     = XtDisplay(client);
    se.window      = XtWindow(client);
    se.root        = ev->root;
    se.subwindow   = None;
    se.time        = ev->time;
    se.x           = 0; /* Who-o-o-ps... */
    se.y           = 0;
    se.x_root      = ev->x_root;
    se.y_root      = ev->y_root;
    se.state       = ev->state;
    se.keycode     = action_inc? w->incdecbutton.key_up : w->incdecbutton.key_dn;
    se.same_screen = ev->same_screen;

    if (XmProcessTraversal(client, XmTRAVERSE_CURRENT))
        XSendEvent(XtDisplay(client), XtWindow(client), False, KeyPressMask, (XEvent *)(&se));
}


static void TimerProc(XtPointer closure, XtIntervalId *id)
{
  XmIncDecButtonWidget selfw = (XmIncDecButtonWidget) closure;
  XButtonEvent         se;
  
    /* We use timer:=0 as a "stop" flag */
    if (selfw->incdecbutton.timer == 0) return;

    /* Purge just-had-shot timer ID */
    selfw->incdecbutton.timer = 0;

    /* Fake a mouse event */
    bzero(&se, sizeof(se));
    se.root  = RootWindowOfScreen(XtScreen((Widget)selfw));
    se.state = selfw->incdecbutton.click_state;
    
    /* Kick the client... */
    SendEventToClient(selfw,
                      selfw->incdecbutton.armed_parts == id_IncPressed,
                      &se);

    /* Schedule next shot */
    if (XtIsSensitive(selfw))
        selfw->incdecbutton.timer =
            XtAppAddTimeOut(XtWidgetToApplicationContext((Widget)selfw),
                            selfw->incdecbutton.repeat_delay,
                            TimerProc,
                            (XtPointer)selfw);
}

static void IncDecBArm(Widget   callw, XEvent   *event,
                       String   *params, Cardinal *num_params )
{
  XmIncDecButtonWidget selfw = (XmIncDecButtonWidget) callw;
    
  int x0, y0, ow, oh;
  XButtonEvent *ev = (XButtonEvent *)event;
  int ex, ey; /* Effective x,y */
  int arrow;

  ////fprintf(stderr, "%s, %d*%d\n", __FUNCTION__, selfw->core.width, selfw->core.height);

    x0 = y0 = selfw->primitive.highlight_thickness;
    ow = selfw->core.height - x0*2;
    oh = selfw->core.height - y0*2;

    if (event->type != ButtonPress) return;

    ex = ev->x - x0;
    ey = ev->y - y0;
    if (ex < 0  ||  ex >= ow  ||  ey < 0  ||  ey >= oh)
        return;

    arrow = 0;
    if (ey < ex) arrow = id_IncPressed;
    if (ey > ex) arrow = id_DecPressed;

    /* Did we hit just a diagonal separator? */
    if (arrow == 0) return;

    selfw->incdecbutton.armed_parts = arrow;
    if (XtIsRealized(callw))
        XClearArea(XtDisplay(callw), XtWindow(callw), 0, 0, 0, 0, True);

    SendEventToClient(selfw, arrow == id_IncPressed, ev);

    /* Initiate autorepeat */
    if (selfw->incdecbutton.timer == 0)
        selfw->incdecbutton.timer =
            XtAppAddTimeOut(XtWidgetToApplicationContext(callw),
                            selfw->incdecbutton.initial_delay,
                            TimerProc,
                            (XtPointer)selfw);
    selfw->incdecbutton.click_state = ev->state;
}

static void IncDecBDisarm(Widget   callw, XEvent   *event,
                          String   *params, Cardinal *num_params )
{
  XmIncDecButtonWidget selfw = (XmIncDecButtonWidget) callw;

    ////fprintf(stderr, "%s\n", __FUNCTION__);
    
    selfw->incdecbutton.armed_parts = 0;
    if (XtIsRealized(callw))
        XClearArea(XtDisplay(callw), XtWindow(callw), 0, 0, 0, 0, True);

    /* Stop autorepeat */
    if (selfw->incdecbutton.timer != 0)
    {
        XtRemoveTimeOut(selfw->incdecbutton.timer);
        selfw->incdecbutton.timer = 0;
    }
}

static void IncDecBRightClick(Widget   callw, XEvent   *event,
                              String   *params, Cardinal *num_params )
{
  XmIncDecButtonWidget  selfw  = (XmIncDecButtonWidget) callw;
  XButtonEvent         *ev     = (XButtonEvent *)event;
  XButtonEvent          se     = *ev;
  Widget                client = selfw->incdecbutton.client_widget;
  Position              cx, cy;

    if (client == NULL) return;

    XtTranslateCoords(client, 0, 0, &cx, &cy);
    
    se.window    = XtWindow(client);
    se.subwindow = None;
    se.x         = se.x_root - cx;
    se.y         = se.y_root - cy;
    
    XSendEvent(XtDisplay(client), XtWindow(client), False, ButtonPressMask, (XEvent *)(&se));
}

static void Realize(Widget                w,
                    XtValueMask          *value_mask,
                    XSetWindowAttributes *attributes)
{
  XmIncDecButtonWidget  selfw  = (XmIncDecButtonWidget) w;
  Widget                client = selfw->incdecbutton.client_widget;
  
    ////fprintf(stderr, "%s: %d*%d\n", __FUNCTION__, w->core.width, w->core.height);
    XtCreateWindow(w, (unsigned int) InputOutput,
                   (Visual *) CopyFromParent, *value_mask, attributes);
    if (client != NULL)
        XtMakeResizeRequest(w, client->core.height, client->core.height, NULL, NULL);
}
