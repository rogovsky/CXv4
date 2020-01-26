#include <stdio.h>
#include <ctype.h>

#include <Xm/Xm.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/ScrollBar.h>

#include "misc_macros.h"

#include "Xh.h"
#include "Xh_plot.h"
#include "Xh_fallbacks.h"
#include "Xh_globals.h"
#include "Xh_viewport.h"

#include "plotdata.h"


enum
{
    LFRT_TICK_LEN = 5, BOTT_TICK_LEN = 5,
    LFRT_TICK_SPC = 1, BOTT_TICK_SPC = 1,
    LFRT_TICK_MRG = 2, BOTT_TICK_MRG = 2,
    V_TICK_STEP = 50,
    GRID_DASHLENGTH = 6,
};

typedef struct
{
    plotdata_t  *data;
    int          show;       // Should show on-screen
    int          magn;       // Magnification factor
    int          plrt;       // Polarity: +1,-1 (multiplicated with magn)
    char         descr [20];
    char         dpyfmt[20];
    char         units [10];

    GC           lineGC;
    GC           goodGC;
    XFontStruct *finfo;
} one_plot_t;

typedef struct _XhPlot_info_t_struct
{
    int                type;
    int                maxplots;
    void              *privptr;

    XhPlot_raw2pvl_t   raw2pvl;
    XhPlot_pvl2dsp_t   pvl2dsp;
    XhPlot_x2xs_t      x2xs;
    XhPlot_xs_t        xs;
    XhPlot_plotdraw_t  draw_one;
    XhPlot_set_rpr_t   set_rpr;

    Xh_viewport_t      vprt;
    XhWindow           top_win;

    GC                 axisGC;
    GC                 gridGC;
    GC                 reprGC;

    XFontStruct       *axis_finfo;
    XFontStruct       *repr_finfo;

    int                num_saved;
    int                maxpts;
    int                x_cmpr;
    int                x_xpnd;
    int                linemode;

    int                mouse_x;
    int                prev_mouse_x;

    int                rpr_on   [XH_PLOT_NUM_REPERS];
    int                rpr_at   [XH_PLOT_NUM_REPERS];
    int                rpr_press[XH_PLOT_NUM_REPERS];

    /* Must be the LAST field */
    one_plot_t         plots[0];    
} plot_info_t;


//// application-independent staff ///////////////////////////////////

static XFontStruct     *last_finfo;

static GC AllocXhGC(Widget w, int idx, const char *fontspec)
{
  XtGCMask   mask;  
  XGCValues  vals;
    
    mask = GCForeground | GCLineWidth;
    vals.foreground = XhGetColor(idx);
    vals.line_width = 0;

    if (fontspec != NULL  &&  fontspec[0] != '\0')
    {
        last_finfo = XLoadQueryFont(XtDisplay(w), fontspec);
        if (last_finfo == NULL)
        {
            fprintf(stderr, "Unable to load font \"%s\"; using \"fixed\"\n", fontspec);
            last_finfo = XLoadQueryFont(XtDisplay(w), "fixed");
        }
        vals.font = last_finfo->fid;

        mask |= GCFont;
    }

    return XtGetGC(w, mask, &vals);
}

static GC AllocThGC(Widget w, int idx, const char *fontspec)
{
  XtGCMask   mask;  
  XGCValues  vals;
    
    mask = GCForeground | GCLineWidth;
    vals.foreground = XhGetColor(idx);
    vals.line_width = 2;

    if (fontspec != NULL  &&  fontspec[0] != '\0')
    {
        last_finfo = XLoadQueryFont(XtDisplay(w), fontspec);
        if (last_finfo == NULL)
        {
            fprintf(stderr, "Unable to load font \"%s\"; using \"fixed\"\n", fontspec);
            last_finfo = XLoadQueryFont(XtDisplay(w), "fixed");
        }
        vals.font = last_finfo->fid;

        mask |= GCFont;
    }

    return XtGetGC(w, mask, &vals);
}

static GC AllocDashGC(Widget w, int idx, int dash_length)
{
  XtGCMask   mask;  
  XGCValues  vals;
    
    mask = GCForeground | GCLineWidth | GCLineStyle | GCDashList;
    vals.foreground = XhGetColor(idx);
    vals.line_width = 0;
    vals.line_style = LineOnOffDash;
    vals.dashes     = dash_length;

    return XtGetGC(w, mask, &vals);
}

typedef void (*do_flush_t)(Display *dpy, Window win, GC gc,
                           XPoint *points, int *npts_p);

