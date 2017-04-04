#include "InputOnlyP.h"


/********    Static Function Declarations    ********/

static void ClassPartInitialize(WidgetClass wc);
static void Initialize(Widget rw,
                       Widget nw,
                       ArgList args,
                       Cardinal *num_args);
static void Redisplay(Widget wid,
                      XEvent *event,
                      Region region);
static void Destroy(Widget callw);
static Boolean SetValues(Widget cw,
                         Widget rw,
                         Widget nw,
                         ArgList args,
                         Cardinal *num_args);
static void Realize(Widget                w,
                    XtValueMask          *value_mask,
                    XSetWindowAttributes *attributes);
static void HighlightBorder  (Widget w);
static void UnhighlightBorder(Widget w);

/********    End Static Function Declarations    ********/

static _XmConst char defaultTranslations[] = "";


static XtActionsRec actionsList[] =
{
};

static XtResource resources[] =
{
};

static XmSyntheticResource syn_resources[] =
{
};

/*  The InputOnly class record definition  */
externaldef (xminputonlyclassrec) XmInputOnlyClassRec xmInputOnlyClassRec=
{
  { /* Core fields */
    (WidgetClass) &xmPrimitiveClassRec, /* superclass            */     
    "XmInputOnly",                      /* class_name            */
    sizeof(XmInputOnlyRec),             /* widget_size           */
    (XtProc)NULL,                       /* class_initialize      */    
    ClassPartInitialize,                /* class_part_initialize */
    FALSE,                              /* class_inited          */     
    Initialize,                         /* initialize            */     
    (XtArgsProc)NULL,                   /* initialize_hook       */
    Realize,                            /* realize               */
    actionsList,                        /* actions               */     
    XtNumber(actionsList),              /* num_actions           */     
    resources,                          /* resources             */     
    XtNumber(resources),                /* num_resources         */     
    NULLQUARK,                          /* xrm_class             */     
    TRUE,                               /* compress_motion       */     
    XtExposeCompressMaximal,            /* compress_exposure     */     
    TRUE,                               /* compress_enterleave   */
    FALSE,                              /* visible_interest      */     
    Destroy,                            /* destroy               */     
    NULL,                               /* resize                */
    Redisplay,                          /* expose                */     
    SetValues,                          /* set_values            */     
    (XtArgsFunc)NULL,                   /* set_values_hook       */
    XtInheritSetValuesAlmost,           /* set_values_almost     */
    (XtArgsProc)NULL,                   /* get_values_hook       */
    (XtAcceptFocusProc)NULL,            /* accept_focus          */     
    XtVersion,                          /* version               */
    (XtPointer)NULL,                    /* callback private      */
    defaultTranslations,                /* tm_table              */
    (XtGeometryHandler)NULL,            /* query_geometry        */
    (XtStringProc)NULL,                 /* display_accelerator   */
    (XtPointer)NULL                     /* extension             */
  },

  { /* XmPrimitive fields */
    HighlightBorder,                    /* border_highlight   */
    UnhighlightBorder,                  /* border_unhighlight */
    XtInheritTranslations,              /* translations                 */
    (XtPointer)NULL,                    /* arm_and_activate             */
    syn_resources,                      /* syn_resources */
    XtNumber(syn_resources),            /* num_syn_resources */
    (XtPointer) NULL                    /* extension                    */
  },

  { /* XmArrowButtonWidget fields */
    (XtPointer) NULL                    /* extension                    */
  }
};

externaldef(xminputonlywidgetclass) WidgetClass xmInputOnlyWidgetClass =
                          (WidgetClass) &xmInputOnlyClassRec;

static void ClassPartInitialize(WidgetClass wc)
{
}

static void Initialize(Widget rw,
                       Widget nw,
                       ArgList args,
                       Cardinal *num_args)
{
  XmInputOnlyWidget request = (XmInputOnlyWidget) rw ;
  XmInputOnlyWidget new_w   = (XmInputOnlyWidget) nw ;

    /*  Set up a geometry for the widget if it is currently 0.  */
    if (request->core.width == 0)
        new_w->core.width  += 15;
    if (request->core.height == 0)
        new_w->core.height += 15;

    /* Force traversal, shadow and border */
    new_w->primitive.traversal_on        = FALSE;
    new_w->primitive.highlight_on_enter  = FALSE;
    new_w->primitive.highlight_thickness = 0;
    new_w->primitive.shadow_thickness    = 0;
    new_w->core.border_width             = 0;

    /* ...and depth */
    new_w->core.depth                   = 0;
}

static void Redisplay(Widget wid,
                      XEvent *event,
                      Region region)
{
    /* "Redisplay" is nonsence for InputOnly windows */
}

static void Destroy(Widget callw)
{
}

static Boolean SetValues(Widget cw,
                         Widget rw,
                         Widget nw,
                         ArgList args,
                         Cardinal *num_args)
{
  XmInputOnlyWidget current = (XmInputOnlyWidget) cw;
  XmInputOnlyWidget new_w   = (XmInputOnlyWidget) nw;
  Boolean  redisplayFlag = False;

    /* Force traversal and border */
    new_w->primitive.traversal_on        = FALSE;
    new_w->primitive.highlight_on_enter  = FALSE;
    new_w->primitive.highlight_thickness = 0;
    new_w->primitive.shadow_thickness    = 0;
    new_w->core.border_width             = 0;

    return redisplayFlag;
}

/* From xc/programs/Xserver/dix/window.c */
#define INPUTONLY_LEGAL_MASK (CWWinGravity | CWEventMask | \
                              CWDontPropagate | CWOverrideRedirect | CWCursor )

static void Realize(Widget                w,
                    XtValueMask          *value_mask,
                    XSetWindowAttributes *attributes)
{
  XmInputOnlyWidget  selfw  = (XmInputOnlyWidget) w;

    XtCreateWindow(w, (unsigned int) InputOnly,
                   (Visual *) CopyFromParent,
                   *value_mask & INPUTONLY_LEGAL_MASK, attributes);
}

static void HighlightBorder  (Widget w) {}
static void UnhighlightBorder(Widget w) {}
