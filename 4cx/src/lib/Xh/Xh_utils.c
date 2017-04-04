#include "Xh_includes.h"


/*
 *  attach{left,right,top,bottom}
 */

#define attachgeneric(side)\
    void attach##side(Widget what, Widget where, int offset)                \
    {                                                                       \
      int  ac;                                                              \
      Arg  al[10];                                                          \
                                                                            \
        ac = 0;                                                             \
        if (where == NULL)                                                  \
        {                                                                   \
            XtSetArg(al[ac], XmN##side##Attachment, XmATTACH_FORM);   ac++; \
        }                                                                   \
        else                                                                \
        {                                                                   \
            XtSetArg(al[ac], XmN##side##Attachment, XmATTACH_WIDGET); ac++; \
            XtSetArg(al[ac], XmN##side##Widget,     where);           ac++; \
        }                                                                   \
        XtSetArg(al[ac], XmN##side##Offset, offset);                  ac++; \
                                                                            \
        XtSetValues(what, al, ac);                                          \
    }

attachgeneric(left)
attachgeneric(right)
attachgeneric(top)
attachgeneric(bottom)


CxWidget XhTopmost(CxWidget w)
{
  Widget  xw = CNCRTZE(w);
  Widget  top;
  
    while(1)
    {
        top = XtParent(xw);
        if (top == NULL) return ABSTRZE(xw);
        xw = top;
    }
}


typedef int (*pix_gtr_t)(Display       *display,
                         Drawable       d,
                         void          *src_ptr,
                         Pixmap        *pixmap_return,
                         Pixmap        *shapemask_return,
                         XpmAttributes *attributes);


static int DoAssignPix(Widget xw,
                       pix_gtr_t getter, void *pix_src_ptr,
                       int  is_arm)
{
  int             success;
  Pixmap          pixmap;
  XpmAttributes   attrs;
  Pixel           color;
  XpmColorSymbol  symbols[2];

    XtVaGetValues(xw,
                  is_arm? XmNarmColor : XmNbackground, &color,
                  NULL);

    symbols[0].name  = NULL;
    symbols[0].value = "none";
    symbols[0].pixel = color;
    symbols[1].name  = NULL;
    symbols[1].value = "#0000FFFFFFFF";
    symbols[1].pixel = color;

    attrs.colorsymbols = symbols;
    attrs.numsymbols   = 2;
    attrs.valuemask    = XpmColorSymbols;
    
    success = (getter(XtDisplay(xw),
                      XRootWindowOfScreen(XtScreen(xw)),
                      pix_src_ptr, &pixmap, NULL, &attrs) == XpmSuccess);
    if (success)
    {
        XtVaSetValues(xw,
                      is_arm? XmNarmPixmap : XmNlabelPixmap, pixmap,
                      NULL);
        if (!is_arm)
        {
            XtVaSetValues(xw, XmNlabelType, XmPIXMAP, NULL);
#if (XmVERSION*1000+XmREVISION) < 2003
            XtVaSetValues(xw, XmNforeground,  color,  NULL);
#endif
        }
    }

    return success;
}

int  XhAssignPixmap        (CxWidget w, char **pix)
{
  Widget  xw = CNCRTZE(w);
  int     r;
  
    r = DoAssignPix    (xw, (pix_gtr_t)XpmCreatePixmapFromData, pix, 0);
    if (r  &&  XtIsSubclass(xw, xmPushButtonWidgetClass))
        r = DoAssignPix(xw, (pix_gtr_t)XpmCreatePixmapFromData, pix, 1);

    return r;
}

int  XhAssignPixmapFromFile(CxWidget w, char *filename)
{
  Widget  xw = CNCRTZE(w);
  int     r;

    if (filename == NULL  ||  filename[0] == '\0') return 0;

    r = DoAssignPix    (xw, (pix_gtr_t)XpmReadFileToPixmap, filename, 0);
    if (r  &&  XtIsSubclass(xw, xmPushButtonWidgetClass))
        r = DoAssignPix(xw, (pix_gtr_t)XpmReadFileToPixmap, filename, 1);

    return r;
}


void XhInvertButton        (CxWidget w)
{
  Widget  xw = CNCRTZE(w);
  Pixmap  pix1, pix2;
  Pixel   col1, col2;
  Pixel   shd1, shd2;
  
        XtVaGetValues(xw,
                      XmNlabelPixmap,       &pix1,
                      XmNarmPixmap,         &pix2,
                      XmNbackground,        &col1,
                      XmNarmColor,          &col2,
                      XmNtopShadowColor,    &shd1,
                      XmNbottomShadowColor, &shd2,
                      NULL);
        XtVaSetValues(xw,
                      XmNlabelPixmap, pix2,
                      XmNarmPixmap,   pix1,
                      XmNbackground,        col2,
                      XmNarmColor,          col1,
                      XmNtopShadowColor,    shd2,
                      XmNbottomShadowColor, shd1,
                      NULL);
}

void XhAssignVertLabel(CxWidget w, const char *text, int alignment)
{
  Widget         xw    = CNCRTZE(w);

  Pixmap         ret   = None;

  Display       *dpy   = XtDisplay(xw);
  Window         root  = RootWindowOfScreen(XtScreen(xw));
  Widget         shell = CNCRTZE(XhTopmost(w));
  Visual        *vis   = NULL;
  
  XmString       s     = NULL;
  XmFontList     fl    = NULL;
  unsigned char  align;
  Pixmap         hpix  = None;
  XImage        *himg  = NULL;
  XImage        *vimg  = NULL;
  char          *data  = NULL;

  int            depth;
  Dimension      width;
  Dimension      height;

  Pixel          fg;
  Pixel          bg;
  XmDirection    dir;
  GC             lgc = NULL;
  GC             cgc = NULL;
  XGCValues      vals;

  int            x;
  int            y;

    if (shell != NULL) XtVaGetValues(shell, XmNvisual, &vis, NULL);
    if (vis == NULL) vis = DefaultVisualOfScreen(XtScreen(shell));

    if (text != NULL)
        s = XmStringCreateLtoR(text, xh_charset);
    else
        XtVaGetValues(CNCRTZE(w), XmNlabelString, &s, NULL);
    
    XtVaGetValues(xw,
                  XmNfontList, &fl,
                  XmNdepth,    &depth,
                  NULL);
    XmStringExtent(fl, s, &width, &height);
    if (width  < 1) width  = 1;
    if (height < 1) height = 1;
    
    hpix = XCreatePixmap(dpy, root, width, height, depth);
    if (hpix == None) goto DO_CLEANUP;
    
    XtVaGetValues(xw,
                  XmNforeground,      &fg,
                  XmNbackground,      &bg,
                  XmNlayoutDirection, &dir,
                  NULL);

    vals.foreground = fg;
    vals.background = bg;
    lgc = XtAllocateGC(xw, depth,
                       GCForeground | GCBackground,
                       &vals,
                       GCFont,
                       0);
    if (lgc == NULL) goto DO_CLEANUP;

    vals.foreground = bg;
    cgc = XtGetGC(xw, GCForeground, &vals);

    XFillRectangle(dpy, hpix, cgc, 0, 0, width, height);
    align                    = XmALIGNMENT_CENTER;
    if (alignment < 0) align = XmALIGNMENT_BEGINNING;
    if (alignment > 0) align = XmALIGNMENT_END;
    XmStringDrawImage(dpy, hpix,
                      fl, s, lgc,
                      0, 0, width,
                      align, dir, NULL);

    if ( 
        (himg = XGetImage(dpy, hpix,
                          0, 0, width, height,
                          AllPlanes, ZPixmap)) == NULL  
         ||
        (data = malloc(width * height * 4)) == NULL
         ||
        (vimg = XCreateImage(dpy, vis, depth, ZPixmap,
                             0, data, height, width,
                             32, 0)) == NULL
       ) goto DO_CLEANUP;

    for (y = 0;  y < height;  y++)
        for (x = 0;  x < width;  x++)
            XPutPixel(vimg, y, width - x - 1,
                      XGetPixel(himg, x, y));

    ret = XCreatePixmap(dpy, root, height, width, depth);
    if (ret == None) goto DO_CLEANUP;

    XPutImage(dpy, ret, cgc,
              vimg, 0, 0, 0, 0, height, width);

    XtVaSetValues(xw,
                  XmNlabelType,   XmPIXMAP,
                  XmNlabelPixmap, ret,
                  NULL);

 DO_CLEANUP:
    if (s    != NULL) XmStringFree  (s);
    if (hpix != None) XFreePixmap   (dpy, hpix);
    if (himg != NULL) XDestroyImage (himg);
    if (vimg != NULL) XDestroyImage (vimg), data = NULL;
    if (data != NULL) free(data);
    if (lgc  != NULL) XtReleaseGC(xw, lgc);
    if (cgc  != NULL) XtReleaseGC(xw, cgc);
}


void XhSwitchContentFoldedness(CxWidget parent)
{
  Widget        xp = CNCRTZE(parent);
  WidgetList    children;
  Cardinal      numChildren;
  Cardinal      i;
  Widget        to_show[200]; /* 200 seems to be a fair limit; in unlimited case we'll need to XtMalloc() */
  Widget        to_hide[200]; /* 200 seems to be a fair limit; in unlimited case we'll need to XtMalloc() */
  int           n_to_show;
  int           n_to_hide;

    if (!XtIsComposite(xp)) return;

    XtVaGetValues(xp,
                  XmNchildren,    &children,
                  XmNnumChildren, &numChildren,
                  NULL);
  
    for (i = 0,               n_to_show = 0,                    n_to_hide = 0;
         i < numChildren  &&  n_to_show < countof(to_show)  &&  n_to_hide < countof(to_hide);
         i++)
        if (XtIsManaged(children[i]))
            to_hide[n_to_hide++] = children[i];
        else
            to_show[n_to_show++] = children[i];
    
    XtChangeManagedSet(to_hide, n_to_hide, NULL, NULL, to_show, n_to_show);
}


//////////////////////////////////////////////////////////////////////

static int (*saved_error_handler)(Display *, XErrorEvent *) = NULL;
static int   may_fail                                       = 0;
static int   was_X_error                                    = 0;

static int ErrorHandler(Display     *d __attribute__((unused)),
                        XErrorEvent *e __attribute__((unused)))
{
    was_X_error = 1;

//    fprintf(stderr, "ERR!!!\n");
    
    return 0;
}

void XhENTER_FAILABLE(void)
{
    may_fail++;
    was_X_error = 0;

    if (may_fail == 1) saved_error_handler = XSetErrorHandler(ErrorHandler);
}

int  XhLEAVE_FAILABLE(void)
{
    was_X_error = 0;
    may_fail--;

    if (may_fail == 0) XSetErrorHandler(saved_error_handler);

    return was_X_error != 0;
}
//////////////////////////////////////////////////////////////////////


void XhProcessPendingXEvents(void)
{
#define DEBUG_XhProcessPendingXEvents 0
#if DEBUG_XhProcessPendingXEvents
  int  ctr = 0; XEvent ev;
  
    while (XtAppPending(xh_context) & XtIMXEvent)
    {
        XtAppPeekEvent(xh_context, &ev);
        fprintf(stderr, "%d ", ev.type);
        ctr++;
        XtAppProcessEvent(xh_context, XtIMXEvent);
    }
    if (ctr) fprintf(stderr, "%dX\n", ctr); else fprintf(stderr, "- ");
#else
    while (XtAppPending(xh_context) & XtIMXEvent)
        XtAppProcessEvent(xh_context, XtIMXEvent);
#endif
}


typedef struct
{
    Widget  w;
    Window  xwin;
    int     ctr;
} evchkrec_t;

static int IsAncestorOf(Window win, Widget w)
{
    while (w != NULL)
    {
        if (win == XtWindow(w)) return 1;
        
        w = XtParent(w);
    }

    return 0;
}

static Bool count_events(Display  *dpy __attribute__((unused)),
                         XEvent   *ev,
                         XPointer  p)
{
  evchkrec_t *rec = (evchkrec_t *)p;
  
    if (ev->type == ConfigureNotify  &&  IsAncestorOf(ev->xany.window, rec->w))
        rec->ctr++;
  
    return False;
}

static Bool erase_events(Display  *dpy __attribute__((unused)),
                         XEvent   *ev,
                         XPointer  p)
{
  evchkrec_t *rec = (evchkrec_t *)p;
  
    if (ev->type == ConfigureNotify  &&  IsAncestorOf(ev->xany.window, rec->w))
        return True;
  
    return False;
}

/*
 *  XhCompressConfigureEvents
 *
 *  Returns:
 *      0 if widget may be redrawn (there were no Configure events in queue)
 *      1 if redrawing should be postponed
 */

int  XhCompressConfigureEvents  (CxWidget w)
{
  Widget               xw  = CNCRTZE(w);
  Display             *dpy = XtDisplay(xw);
  XEvent               junk_ev;
  evchkrec_t           ev_rec;

    ev_rec.w    = xw;
    ev_rec.xwin = XtWindow(xw);
    ev_rec.ctr  = 0;
    XCheckIfEvent(dpy, &junk_ev, count_events, (XPointer)&ev_rec);
    if (ev_rec.ctr > 1)
    {
        //fprintf(stderr, "%s: %d\n", XtName(w), ev_rec.ctr);
        while (ev_rec.ctr > 1  &&
               XCheckIfEvent(dpy, &junk_ev, erase_events, (XPointer)&ev_rec))
               ev_rec.ctr--;
    }

    return ev_rec.ctr != 0;
}


void XhRemoveExposeEvents       (CxWidget w)
{
  Widget         xw  = CNCRTZE(w);
  Display       *dpy = XtDisplay(xw);
  Window         win = XtWindow (xw);
  XEvent         junk_ev;

    while (XCheckTypedWindowEvent(dpy, win, Expose, &junk_ev));
}


#include <X11/IntrinsicP.h>
#include <X11/CoreP.h>
#include <Xm/FormP.h>

void XhAdjustPreferredSizeInForm(CxWidget w)
{
  Widget               xw  = CNCRTZE(w);
  Widget               parent;
  Dimension            cur_w;
  Dimension            cur_h;
  XmFormConstraintRec *cp;

    while (xw != NULL)
    {
        parent = XtParent(xw);

        if (parent != NULL  &&  XmIsForm(parent))
        {
            XtVaGetValues(xw, XmNwidth, &cur_w, XmNheight, &cur_h, NULL);
            if (cur_w != 0  &&  cur_h != 0  &&  (cur_w != 1  ||  cur_h != 1))
            {
                cp = (XmFormConstraintRec *)(xw->core.constraints);
                cp->form.preferred_width  = cur_w;
                cp->form.preferred_height = cur_h;
            }
        }

        xw = parent;
    }
}