static void FlushXLines(Display *dpy, Window win, GC gc,
                        XPoint *points, int *npts_p)
{
    if (*npts_p == 0) return;
    if (*npts_p == 1) XDrawPoint(dpy, win, gc, points[0].x, points[0].y);
    else              XDrawLines(dpy, win, gc, points, *npts_p, CoordModeOrigin);

    points[0] = points[(*npts_p) - 1];
    *npts_p = 1;
}

static void FlushXPoints(Display *dpy, Window win, GC gc,
                         XPoint *points, int *npts_p)
{
    if (*npts_p == 0) return;
    XDrawPoints(dpy, win, gc, points, *npts_p, CoordModeOrigin);

    points[0] = points[(*npts_p) - 1];
    *npts_p = 1;
}

//////////////////////////////////////////////////////////////////////

static void DrawPlotView  (plot_info_t *plot)
{
  Display   *dpy = XtDisplay(plot->vprt.view);
  Window     win = XtWindow (plot->vprt.view);

  Dimension  grf_w;
  Dimension  grf_h;

  int        i;
  int        x;
  int        y;

  int        dash_ofs;

  int        v_tick_segs;

  int        stage;
  int        nl;
  GC         gc;
  int        magn;

  int        nr;
  char       rns;

    XtVaGetValues(plot->vprt.view, XmNwidth, &grf_w, XmNheight, &grf_h, NULL);

    /* Grid */
    /* Vertical lines */
    for (x = 100 - (plot->vprt.horz_offset % 100);  x < grf_w;  x += 100)
    {
        XDrawLine(dpy, win, plot->gridGC,
                  x, 0, x, grf_h-1);
    }
    /* Horizontal lines */
    v_tick_segs = ((grf_h - V_TICK_STEP*2) / (V_TICK_STEP*2)) * 2 + 2;
    dash_ofs = plot->vprt.horz_offset % (GRID_DASHLENGTH * 2);
    for (i = 2;  i <= v_tick_segs - 2;  i += 2)
    {
        y = grf_h-1 - RESCALE_VALUE(i, 0, v_tick_segs, 0, grf_h-1);
        
        XDrawLine(dpy, win, plot->gridGC,
                  -dash_ofs, y, grf_w-1, y);
    }

    /* Repers */
    for (nr = 0;  nr < XH_PLOT_NUM_REPERS;  nr++)
        if (plot->rpr_on[nr])
        {
            x = scale32via64(plot->rpr_at[nr], plot->x_xpnd, plot->x_cmpr)
                -
                plot->vprt.horz_offset;
            if (x >= 0  &&  x < grf_w)
            {
                XDrawLine(dpy, win, plot->reprGC,
                          x, 0, x, grf_h-1);
                rns = '1' + nr;
                XDrawString(dpy, win, plot->reprGC,
                            x + 2, 0 + plot->repr_finfo->ascent,
                            &rns, 1);
            }
        }

    /* Data */
    for (stage = plot->num_saved;
         stage >= 0;
         stage--)
        for (nl = 0;  nl < plot->maxplots;  nl++)
        {
            if (plot->plots[nl].show          &&
                plot->plots[nl].data != NULL  &&
                plot->plots[nl].data->on)
            {
                magn = plot->plots[nl].magn * plot->plots[nl].plrt;
                gc = stage > 0? plot->plots[nl].goodGC : plot->plots[nl].lineGC;
                if      (plot->draw_one != NULL)
                         plot->draw_one(plot->privptr, nl, stage,
                                        magn, gc);
                else if (stage == 0) /* Builtin-draw shows only current plots */
                    XhPlotOneDraw(plot,
                                  plot->plots[nl].data,
                                  magn, gc);
            }
        }
}

static void DrawPlotAxis  (plot_info_t *plot)
{
  Display   *dpy = XtDisplay(plot->vprt.axis);
  Window     win = XtWindow (plot->vprt.axis);

  Dimension  cur_w;
  Dimension  cur_h;
  int        grf_w;
  int        grf_h;

  typedef enum
  {
      VLPLACE_BOTH    = 0,
      VLPLACE_EVENODD = 1,
      VLPLACE_HALVES  = 2
  } vlplace_t; // "vl" -- Vertical axis Legend
  vlplace_t  vlplace;
  int        lr;
  int        num_lr[2];
  int        h_lr  [2];
  int        y_lr  [2];

  int        limit;
  int        i;
  int        x;
  int        y;
  int        tick_len;
  char       buf[400];
  int        frsize;

  int        v_tick_segs;
  int        h;
  int        even;
  int        nl;
  int        num_on;
  int        num_used;
  one_plot_t  *one;
  double       val;
  char        *p;
  int          twid;


    XtVaGetValues(plot->vprt.axis, XmNwidth, &cur_w, XmNheight, &cur_h, NULL);

    grf_w = cur_w - plot->vprt.m_lft - plot->vprt.m_rgt;
    grf_h = cur_h - plot->vprt.m_top - plot->vprt.m_bot;

    if (grf_w <= 0  ||  grf_h <= 0) return;


    /* The frame */
    
    XDrawRectangle(dpy, win, plot->axisGC,
                   plot->vprt.m_lft - 1, plot->vprt.m_top - 1,
                   grf_w            + 1, grf_h            + 1);


    /* Horizontal axis */
    
    /* Note: 'i' goes in SCREEN coords, not in counts */    
    limit = grf_w + plot->vprt.horz_offset;

    for (i = 0;  i < limit;  i += 10)
    {
        x = i - plot->vprt.horz_offset;
        if (x < 0)      continue; /*!!! Should start i from somewhere more near */

        if      ((i % 100) == 0) tick_len = BOTT_TICK_LEN;
        else if ((i % 50)  == 0) tick_len = BOTT_TICK_LEN / 2 + 1;
        else                     tick_len = 2;

        XDrawLine(dpy, win, plot->axisGC,
                  plot->vprt.m_lft + x, plot->vprt.m_top - tick_len,
                  plot->vprt.m_lft + x, plot->vprt.m_top - 1);
        XDrawLine(dpy, win, plot->axisGC,
                  plot->vprt.m_lft + x, plot->vprt.m_top + grf_h + 0,
                  plot->vprt.m_lft + x, plot->vprt.m_top + grf_h + tick_len - 1);

        if ((i % 100) == 0)
        {
            snprintf(buf, sizeof(buf), "%d%s",
                     plot->x2xs(plot->privptr,
                                scale32via64(i, plot->x_cmpr, plot->x_xpnd)),
                     plot->xs  (plot->privptr));
            XDrawString(dpy, win, plot->axisGC,
                        plot->vprt.m_lft + x - XTextWidth(plot->axis_finfo, buf, strlen(buf)) / 2,
                        plot->vprt.m_top         - BOTT_TICK_LEN - BOTT_TICK_SPC - plot->axis_finfo->descent,
                        buf, strlen(buf));
            XDrawString(dpy, win, plot->axisGC,
                        plot->vprt.m_lft + x - XTextWidth(plot->axis_finfo, buf, strlen(buf)) / 2,
                        plot->vprt.m_top + grf_h + BOTT_TICK_LEN + BOTT_TICK_SPC + plot->axis_finfo->ascent,
                        buf, strlen(buf));
        }
    }


    /* Vertical axis */

    /* Count visible rows */
    for (nl = 0, num_on = 0;  nl < plot->maxplots;  nl++)
        num_on += (plot->plots[nl].show != 0     &&
                   plot->plots[nl].data != NULL  &&
                   plot->plots[nl].data->on);

    /* VL-place initialization */
    vlplace = VLPLACE_BOTH;
    num_lr[0] = num_lr[1] = num_on;
    if (num_on > 6)
    {
        vlplace = 0? VLPLACE_EVENODD : VLPLACE_HALVES;
        num_lr[1] = num_on / 2;  // With this order more rows go to left
        num_lr[0] = num_on - num_lr[1];
    }

    v_tick_segs = ((grf_h - V_TICK_STEP*2) / (V_TICK_STEP*2)) * 2 + 2;
    for (i = 0;  i <= v_tick_segs;  i++)
    {
        h = RESCALE_VALUE(i, 0, v_tick_segs, 0, grf_h-1);
        y = plot->vprt.m_top + grf_h - 1 - h;
        even = ((i % 2) == 0);
        if (even) tick_len = LFRT_TICK_LEN;
        else      tick_len = LFRT_TICK_LEN / 2 + 1;

        XDrawLine(dpy, win, plot->axisGC,
                  plot->vprt.m_lft - tick_len, y,
                  plot->vprt.m_lft - 1,        y);
        XDrawLine(dpy, win, plot->axisGC,
                  plot->vprt.m_lft + grf_w,                y,
                  plot->vprt.m_lft + grf_w + tick_len - 1, y);

        if (even)
        {
            for (lr = 0;  lr <= 1;  lr++)
            {
                h_lr[lr] = (plot->plots[0].finfo->ascent + plot->plots[0].finfo->descent)
                    * num_lr[lr];
                y_lr[lr] = y - h_lr[lr] / 2;
                if (y_lr[lr] > cur_h - plot->vprt.m_bot - h_lr[lr])
                    y_lr[lr] = cur_h - plot->vprt.m_bot - h_lr[lr];
                if (y_lr[lr] < plot->vprt.m_top)
                    y_lr[lr] = plot->vprt.m_top;
            }

            for (nl = 0, one = plot->plots + nl, num_used = 0;
                 nl < plot->maxplots;
                 nl++,   one++)
                if (one->show          &&
                    one->data != NULL  &&
                    one->data->on)
                {
                    if (reprof_cxdtype(one->data->x_buf_dtype) == CXDTYPE_REPR_INT)
                        val = plot->raw2pvl
                              (plot->privptr, nl,
                               RESCALE_VALUE((int64)i,
                                             0, v_tick_segs,
                                             one->data->cur_range.int_r[0],
                                             one->data->cur_range.int_r[1])
                              );
                    else
                        val = RESCALE_VALUE(i,
                                            0, v_tick_segs,
                                            one->data->cur_range.dbl_r[0],
                                            one->data->cur_range.dbl_r[1]);
                    val = plot->pvl2dsp(plot->privptr, nl, val) / one->magn * one->plrt;

                    p = snprintf_dbl_trim(buf, sizeof(buf), one->dpyfmt, val, 1);
                    frsize = sizeof(buf)-1 - (p - buf) - strlen(p);
                    if (frsize > 0) snprintf(p + strlen(p), frsize, "%s", one->units);
                    twid = XTextWidth(one->finfo, p, strlen(p));

                    for (lr = 0;  lr <= 1;  lr++)
                        if (vlplace == VLPLACE_BOTH
                            ||
                            (
                             vlplace == VLPLACE_EVENODD  &&
                             (num_used & 1) == (lr & 1)
                            )
                            ||
                            (
                             vlplace == VLPLACE_HALVES  &&
                             (
                              (lr == 0  &&  num_used <  num_lr[0])  ||
                              (lr == 1  &&  num_used >= num_lr[0])
                             )
                            )
                           )
                        {
                            if (lr == 0) x = plot->vprt.m_lft -         LFRT_TICK_LEN - LFRT_TICK_SPC - twid;
                            else         x = plot->vprt.m_lft + grf_w + LFRT_TICK_LEN + LFRT_TICK_SPC;

                            XDrawString(dpy, win, one->lineGC,
                                        x,
                                        y_lr[lr] + one->finfo->ascent,
                                        p, strlen(p));

                            y_lr[lr] += one->finfo->ascent + one->finfo->descent;
                        }

                    num_used++;
                }
        }
    }    
}

static double GetDsp(plot_info_t *plot, int nl, int x)
{
  plotdata_t *pd  = plot->plots[nl].data;
  double      val;

    if (reprof_cxdtype(pd->x_buf_dtype) == CXDTYPE_REPR_INT)
        val = plot->raw2pvl(plot->privptr, nl, plotdata_get_int(pd, x));
    else
        val =                                  plotdata_get_dbl(pd, x);

    return plot->pvl2dsp(plot->privptr, nl, val);
}

static void ShowMouseStats(plot_info_t *plot, int x)
{
  int         nl;
  char        numbuf[100];
  char       *p;
  char        buf[1000];
  int         curlen;
  int         frsize;
  int         snplen;

    if (x < 0  &&  plot->prev_mouse_x >= 0) XhMakeTempMessage(plot->top_win, NULL);

    if (x >= 0)
    {
        curlen = sprintf(buf, "%d%s:",
                         plot->x2xs(plot->privptr, x), plot->xs(plot->privptr));

        for (nl = 0;  nl < plot->maxplots;  nl++)
            if (plot->plots[nl].show          &&
                plot->plots[nl].data != NULL  &&
                plot->plots[nl].data->on      &&
                plot->plots[nl].data->numpts > x)
            {
                p = snprintf_dbl_trim(numbuf, sizeof(numbuf),
                                      plot->plots[nl].dpyfmt,
                                      GetDsp(plot, nl, x), 1);

                frsize = sizeof(buf)-1 - curlen;
                if (frsize < 1) break;
                snplen = snprintf(buf + curlen, frsize,
                                  " %s=%s%s",
                                  plot->plots[nl].descr,
                                  p,
                                  plot->plots[nl].units);
                if (snplen < 0  ||  snplen > frsize) break;
                curlen += snplen;
            }

        XhMakeTempMessage(plot->top_win, buf);
    }

    plot->prev_mouse_x = x;
}

static void PlotDrawProc  (Xh_viewport_t *vp, int parts)
{
  plot_info_t *plot = vp->privptr;

////fprintf(stderr, "%s(parts=%d)\n", __FUNCTION__, parts);
    if (parts & XH_VIEWPORT_VIEW)   DrawPlotView(plot);
    if (parts & XH_VIEWPORT_AXIS)   DrawPlotAxis(plot);
    if (parts & XH_VIEWPORT_STATS)  ShowMouseStats(plot, plot->mouse_x);
}

static void PlotMsevProc  (Xh_viewport_t *vp,
                           int ev_type, int mod_mask,
                           int bn, int x, int y)
{
  plot_info_t *plot = vp->privptr;

  int          nr;

    /* Take care of status line display */
    if (ev_type == XH_VIEWPORT_MSEV_LEAVE)
    {
        plot->mouse_x = -1;
    }
    else
    {
        plot->mouse_x = scale32via64(x + vp->horz_offset,
                                     plot->x_cmpr,
                                     plot->x_xpnd);
        //if (x > width) plot->mouse_x = -1;
    }
    ShowMouseStats(plot, plot->mouse_x);

    /* Repers... */
    if (ev_type == XH_VIEWPORT_MSEV_PRESS  &&  bn < XH_PLOT_NUM_REPERS)
        plot->rpr_press[bn] = 1;
    for (nr = 0;  nr < XH_PLOT_NUM_REPERS;  nr++)
        if (plot->rpr_press[nr]  &&  plot->mouse_x >= 0  &&
            plot->set_rpr != NULL)
            plot->set_rpr(plot->privptr, nr, plot->mouse_x);
    if (ev_type == XH_VIEWPORT_MSEV_RELEASE  &&  bn < XH_PLOT_NUM_REPERS)
        plot->rpr_press[bn] = 0;
}

//////////////////////////////////////////////////////////////////////

static double       builtin_raw2pvl (void *privptr, int nl, int raw)
{
    return (double)raw;
}

static double       builtin_pvl2dsp (void *privptr, int nl, double pvl)
{
    return pvl;
}

static int          builtin_x2xs    (void *privptr, int x)
{
    return x;
}

static const char * builtin_xs      (void *privptr)
{
    return "?s";
}

XhPlot    XhCreatePlot(Widget parent, int type, int maxplots,
                       void              *privptr,
                       XhPlot_raw2pvl_t   raw2pvl,
                       XhPlot_pvl2dsp_t   pvl2dsp,
                       XhPlot_x2xs_t      x2xs,
                       XhPlot_xs_t        xs,
                       XhPlot_plotdraw_t  draw_one,
                       XhPlot_set_rpr_t   set_rpr,
                       int init_w, int init_h, int options, int black,
                       ...)
{
  plot_info_t *plot;
  size_t       plot_size = sizeof(*plot) + sizeof(plot->plots[0]) * maxplots;

  int          loc = options & XH_PLOT_CPANEL_MASK;
  int          parts;

  CxPixel      bg;
  int          cofs = black? (XH_COLOR_BGRAPH_BG - XH_COLOR_GRAPH_BG) : 0;
  int          nl;

    if (maxplots <= 0)
    {
        fprintf(stderr, "%s(): maxplots=%d, nonsense.\n",
                __FUNCTION__, maxplots);
        return NULL;
    }

    if (raw2pvl  == NULL) raw2pvl  = builtin_raw2pvl;
    if (pvl2dsp  == NULL) pvl2dsp  = builtin_pvl2dsp;
    if (x2xs     == NULL) x2xs     = builtin_x2xs;
    if (xs       == NULL) xs       = builtin_xs;

    /* Create & init a basic record */
    plot = malloc(plot_size);
    if (plot == NULL) return NULL;
    bzero(plot, plot_size);
    plot->type     = type;
    plot->maxplots = maxplots;

    plot->privptr  = privptr;
    plot->raw2pvl  = raw2pvl;
    plot->pvl2dsp  = pvl2dsp;
    plot->x2xs     = x2xs;
    plot->xs       = xs;
    plot->draw_one = draw_one;
    plot->set_rpr  = set_rpr;

    plot->x_cmpr   = 1;
    plot->x_xpnd   = 1;

    plot->mouse_x  = plot->prev_mouse_x = -1;

    /* Create a viewport */
    parts = 
        XH_VIEWPORT_HORZBAR | XH_VIEWPORT_VERTBAR*0 |
        XH_VIEWPORT_CPANEL
        ;
    if (options & XH_PLOT_NO_CPANEL_FOLD_MASK)
        parts |=  XH_VIEWPORT_NOFOLD;
    if (options & XH_PLOT_NO_SCROLLBAR_MASK)
        parts &=~ XH_VIEWPORT_HORZBAR | XH_VIEWPORT_VERTBAR;
    if      (loc == XH_PLOT_CPANEL_AT_LEFT)   parts |= XH_VIEWPORT_CPANEL_AT_LEFT;
    else if (loc == XH_PLOT_CPANEL_AT_RIGHT)  parts |= XH_VIEWPORT_CPANEL_AT_RIGHT;
    else if (loc == XH_PLOT_CPANEL_AT_TOP)    parts |= XH_VIEWPORT_CPANEL_AT_TOP;
    else if (loc == XH_PLOT_CPANEL_AT_BOTTOM) parts |= XH_VIEWPORT_CPANEL_AT_BOTTOM;
    else                                      parts &=~ XH_VIEWPORT_CPANEL;
    XhViewportCreate(&(plot->vprt), parent,
                     plot, PlotDrawProc, PlotMsevProc,
                     parts,
                     init_w, init_h, cofs + XH_COLOR_GRAPH_BG,
                     30, 30, 10, 10);

    plot->top_win = XhWindowOf(parent);

    /* Create GCs */
    for (nl = 0;  nl < plot->maxplots;  nl++)
    {
        plot->plots[nl].lineGC = (((options & XH_PLOT_WIDE_MASK) == 0)? 
                                  AllocXhGC : AllocThGC)
                                            (parent, cofs + XH_COLOR_GRAPH_LINE1 + (nl % XH_NUM_DISTINCT_LINE_COLORS), 
                                                                                   XH_TINY_FIXED_FONT);   plot->plots[nl].finfo = last_finfo;
        plot->plots[nl].goodGC = (((options & XH_PLOT_WIDE_MASK) == 0)? 
                                  AllocXhGC : AllocThGC)
                                            (parent, cofs + XH_COLOR_GRAPH_PREV,   NULL);
        plot->plots[nl].magn   = 1;
        plot->plots[nl].plrt   = +1;
    }
    plot->axisGC               = AllocXhGC  (parent, cofs + XH_COLOR_GRAPH_AXIS,   XH_TINY_FIXED_FONT);   plot->axis_finfo      = last_finfo;
    plot->gridGC               = AllocDashGC(parent, cofs + XH_COLOR_GRAPH_GRID,   GRID_DASHLENGTH);
    plot->reprGC               = AllocXhGC  (parent, cofs + XH_COLOR_GRAPH_REPERS, XH_PROPORTIONAL_FONT); plot->repr_finfo      = last_finfo;

    return plot;
}

void      XhPlotCalcMargins(XhPlot plot)
{
  int                  lfrt_cwdt;
  int                  bott_chgt;

  int                  nl;
  one_plot_t          *one;
  int                  minmax;
  double               val;
  char                 buf[400];
  int                  frsize;
  int                  cwdt;

  int                  m_h;
  int                  m_v;

    lfrt_cwdt = 0;
    for (nl = 0, one = plot->plots + nl;  nl < plot->maxplots;  nl++, one++)
        if (one->data != NULL)
            for (minmax = 0;  minmax <= 1;  minmax++)
            {
                if (reprof_cxdtype(one->data->x_buf_dtype) == CXDTYPE_REPR_INT)
                    val = plot->raw2pvl(plot->privptr, nl,
                                        one->data->all_range.int_r[minmax]);
                else
                    val =               one->data->all_range.dbl_r[minmax];
                val = plot->pvl2dsp(plot->privptr, nl, val);

                snprintf_dbl_trim(buf, sizeof(buf), 
                                  one->dpyfmt, val * one->plrt, 0);
                frsize = sizeof(buf)-1 - strlen(buf);
                if (frsize > 0)
                    snprintf(buf + strlen(buf), frsize, "%s", one->units);

                cwdt = XTextWidth(one->finfo, buf, strlen(buf));
                if (lfrt_cwdt < cwdt) lfrt_cwdt = cwdt;
            }

    bott_chgt = plot->axis_finfo->ascent + plot->axis_finfo->descent;

    m_h = LFRT_TICK_LEN + LFRT_TICK_SPC + lfrt_cwdt + LFRT_TICK_MRG;
    m_v = BOTT_TICK_LEN + BOTT_TICK_SPC + bott_chgt + BOTT_TICK_MRG;

    XhViewportSetMargins(&(plot->vprt), m_h, m_h, m_v, m_v);
}

void      XhPlotUpdate     (XhPlot plot, int with_axis)
{
    XhViewportUpdate(&(plot->vprt),
                     XH_VIEWPORT_ALL &~ (with_axis? 0 : XH_VIEWPORT_AXIS));
}

int       XhPlotSetMaxPts  (XhPlot plot, int maxpts)
{
    if (maxpts < 0) maxpts = 0;

    plot->maxpts            = maxpts;
    plot->vprt.horz_maximum = scale32via64(plot->maxpts, plot->x_xpnd, plot->x_cmpr);
    return XhViewportSetHorzbarParams(&(plot->vprt));
}

void      XhPlotSetCmpr    (XhPlot plot, int cmpr)
{
    //if (cmpr < 1) cmpr = 1;
    if (cmpr == 0) cmpr = 1;

    if (cmpr > 0)
    {
        plot->x_cmpr = +cmpr;
        plot->x_xpnd = 1;
    }
    else
    {
        plot->x_cmpr = 1;
        plot->x_xpnd = -cmpr;
    }
    plot->vprt.horz_maximum = scale32via64(plot->maxpts, plot->x_xpnd, plot->x_cmpr);
    XhViewportSetHorzbarParams(&(plot->vprt));
    XhPlotUpdate(plot, 1);
}

void      XhPlotSetNumSaved(XhPlot plot, int num_saved)
{
    if (num_saved < 0) num_saved = 0;
    plot->num_saved = num_saved;
}

void      XhPlotSetReper   (XhPlot plot, int nr, int x)
{
    if (nr < 0  ||  nr > XH_PLOT_NUM_REPERS) return;

    if (x < 0) plot->rpr_on[nr] = 0;
    else
    {
        plot->rpr_on[nr] = 1;
        plot->rpr_at[nr] = x;
    }
    XhViewportUpdate(&(plot->vprt), XH_VIEWPORT_VIEW);
}

void      XhPlotSetLineMode(XhPlot plot, int mode)
{
    plot->linemode = mode;
    XhViewportUpdate(&(plot->vprt), XH_VIEWPORT_VIEW);
}


int       XhPlotOneSetData(XhPlot plot, int n, plotdata_t *data)
{
  one_plot_t *one = plot->plots + n;

    if (n >= plot->maxplots) return -1;

    one->data = data;

    return 0;
}

int       XhPlotOneSetShow(XhPlot plot, int n, int show)
{
  one_plot_t *one = plot->plots + n;

    if (n >= plot->maxplots) return -1;

    one->show = show != 0;

    return 0;
}

int       XhPlotOneSetMagn(XhPlot plot, int n, int magn)
{
  one_plot_t *one = plot->plots + n;

    if (n >= plot->maxplots) return -1;

    if (magn <= 0) magn = 1;
    one->magn = magn;

    return 0;
}

int       XhPlotOneSetInvp(XhPlot plot, int n, int invp)
{
  one_plot_t *one = plot->plots + n;

    if (n >= plot->maxplots) return -1;

    one->plrt = (invp != 0)? -1 : +1;

    return 0;
}

int       XhPlotOneSetStrs(XhPlot plot, int n,
                           const char *descr,
                           const char *dpyfmt,
                           const char *units)
{
  one_plot_t *one = plot->plots + n;

    if (n >= plot->maxplots) return -1;

    strzcpy(one->descr,  descr,  sizeof(one->descr));
    strzcpy(one->dpyfmt, dpyfmt, sizeof(one->dpyfmt));
    strzcpy(one->units,  units,  sizeof(one->units));

    return 0;
}

void      XhPlotOneDraw (XhPlot plot, plotdata_t *data, int magn, GC gc)
{
  Display   *dpy = XtDisplay(plot->vprt.view);
  Window     win = XtWindow (plot->vprt.view);

  Dimension  grf_w;
  Dimension  grf_h;

  int               lft_out; // LeFT OUTdent
  int               rgt_lim; // RiGhT LIMit
  int               data_ofs;

  int               is_int    = reprof_cxdtype(data->x_buf_dtype) == CXDTYPE_REPR_INT;
  int               is_usgn   = ((data->x_buf_dtype & CXDTYPE_USGN_MASK) != 0);
  size_t            dataunits = sizeof_cxdtype(data->x_buf_dtype);
  plotdata_range_t  cur_range = data->cur_range;

  int               i;
  int               x;
  int               x_ctr;
  uint8            *p8;
  int16            *p16;
  int32            *p32;
  float32          *f32;
  float64          *f64;

  int32             iv;
  float64           dv;
  int               y;

  enum       {PTSBUFSIZE = 1000};
  XPoint      points[PTSBUFSIZE];
  int         npoints;
  do_flush_t  do_flush;

    if (win == 0)    return;
    if (!(data->on)) return;
    XtVaGetValues(plot->vprt.view, XmNwidth, &grf_w, XmNheight, &grf_h, NULL);

    lft_out = (plot->vprt.horz_offset == 0  ||  plot->x_xpnd <= 1)? 0 : 1;
    rgt_lim = grf_w + plot->x_xpnd - 1;
    data_ofs = scale32via64(plot->vprt.horz_offset,
                            plot->x_cmpr,
                            plot->x_xpnd);

    do_flush = (plot->linemode == 1)? FlushXPoints : FlushXLines;

    if (data == NULL) return;
    p8  = (uint8 *)  (data->x_buf) + data_ofs;
    p16 = (int16 *)  (data->x_buf) + data_ofs;
    p32 = (int32 *)  (data->x_buf) + data_ofs;
    f32 = (float32 *)(data->x_buf) + data_ofs;
    f64 = (float64 *)(data->x_buf) + data_ofs;

    ////fprintf(stderr, "*%d /%d lft_out=%d horz_offset=%d data_ofs=%d x=%d\n", plot->x_xpnd, plot->x_cmpr, lft_out, plot->vprt.horz_offset, data_ofs, -lft_out*(plot->vprt.horz_offset%plot->x_xpnd));
    for (npoints = 0,          x_ctr = 0,
         i = data_ofs,         x = -lft_out*(plot->vprt.horz_offset%plot->x_xpnd);

         i < data->numpts  &&  x < rgt_lim;

         i++)
    {
        if (npoints == PTSBUFSIZE) do_flush(dpy, win, gc,
                                            points, &npoints);

        if (is_int)
        {
            if (is_usgn)
            {
                if      (dataunits == 4)    iv = (uint32)(*p32++);
                else if (dataunits == 2)    iv = (uint16)(*p16++);
                else /* (dataunits == 1) */ iv = (uint8) (*p8++);
            }
            else
            {
                if      (dataunits == 4)    iv = *p32++;
                else if (dataunits == 2)    iv = *p16++;
                else /* (dataunits == 1) */ iv = *p8++;
            }

            y = grf_h-1 - (int)(RESCALE_VALUE((int64)(iv - data->cur_int_zero) * magn + data->cur_int_zero,
                                              cur_range.int_r[0], cur_range.int_r[1],
                                              0,                  grf_h - 1));
        }
        else
        {
            if      (dataunits == 8)    dv = *f64++;
            else                        dv = *f32++;

            y = grf_h-1 - (int)(RESCALE_VALUE((dv) * magn,
                                              cur_range.dbl_r[0], cur_range.dbl_r[1],
                                              0,                  grf_h - 1));
        }
        if      (y < -32767) y = -32767;
        else if (y > +32767) y = +32767;

        points[npoints].y = y;
        points[npoints].x = x;
        npoints++;

        if (plot->x_cmpr > 1)
        {
            x_ctr++;
            if (x_ctr >= plot->x_cmpr)
            {
                x_ctr = 0;
                x++;
            }
        }
        else
            x += plot->x_xpnd;
    }

    do_flush(dpy, win, gc,
             points, &npoints);
}

void     *XhPlotPrivptr (XhPlot plot)
{
    return plot->privptr;
}

Widget    XhWidgetOfPlot(XhPlot plot)
{
    return plot->vprt.container;
}

Widget    XhAngleOfPlot (XhPlot plot)
{
    return plot->vprt.angle;
}

Widget    XhCpanelOfPlot(XhPlot plot)
{
    return plot->vprt.cpanel;
}


void      XhPlotSetCpanelState(XhPlot plot, int state)
{
    XhViewportSetCpanelState(&(plot->vprt), state);
}


void      XhPlotSetBG   (XhPlot plot, CxPixel bg)
{
    XhViewportSetBG(&(plot->vprt), bg);
}
