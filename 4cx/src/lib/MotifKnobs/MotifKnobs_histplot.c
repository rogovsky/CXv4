#include <stdio.h>
#include <ctype.h>

#include <math.h>

#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/PushB.h>
#include <Xm/ScrollBar.h>
#include <Xm/Text.h>
#include <Xm/ToggleB.h>

#include "misc_macros.h"
#include "misclib.h"

#include "datatreeP.h"
#include "Knobs.h"
#include "MotifKnobsP.h"
#include "MotifKnobs_histplot.h"

#include "Xh_globals.h"
#include "Xh_fallbacks.h"


#ifndef NAN
  #warning The NAN macro is undefined, using strtod("NAN") instead
  #define NAN strtod("NAN", NULL)
#endif

#include "pixmaps/btn_mini_save.xpm"


//////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////

static char *make_time_str(MotifKnobs_histplot_t *hp, int age, int with_msc)
{
  static char     buf[100];
  int             secs;
  struct timeval  at;
  struct tm      *st;

    if (hp->view_only  &&
        hp->timestamps_ring_used > age)
    {
        at = hp->timestamps_ring[hp->timestamps_ring_used - 1 - age];
        st = localtime(&(at.tv_sec));
        if (with_msc)
            sprintf(buf, "%02d:%02d:%02d.%03d",
                    st->tm_hour, st->tm_min, st->tm_sec, (int)(at.tv_usec / 1000));
        else
            sprintf(buf, "%02d:%02d:%02d",
                    st->tm_hour, st->tm_min, st->tm_sec);
    }
    else
    {
        secs = (((double)age) * hp->cyclesize_us / 1000000);
        
        if      (secs < 59)
            sprintf(buf,            "%d",                               -secs);
        else if (secs < 3600)
            sprintf(buf,      "-%d:%02d",               secs / 60,       secs % 60);
        else
            sprintf(buf, "-%d:%02d:%02d", secs / 3600, (secs / 60) % 60, secs % 60);
    }

    return buf;
}

//////////////////////////////////////////////////////////////////////

enum
{
    H_TICK_LEN      = 5,
    V_TICK_LEN      = 5,
    H_TICK_SPC      = 1,
    V_TICK_SPC      = 0,
    H_TICK_MRG      = 2,
    V_TICK_MRG      = 1,
    GRID_DASHLENGTH = 6,

    H_TICK_STEP = 30,
    V_TICK_SEGS = 8,
    V_TICK_STEP = 50,
};

static int x_scale_factors[] = {1, 10, 60};

/* Return 1 if sensible range was found and 0 if default range is in effect */
static int GetDispRange(DataKnob k, double *min_p, double *max_p)
{
  register int     ret = 1;
  register double  mindisp;
  register double  maxdisp;

    if      (k->u.k.num_params   >    DATAKNOB_PARAM_DISP_MAX  &&
             (mindisp = k->u.k.params[DATAKNOB_PARAM_DISP_MIN].value)
             <
             (maxdisp = k->u.k.params[DATAKNOB_PARAM_DISP_MAX].value))
    {/* Nothing to do, ranges are already got */}
    else if (k->u.k.num_params   >    DATAKNOB_PARAM_ALWD_MAX  &&
             (mindisp = k->u.k.params[DATAKNOB_PARAM_ALWD_MIN].value)
             <
             (maxdisp = k->u.k.params[DATAKNOB_PARAM_ALWD_MAX].value))
    {/* Nothing to do, ranges are already got */}
    else if (k->u.k.cl_ref <= 0 /* simulation of !cda_ref_is_sensible() */  &&
             k->u.k.num_params   >    DATAKNOB_PARAM_NORM_MAX  &&
             (mindisp = k->u.k.params[DATAKNOB_PARAM_NORM_MIN].value)
             <
             (maxdisp = k->u.k.params[DATAKNOB_PARAM_NORM_MAX].value))
    {/* Nothing to do, ranges are already got */}
    else if (k->u.k.cl_ref <= 0 /* simulation of !cda_ref_is_sensible() */  &&
             k->u.k.num_params   >    DATAKNOB_PARAM_YELW_MAX  &&
             (mindisp = k->u.k.params[DATAKNOB_PARAM_YELW_MIN].value)
             <
             (maxdisp = k->u.k.params[DATAKNOB_PARAM_YELW_MAX].value))
    {/* Nothing to do, ranges are already got */}
    else
    {
        ret     = 0;
        mindisp = -100.0;
        maxdisp = +100.0;
    }

    *min_p = mindisp;
    *max_p = maxdisp;

    return ret;
}

static int ShouldUseLog(DataKnob k)
{
  const   char *p = k->u.k.dpyfmt + strlen(k->u.k.dpyfmt) - 1;
  double  mindisp, maxdisp;

    GetDispRange(k, &mindisp, &maxdisp);

    return
      tolower(*p) == 'e'  &&
      mindisp >= 1e-100  &&  maxdisp >= 1e-100;
}

typedef void (*do_flush_t)(Display *dpy, Window win, GC gc,
                           XPoint *points, int *npts_p);

static void FlushXLines (Display *dpy, Window win, GC gc,
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

static void FlushXBlots(Display *dpy, Window win, GC gc,
                        XPoint *points, int *npts_p)
{
  int     n;
  XPoint  pts[5];
    
    if (*npts_p == 0) return;
    pts[1].x = +1; pts[1].y = +0;
    pts[2].x = -1; pts[2].y = +1;
    pts[3].x = -1; pts[3].y = -1;
    pts[4].x = +1; pts[4].y = -1;
    for (n = 0;  n < *npts_p;  n++)
    {
        pts[0].x = points[n].x;
        pts[0].y = points[n].y;
        XDrawLines(dpy, win, gc, pts, countof(pts), CoordModePrevious);
    }

    points[0] = points[(*npts_p) - 1];
    *npts_p = 1;
}

static void DrawGraph(MotifKnobs_histplot_t *hp, int do_clear)
{
  Display    *dpy = XtDisplay(hp->graph);
  Window      win = XtWindow (hp->graph);
  Dimension   grf_w;
  Dimension   grf_h;

  int         row;
  DataKnob    k;
  int         is_logarithmic;
  double      mindisp;
  double      maxdisp;

  int         dash_ofs;
  int         v_tick_segs;
  int         i;
  int         limit;
  int         scr_age;
  int         age;
  double      v;
  int         x;
  int         y;
  int         x_mul;
  int         x_div;

  enum       {PTSBUFSIZE = 1000};
  XPoint      points[PTSBUFSIZE];
  int         npoints;
  do_flush_t  do_flush;
  int         is_wide;

    if (win == 0) return; // A guard for resize-called-before-realized
    XtVaGetValues(hp->graph, XmNwidth, &grf_w, XmNheight, &grf_h, NULL);
  
    if (do_clear)
        XFillRectangle(dpy, win, hp->bkgdGC, 0, 0, grf_w, grf_h);

    do_flush = (hp->mode == MOTIFKNOBS_HISTPLOT_MODE_DOTS)? FlushXPoints : FlushXLines;
    if (hp->mode == MOTIFKNOBS_HISTPLOT_MODE_BLOT) do_flush = FlushXBlots;
    is_wide  = hp->mode == MOTIFKNOBS_HISTPLOT_MODE_WIDE;

    // Grid -- horizontal lines
    v_tick_segs = ((grf_h - V_TICK_STEP*2) / (V_TICK_STEP*2)) * 2 + 2;
    dash_ofs = hp->horz_offset % (GRID_DASHLENGTH * 2);
    for (i = 2;  i <= v_tick_segs - 2;  i += 2)
    {
        y = grf_h-1 - RESCALE_VALUE(i, 0, v_tick_segs, 0, grf_h-1);
        y = RESCALE_VALUE(i, 0, v_tick_segs, grf_h-1, 0);
        
        XDrawLine(dpy, win, hp->gridGC,
                  grf_w-1+dash_ofs, y, 0, y);
    }
    
    // Grid -- vertical lines
    for (scr_age = (H_TICK_STEP*2) - hp->horz_offset % (H_TICK_STEP*2);
         scr_age < grf_w;
         scr_age += H_TICK_STEP*2)
    {
        x = grf_w - 1 - scr_age;
        XDrawLine(dpy, win, hp->gridGC,
                  x, 0, x, grf_h-1);
    }

    // "Reper" -- only in static mode
    if ((hp->view_only || 0)  &&  hp->x_index >= 0)
    {
        x = grf_w - 1 - (hp->x_index / hp->x_scale - hp->horz_offset);
        XDrawLine(dpy, win, hp->axisGC,
                  x, 0, x, grf_h-1);
    }

    for (row = hp->rows_used - 1;  row >= 0;  row--)
    {
        if (!hp->show[row]) continue;
        
        k = hp->target[row];

        x_mul = 1;
        x_div = hp->x_scale;

        limit = (grf_w + hp->horz_offset) * hp->x_scale;
        if (limit > k->u.k.histring_used)
            limit = k->u.k.histring_used;

        GetDispRange(k, &mindisp, &maxdisp);
        is_logarithmic = ShouldUseLog(k);
        if (is_logarithmic)
        {
            ////fprintf(stderr, "LOG(%s)!\n", k->ident);
            mindisp = log(mindisp);
            maxdisp = log(maxdisp);
        }
        
        for (age = hp->horz_offset * hp->x_scale, npoints = 0;
             age < limit;
             age++)
        {
            v = k->u.k.histring[
                                (
                                 k->u.k.histring_start
                                 +
                                 (k->u.k.histring_used - 1 - age)
                                )
                                %
                                k->u.k.histring_len
                               ];
            if (is_logarithmic)
            {
                if (v <= 1e-100) v = log(1e-100);
                else             v = log(v);

                y = RESCALE_VALUE(v,
                                  mindisp, maxdisp,
                                  grf_h - 1, 0);
            }
            else
            {
                y = RESCALE_VALUE(v,
                                  mindisp, maxdisp,
                                  grf_h - 1, 0);
            }
            ////if (t == 0  &&  isnan(v)) fprintf(stderr, "%s v=nan y=%d\n", k->ident, y);
            if      (y < -32767) y = -32767;
            else if (y > +32767) y = +32767;
            x = grf_w - 1 - ((age * x_mul) / x_div) + hp->horz_offset;

            if (npoints == PTSBUFSIZE) do_flush(dpy, win, hp->chanGC[is_wide][row % XH_NUM_DISTINCT_LINE_COLORS],
                                                points, &npoints);
            points[npoints].x = x;
            points[npoints].y = y;
            npoints++;

            /*
             This is  for frqdiv>1; this check is performed AFTER adding
             the point for a line to be drawn towards it from previous point.
             */
            if (x < 0) break; 
        }
        
        do_flush(dpy, win, hp->chanGC[is_wide][row % XH_NUM_DISTINCT_LINE_COLORS],
                 points, &npoints);
    }
}

static void DrawAxis (MotifKnobs_histplot_t *hp, int do_clear)
{
  Display   *dpy = XtDisplay(hp->axis);
  Window     win = XtWindow (hp->axis);
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

  int        even;
  int        age;
  int        v_tick_segs;
  int        i;
  int        x;
  int        y;
  int        h;
  int        len;
  char       buf[1000];
  int        frsize;

  int        row;
  int        num_on;
  int        num_used;
  DataKnob   k;
  int        is_logarithmic;
  double     mindisp;
  double     maxdisp;
  double     v;
  char      *p;
  int        twid;

    ////fprintf(stderr, "%s(%p, %d)\n", __FUNCTION__, hp, do_clear);
    if (win == 0) return; // A guard for resize-called-before-realized
    XtVaGetValues(hp->axis, XmNwidth, &cur_w, XmNheight, &cur_h, NULL);

    if (do_clear)
        XFillRectangle(dpy, win, hp->bkgdGC, 0, 0, cur_w, cur_h);

    grf_w = cur_w - hp->m_lft - hp->m_rgt;
    grf_h = cur_h - hp->m_top - hp->m_bot;

    if (grf_w <= 0  ||  grf_h <= 0) return;

    XDrawRectangle(dpy, win, hp->axisGC,
                   hp->m_lft - 1, hp->m_top - 1,
                   grf_w + 1,      grf_h + 1);

    for (age = hp->horz_offset + H_TICK_STEP - 1, age -= age % H_TICK_STEP;
         age < hp->horz_offset + grf_w;
         age += H_TICK_STEP)
    {
        x = cur_w - hp->m_rgt - 1 - age + hp->horz_offset;
        even = (age % (H_TICK_STEP*2)) == 0;
        if (even) len = V_TICK_LEN;
        else      len = V_TICK_LEN / 2 + 1;

        XDrawLine(dpy, win, hp->axisGC,
                  x, hp->m_top - len,
                  x, hp->m_top - 1);
        XDrawLine(dpy, win, hp->axisGC,
                  x, hp->m_top + grf_h,
                  x, hp->m_top + grf_h + len - 1);

        if (even)
        {
            p = make_time_str(hp, age * hp->x_scale, 0);
            XDrawString(dpy, win, hp->axisGC,
                        x - XTextWidth(hp->axis_finfo, p, strlen(p)) / 2,
                        hp->m_top - V_TICK_LEN - V_TICK_SPC - hp->axis_finfo->descent,
                        p, strlen(p));
            XDrawString(dpy, win, hp->axisGC,
                        x - XTextWidth(hp->axis_finfo, p, strlen(p)) / 2,
                        hp->m_top + grf_h +
                            V_TICK_LEN + V_TICK_SPC + hp->axis_finfo->ascent,
                        p, strlen(p));
        }
    }

    /* Count visible rows */
    for (row = 0, num_on = 0;  row < hp->rows_used;  row++)
        num_on += hp->show[row];

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
        y = hp->m_top + grf_h - 1 - h;
        even = ((i % 2) == 0);
        if (even) len = H_TICK_LEN;
        else      len = H_TICK_LEN / 2 + 1;

        XDrawLine(dpy, win, hp->axisGC,
                  hp->m_lft - len, y,
                  hp->m_lft - 1,   y);
        XDrawLine(dpy, win, hp->axisGC,
                  hp->m_lft + grf_w,           y,
                  hp->m_lft + grf_w + len - 1, y);

        if (even)
        {
            for (lr = 0;  lr <= 1;  lr++)
            {
                h_lr[lr] = (hp->chan_finfo[0]->ascent + hp->chan_finfo[0]->descent)
                    * num_lr[lr];
                y_lr[lr] = y - h_lr[lr] /2;
                if (y_lr[lr] > cur_h - hp->m_bot - h_lr[lr])
                    y_lr[lr] = cur_h - hp->m_bot - h_lr[lr];
                if (y_lr[lr] < hp->m_top)
                    y_lr[lr] = hp->m_top;
            }

            for (row = 0, num_used = 0;  row < hp->rows_used;  row++)
            {
                if (!hp->show[row]) continue;

                k = hp->target[row];
                GetDispRange(k, &mindisp, &maxdisp);
                is_logarithmic = ShouldUseLog(k);
                if (is_logarithmic)
                {
                    ////fprintf(stderr, "LOG(%s)!\n", k->ident);
                    mindisp = log(mindisp);
                    maxdisp = log(maxdisp);
                }

                if (is_logarithmic)
                    v = exp(
                            RESCALE_VALUE(i,
                                          0,            v_tick_segs,
                                          log(mindisp), log(maxdisp))
                           );
                                          
                else
                    v =     RESCALE_VALUE(i,
                                          0,            v_tick_segs,
                                          mindisp,      maxdisp);

                p = snprintf_dbl_trim(buf, sizeof(buf), k->u.k.dpyfmt, v, 1);
                frsize = sizeof(buf)-1 - (p - buf) - strlen(p);
                if (frsize > 0  &&  k->u.k.units != NULL)
                    snprintf(p + strlen(p), frsize, "%s", k->u.k.units);
                twid = XTextWidth(hp->chan_finfo[row % XH_NUM_DISTINCT_LINE_COLORS], p, strlen(p));

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
                        if (lr == 0) x = hp->m_lft         - H_TICK_LEN - H_TICK_SPC - twid;
                        else         x = hp->m_lft + grf_w + H_TICK_LEN + H_TICK_SPC;

                        XDrawString(dpy, win, hp->chanGC[0][row % XH_NUM_DISTINCT_LINE_COLORS],
                                    x,
                                    y_lr[lr] + hp->chan_finfo[row % XH_NUM_DISTINCT_LINE_COLORS]->ascent,
                                    p, strlen(p));

                        y_lr[lr] += hp->chan_finfo[0]->ascent + hp->chan_finfo[0]->descent;
                    }

                num_used++;
            }
        }
    }
}

//////////////////////////////////////////////////////////////////////

static void SetHorzOffset(MotifKnobs_histplot_t *hp, int v)
{
    if ((v == 0) != (hp->horz_offset == 0))
    {
        XtVaSetValues(hp->horzbar,
                      XmNbackground, XhGetColor(v == 0? XH_COLOR_GRAPH_BG
                                                      : XH_COLOR_JUST_RED),
                      NULL);
    }
    hp->horz_offset = v;
}

static int SetHorzbarParams(MotifKnobs_histplot_t *hp)
{
  int  ret = 0;

  Dimension  v_span;
  int        n_maximum;
  int        n_slidersize;
  int        n_value;

    XtVaGetValues(hp->graph, XmNwidth, &v_span, NULL);

    n_maximum = hp->horz_maximum;
    if (n_maximum < v_span) n_maximum = v_span;

    n_slidersize = v_span;

    if (n_maximum    == hp->o_horzbar_maximum  &&
        n_slidersize == hp->o_horzbar_slidersize)
        return ret;
    hp->o_horzbar_maximum    = n_maximum;
    hp->o_horzbar_slidersize = n_slidersize;

    XtVaGetValues(hp->horzbar, XmNvalue, &n_value, NULL);
    if (n_value > n_maximum - n_slidersize)
    {
        SetHorzOffset(hp, n_value = n_maximum - n_slidersize);
        ret = 1;
    }

    XtVaSetValues(hp->horzbar,
                  XmNminimum,       0,
                  XmNmaximum,       n_maximum,
                  XmNsliderSize,    n_slidersize,
                  XmNvalue,         n_value,
                  XmNincrement,     1,
                  XmNpageIncrement, /*n_maximum / 10*/v_span,
                  NULL);

    return ret;
}

//////////////////////////////////////////////////////////////////////

static void GraphExposureCB(Widget     w,
                            XtPointer  closure,
                            XtPointer  call_data)
{
  MotifKnobs_histplot_t *hp = (MotifKnobs_histplot_t *) closure;

    if (hp->ignore_graph_expose) return;
    XhRemoveExposeEvents(ABSTRZE(w));
    DrawGraph(hp, False);
}

static void AxisExposureCB(Widget     w,
                           XtPointer  closure,
                           XtPointer  call_data)
{
  MotifKnobs_histplot_t *hp = (MotifKnobs_histplot_t *) closure;

    if (hp->ignore_axis_expose) return;
    XhRemoveExposeEvents(ABSTRZE(w));
    DrawAxis(hp, False);
}

static void GraphResizeCB(Widget     w,
                          XtPointer  closure,
                          XtPointer  call_data)
{
  MotifKnobs_histplot_t *hp = (MotifKnobs_histplot_t *) closure;

    XhAdjustPreferredSizeInForm(ABSTRZE(w));
    if (XhCompressConfigureEvents(ABSTRZE(w)) == 0)
    {
        hp->ignore_graph_expose = 0;
        SetHorzbarParams(hp);
        DrawGraph(hp, True);
    }
    else
        hp->ignore_graph_expose = 1;
}

static void AxisResizeCB(Widget     w,
                         XtPointer  closure,
                         XtPointer  call_data)
{
  MotifKnobs_histplot_t *hp = (MotifKnobs_histplot_t *) closure;

    XhAdjustPreferredSizeInForm(ABSTRZE(w));
    if (XhCompressConfigureEvents(ABSTRZE(w)) == 0)
    {
        hp->ignore_axis_expose = 0;
        DrawAxis(hp, True);
    }
    else
        hp->ignore_axis_expose = 1;
}

static void HorzScrollCB(Widget     w,
                         XtPointer  closure,
                         XtPointer  call_data)
{
  MotifKnobs_histplot_t *hp = closure;

  XmScrollBarCallbackStruct *info = (XmScrollBarCallbackStruct *)call_data;

   SetHorzOffset(hp, info->value);
   DrawGraph(hp, True);
   DrawAxis (hp, True);
}

static void set_x_index(MotifKnobs_histplot_t *hp, int idx);
static void PointerHandler(Widget     w,
                           XtPointer  closure,
                           XEvent    *event,
                           Boolean   *continue_to_dispatch)
{
  MotifKnobs_histplot_t *hp = closure;

  XMotionEvent      *mev = (XMotionEvent      *) event;
  XButtonEvent      *bev = (XButtonEvent      *) event;
  XCrossingEvent    *cev = (XEnterWindowEvent *) event;

  int                    x;
  int                    is_in;
  Dimension              grf_w;

    if      (event->type == MotionNotify)
    {
        x = mev->x;
        if ((mev->state & Button1Mask) == 0  &&  hp->view_only == 0) return;
        is_in = 1;
    }
    else if (event->type == ButtonPress  ||  event->type == ButtonRelease)
    {
        x = bev->x;
        is_in = (event->type == ButtonPress) || hp->view_only;
    }
    else if (event->type == LeaveNotify)
    {
        x = cev->x;
        is_in = 0;
    }
    else return;

    if (is_in)
    {
        XtVaGetValues(hp->graph, XmNwidth, &grf_w, NULL);
        x = ((grf_w - 1 - x) + hp->horz_offset) * hp->x_scale;
        set_x_index(hp, x >= 0? x : -1);
    }
    else
        set_x_index(hp, -1);
}

//////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////

#define GOOD_ATTACHMENTS 1
static void SetAttachments(MotifKnobs_histplot_t *hp)
{
#if GOOD_ATTACHMENTS
    XtVaSetValues(hp->grid,
                  XmNtopAttachment,     XmATTACH_FORM,
                  XmNrightAttachment,   XmATTACH_FORM,
                  XmNleftAttachment,    XmATTACH_NONE,
                  NULL);

    XtVaSetValues(hp->w_r,
                  XmNrightAttachment,   XmATTACH_WIDGET,
                  XmNrightWidget,       hp->grid,
                  XmNleftAttachment,    XmATTACH_NONE,
                  NULL);

    XtVaSetValues(hp->w_l,
                  XmNleftAttachment,    XmATTACH_FORM,
                  NULL);

    XtVaSetValues(hp->graph,
                  XmNleftAttachment,    XmATTACH_WIDGET,
                  XmNleftWidget,        hp->w_l,
                  XmNrightAttachment,   XmATTACH_WIDGET,
                  XmNrightWidget,       hp->w_r,
                  XmNtopAttachment,     XmATTACH_FORM,
                  XmNbottomAttachment,  XmATTACH_WIDGET,
                  XmNbottomWidget,      hp->horzbar,
                  NULL);

    XtVaSetValues(hp->axis,
                  XmNleftAttachment,    XmATTACH_OPPOSITE_WIDGET,
                  XmNleftWidget,        hp->w_l,
                  XmNrightAttachment,   XmATTACH_OPPOSITE_WIDGET,
                  XmNrightWidget,       hp->w_r,
                  XmNtopAttachment,     XmATTACH_FORM,
                  XmNbottomAttachment,  XmATTACH_WIDGET,
                  XmNbottomWidget,      hp->horzbar,
                  NULL);

    XtVaSetValues(hp->horzbar,
                  XmNleftAttachment,    XmATTACH_WIDGET,
                  XmNleftWidget,        hp->w_l,
                  XmNrightAttachment,   XmATTACH_WIDGET,
                  XmNrightWidget,       hp->w_r,
                  XmNbottomAttachment,  XmATTACH_FORM,
                  NULL);

    XtVaSetValues(hp->time_dpy,
                  XmNrightAttachment,   XmATTACH_FORM,
                  XmNtopAttachment,     XmATTACH_WIDGET,
                  XmNtopWidget,         hp->grid,
                  NULL);

#else
    XtVaSetValues(hp->grid,
                  XmNtopAttachment,     XmATTACH_FORM,
                  XmNrightAttachment,   XmATTACH_FORM,
                  NULL);

    XtVaSetValues(hp->axis,
                  XmNleftAttachment,    XmATTACH_FORM,
                  XmNrightAttachment,   XmATTACH_WIDGET,
                  XmNrightWidget,       hp->grid,
                  XmNtopAttachment,     XmATTACH_FORM,
                  XmNbottomAttachment,  XmATTACH_FORM,
                  NULL);

    XtVaSetValues(hp->graph,
                  XmNleftAttachment,    XmATTACH_FORM,
                  XmNrightAttachment,   XmATTACH_WIDGET,
                  XmNrightWidget,       hp->grid,
                  XmNtopAttachment,     XmATTACH_FORM,
                  XmNbottomAttachment,  XmATTACH_FORM,
                  NULL);
#endif
}

static void CalcSetMargins(MotifKnobs_histplot_t *hp)
{
  int       row;
  int       side;
  int       curw;
  int       maxw;
  DataKnob  k;
  double    mindisp;
  double    maxdisp;
  double    v;
  char      buf[1000];
  int       frsize;
  char     *p;
  
    for (row = 0, maxw = 0;
         row < hp->rows_used;
         row++)
    {
        k = hp->target[row];
        for (side = 0;  side <= 1;  side++)
        {
            if (k->u.k.num_params > DATAKNOB_PARAM_DISP_MAX  &&
                (mindisp = k->u.k.params[DATAKNOB_PARAM_DISP_MIN].value)
                <
                (maxdisp = k->u.k.params[DATAKNOB_PARAM_DISP_MAX].value))
            {}
            else
            {
                mindisp = -100;
                maxdisp = +100;
            }
            v = (side == 0)? mindisp : maxdisp;

            p = snprintf_dbl_trim(buf, sizeof(buf), k->u.k.dpyfmt, v, 1);
            frsize = sizeof(buf)-1 - strlen(buf);
            if (frsize > 0  &&  k->u.k.units != NULL)
                snprintf(p + strlen(p), frsize, "%s", k->u.k.units);

            curw = XTextWidth(hp->chan_finfo[row % XH_NUM_DISTINCT_LINE_COLORS], p, strlen(p));

            if (maxw < curw) maxw = curw;
        }
    }
////fprintf(stderr, "maxw=%d\n", maxw);
    
    hp->m_lft = hp->m_rgt = 
        H_TICK_MRG +
        maxw +
        H_TICK_SPC +
        H_TICK_LEN;
    hp->m_top = hp->m_bot =
        V_TICK_MRG +
        hp->axis_finfo->ascent + hp->axis_finfo->descent +
        V_TICK_SPC +
        V_TICK_LEN;

#if GOOD_ATTACHMENTS
    XtVaSetValues(hp->w_l,
                  XmNwidth, hp->m_lft,
                  NULL);
    XtVaSetValues(hp->w_r,
                  XmNwidth, hp->m_rgt,
                  NULL);
    XtVaSetValues(hp->graph,
                  XmNtopOffset,    hp->m_top,
                  XmNbottomOffset, hp->m_bot,
                  NULL);
#else
    XtVaSetValues(hp->graph,
                  XmNleftOffset,   hp->m_lft,
                  XmNrightOffset,  hp->m_rgt,
                  XmNtopOffset,    hp->m_top,
                  XmNbottomOffset, hp->m_bot,
                  NULL);
#endif
}

static void RenewPlotRow(MotifKnobs_histplot_t *hp, int  row,
                         DataKnob  k, int showness)
{
  char     *label;
  XmString  s;
  
  Boolean   is_set = showness? XmSET : XmUNSET;
  Boolean   cur_set;

    hp->target[row] = k;
    hp->show  [row] = showness;

    label = k->label;
    if (label == NULL  ||  label[0] == '\0') label = k->ident;
    if (label == NULL  ||  label[0] == '\0') label = " ";
    XtVaSetValues(hp->label[row],
                  XmNlabelString, s = XmStringCreateLtoR(label, xh_charset),
                  NULL);
    XmStringFree(s);

    XtVaGetValues(hp->label[row], XmNset, &cur_set, NULL);
    if (cur_set != is_set)
        XtVaSetValues(hp->label[row], XmNset, is_set, NULL);

    XtVaSetValues(hp->val_dpy[row],
                  XmNcolumns, GetTextColumns(k->u.k.dpyfmt, k->u.k.units),
                  NULL);
    hp->curstate[row] = KNOBSTATE_UNINITIALIZED;
}

static void UpdatePlotRow(MotifKnobs_histplot_t *hp, int  row)
{
  DataKnob     k = hp->target[row];
  knobstate_t  new_state;
  Pixel        fg;
  Pixel        bg;

    if      (hp->x_index < 0  &&  !hp->view_only)
        MotifKnobs_SetTextString(k, hp->val_dpy[row], k->u.k.curv);
    else if (hp->x_index < k->u.k.histring_used  &&
             hp->x_index >= 0)
        MotifKnobs_SetTextString(k, hp->val_dpy[row],
                                 k->u.k.histring[
                                                 (k->u.k.histring_start + (k->u.k.histring_used - 1 - hp->x_index))
                                                 %
                                                 k->u.k.histring_len
                                                ]);
    else
        XmTextSetString         (   hp->val_dpy[row], "");

    new_state = (hp->x_index < 0)? k->curstate : KNOBSTATE_NONE;
    if (new_state != hp->curstate[row])
    {
        MotifKnobs_ChooseColors(k->colz_style, new_state,
                                hp->deffg, hp->defbg,
                                &fg, &bg);
        XtVaSetValues(hp->val_dpy[row],
                      XmNforeground, fg,
                      XmNbackground, bg,
                      NULL);
        hp->curstate[row] = new_state;
    }
}

static void UpdateAllPlotRows(MotifKnobs_histplot_t *hp)
{
  int   row;
  
    for (row = 0;  row < hp->rows_used;  row++)
        UpdatePlotRow(hp, row);
}

static void set_x_index(MotifKnobs_histplot_t *hp, int idx)
{
    /* Update value of time_dpy */
    /* 1. Blank if state changes from "on" to "off" */
    if (idx < 0  &&  hp->x_index >= 0)
    {
        XmTextSetString(hp->time_dpy, "");
    }
    /* 2. Display time if required */
    if (idx >= 0)
    {
        XmTextSetString(hp->time_dpy, make_time_str(hp, idx, 1));
    }

#if 0
    /* Change visibility of time_dpy if negativeness of x_index changes */
    if      (idx >= 0  &&  hp->x_index <  0)
        XtManageChild  (hp->time_dpy);
    else if (idx <  0  &&  hp->x_index >= 0)
        XtUnmanageChild(hp->time_dpy);
#endif

    hp->x_index = idx;

    UpdateAllPlotRows(hp);
    if (hp->view_only) DrawGraph(hp, True);
}

static void SetPlotCount(MotifKnobs_histplot_t *hp, int count)
{
  int  row;
    
    hp->rows_used = count;
    
    if (count > 0)
    {
        XtManageChildren    (hp->label,   count);
        XtManageChildren    (hp->val_dpy, count);
        if (!hp->fixed)
        {
            XtManageChildren(hp->b_up,    count);
            XtManageChildren(hp->b_dn,    count);
            XtManageChildren(hp->b_rm,    count);
        }
    }

    if (count < MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX)
    {
        XtUnmanageChildren    (hp->label   + count, MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX - count);
        XtUnmanageChildren    (hp->val_dpy + count, MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX - count);
        if (!hp->fixed)
        {
            XtUnmanageChildren(hp->b_up    + count, MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX - count);
            XtUnmanageChildren(hp->b_dn    + count, MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX - count);
            XtUnmanageChildren(hp->b_rm    + count, MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX - count);
        }
    }
    
    for (row = 0;  row < count;  row++)
    {
        XhGridSetChildPosition    (hp->label  [row], 0, row);
        XhGridSetChildPosition    (hp->val_dpy[row], 1, row);
        if (!hp->fixed)
        {
            XhGridSetChildPosition(hp->b_up   [row], 2, row);
            XhGridSetChildPosition(hp->b_dn   [row], 3, row);
            XhGridSetChildPosition(hp->b_rm   [row], 4, row);
        }
    }

    XhGridSetSize(hp->grid, hp->fixed? 2 : 5, count);
}

static int CalcPlotParams(MotifKnobs_histplot_t *hp)
{
  int       row;
  int       maxpts;
  DataKnob  k;

    for (row = 0, maxpts = 0;  row < hp->rows_used;  row++)
    {
        k = hp->target[row];
        if (maxpts < k->u.k.histring_used)
            maxpts = k->u.k.histring_used;
    }
    
    hp->horz_maximum = maxpts / hp->x_scale;

    return SetHorzbarParams(hp);
}

static void DisplayPlot(MotifKnobs_histplot_t *hp)
{
    if (hp->x_index < 0) UpdateAllPlotRows(hp);

    CalcPlotParams(hp);
    
    DrawGraph(hp, True);
}

//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////

static int find_row(CxWidget widgets[], int count, CxWidget w)
{
  int  row;

    for (row = 0;  row < count;  row++)
        if (widgets[row] == w) return row;

    return -1;
}

// Individual-row callbacks

static void ShowOnOffCB(Widget     w,
                        XtPointer  closure,
                        XtPointer  call_data)
{
  MotifKnobs_histplot_t *hp  = closure;
  int                    row = find_row(hp->label, countof(hp->label), w);
  XmToggleButtonCallbackStruct *info = (XmToggleButtonCallbackStruct *) call_data;
  
    hp->show[row] = (info->set == XmUNSET? 0 : 1);
    
    DrawGraph     (hp, True);
    DrawAxis      (hp, True);
}

static void PlotMouse3Handler(Widget     w,
                              XtPointer  closure,
                              XEvent    *event,
                              Boolean   *continue_to_dispatch)
{
  MotifKnobs_histplot_t            *hp  = closure;
  int                               row;
  MotifKnobs_histplot_mb3_evinfo_t  info;

    row = find_row(hp->label, countof(hp->label), w);
    if (row < 0)
        row = find_row(hp->val_dpy, countof(hp->val_dpy), w);

    bzero(&info, sizeof(info));
    info.k                    = hp->target[row];
    info.event                = event;
    info.continue_to_dispatch = continue_to_dispatch;
    if (hp->evproc != NULL)
        hp->evproc(hp, hp->evproc_privptr, MOTIFKNOBS_HISTPLOT_REASON_MB3,
                   &info);
}
static void PlotUpCB(Widget     w,
                     XtPointer  closure,
                     XtPointer  call_data  __attribute__((unused)))
{
  MotifKnobs_histplot_t *hp  = closure;
  int                    row = find_row(hp->b_up, countof(hp->b_up), w);
  DataKnob               k_this;
  DataKnob               k_prev;
  int                    s_this;
  int                    s_prev;
  
    if (row < 0)  return;
    if (row == 0) return;
    k_this = hp->target[row];      s_this = hp->show[row];
    k_prev = hp->target[row - 1];  s_prev = hp->show[row - 1];
    RenewPlotRow  (hp, row,     k_prev, s_prev);
    RenewPlotRow  (hp, row - 1, k_this, s_this);
    UpdatePlotRow (hp, row);
    UpdatePlotRow (hp, row - 1);

    DrawGraph     (hp, True);
    DrawAxis      (hp, True);
}

static void PlotDnCB(Widget     w,
                     XtPointer  closure,
                     XtPointer  call_data  __attribute__((unused)))
{
  MotifKnobs_histplot_t *hp  = closure;
  int                    row = find_row(hp->b_dn, countof(hp->b_dn), w);
  DataKnob               k_this;
  DataKnob               k_next;
  int                    s_this;
  int                    s_next;

    if (row < 0)  return;
    if (row == hp->rows_used - 1) return;
    k_this = hp->target[row];      s_this = hp->show[row];
    k_next = hp->target[row + 1];  s_next = hp->show[row + 1];
    RenewPlotRow  (hp, row,     k_next, s_next);
    RenewPlotRow  (hp, row + 1, k_this, s_this);
    UpdatePlotRow (hp, row);
    UpdatePlotRow (hp, row + 1);

    DrawGraph     (hp, True);
    DrawAxis      (hp, True);
}

static void PlotRmCB(Widget     w,
                     XtPointer  closure,
                     XtPointer  call_data  __attribute__((unused)))
{
  MotifKnobs_histplot_t *hp  = closure;
  int                    row = find_row(hp->b_rm, countof(hp->b_rm), w);
  int                    y;
  
    if (row < 0)  return;
    if (hp->rows_used == 1)
        if (hp->evproc != NULL)
            hp->evproc(hp, hp->evproc_privptr, MOTIFKNOBS_HISTPLOT_REASON_CLOSE,
                       NULL);

    for (y = row;  y < hp->rows_used - 1;  y++)
    {
        RenewPlotRow (hp, y, hp->target[y + 1], hp->show[y + 1]);
        UpdatePlotRow(hp, y);
    }
    SetPlotCount(hp, hp->rows_used - 1);

    CalcSetMargins(hp);
    DrawGraph     (hp, True);
    DrawAxis      (hp, True);
}

// Histplot-wide callbacks

static void XScaleKCB(DataKnob k, double v, void *privptr)
{
  MotifKnobs_histplot_t *hp  = privptr;

    hp->x_scale = x_scale_factors[(int)(round(v))];
    CalcPlotParams(hp);
    DrawGraph(hp, True);
    DrawAxis (hp, True);
}

static void ModeKCB(DataKnob k, double v, void *privptr)
{
  MotifKnobs_histplot_t *hp  = privptr;

    hp->mode = (int)(round(v));
    DrawGraph(hp, True);
}

static void OkCB(Widget     w,
                 XtPointer  closure,
                 XtPointer  call_data  __attribute__((unused)))
{
  MotifKnobs_histplot_t *hp  = closure;
  
    if (hp->evproc != NULL)
        hp->evproc(hp, hp->evproc_privptr, MOTIFKNOBS_HISTPLOT_REASON_CLOSE,
                   NULL);
}

static void SaveCB(Widget     w,
                   XtPointer  closure,
                   XtPointer  call_data  __attribute__((unused)))
{
  MotifKnobs_histplot_t *hp  = closure;
  
    if (hp->evproc != NULL)
        hp->evproc(hp, hp->evproc_privptr, MOTIFKNOBS_HISTPLOT_REASON_SAVE,
                   NULL);
}

//////////////////////////////////////////////////////////////////////

static CxWidget MakePlotButton(MotifKnobs_histplot_t *hp,
                               CxWidget grid, int x, int row,
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
    XtAddCallback(b, XmNactivateCallback, callback, hp);

    return b;
}


//--------------------------------------------------------------------

int  MotifKnobs_CreateHistplot(MotifKnobs_histplot_t *hp,
                               Widget parent,
                               int def_w, int def_h,
                               int flags,
                               int cyclesize_us,
                               MotifKnobs_histplot_evproc_t  evproc,
                               void                         *evproc_privptr)
{
  CxPixel        bg  = XhGetColor(XH_COLOR_GRAPH_BG);
  Widget         bottom;
  DataKnob       k_md;
  Widget         xmod;
  DataKnob       k_xs;
  Widget         xscl;

  XmString       s;
  int            row;

    bzero(hp, sizeof(*hp));
    hp->cyclesize_us   = cyclesize_us;
    hp->evproc         = evproc;
    hp->evproc_privptr = evproc_privptr;

    hp->x_scale = x_scale_factors[0];

    hp->fixed = ((flags & MOTIFKNOBS_HISTPLOT_FLAG_FIXED) != 0);

    hp->view_only = ((flags & MOTIFKNOBS_HISTPLOT_FLAG_VIEW_ONLY) != 0);
    hp->x_index   = -1;

    hp->form = XtVaCreateManagedWidget("form", xmFormWidgetClass,
                                       parent,
                                       XmNbackground,      bg,
                                       XmNshadowThickness, 0,
                                       NULL);
    bottom = NULL;

    if ((flags & MOTIFKNOBS_HISTPLOT_FLAG_WITH_CLOSE_B) != 0)
    {
        hp->ok = XtVaCreateManagedWidget("ok", xmPushButtonWidgetClass, hp->form,
                                         XmNlabelString,      s = XmStringCreateLtoR("Close", xh_charset),
                                         XmNshowAsDefault,    1,
                                         XmNbottomAttachment, XmATTACH_FORM,
                                         XmNbottomOffset,     bottom != NULL? MOTIFKNOBS_INTERKNOB_V_SPACING : 0,
                                         XmNrightAttachment,  XmATTACH_FORM,
                                         XmNrightOffset,      MOTIFKNOBS_INTERKNOB_H_SPACING,
                                         NULL);
        XmStringFree(s);
        XtAddCallback(hp->ok, XmNactivateCallback, OkCB, hp);

        bottom = hp->ok;
    }

    if ((flags & MOTIFKNOBS_HISTPLOT_FLAG_NO_SAVE_BUTT) == 0)
    {
        hp->save = XtVaCreateManagedWidget("push_i", xmPushButtonWidgetClass, hp->form,
                                           XmNlabelString,      s = XmStringCreateLtoR("Save", xh_charset),
                                           XmNtraversalOn,      False,
                                           NULL);
        XmStringFree(s);
        XhAssignPixmap(hp->save, btn_mini_save_xpm);
        XhSetBalloon  (hp->save, "Save plots to file");
        XtAddCallback (hp->save, XmNactivateCallback, SaveCB, hp);


        if ((flags & MOTIFKNOBS_HISTPLOT_FLAG_HORZ_CTLS) == 0)
        {
            attachbottom (hp->save, bottom, bottom != NULL? MOTIFKNOBS_INTERKNOB_V_SPACING : 0);
            attachright  (hp->save, NULL,   MOTIFKNOBS_INTERKNOB_H_SPACING);
        }
        else
        {
            attachright  (hp->save, bottom, bottom != NULL? MOTIFKNOBS_INTERKNOB_V_SPACING : 0);
            attachbottom (hp->save, NULL,   MOTIFKNOBS_INTERKNOB_H_SPACING);
        }
        bottom = hp->save;
    }

    ////printf("%s: %d*%d\n", __FUNCTION__, def_w, def_h);
    if ((flags & MOTIFKNOBS_HISTPLOT_FLAG_NO_MODE_SWCH) == 0)
    {
        k_md = CreateSimpleKnob(" rw look=selector label=''"
                                " items='#T---\t===\t. . .\t***'",
                                0, hp->form,
                                ModeKCB, hp);
        xmod = GetKnobWidget(k_md);
        XtVaSetValues(xmod, XmNtraversalOn, False, NULL);

        if ((flags & MOTIFKNOBS_HISTPLOT_FLAG_HORZ_CTLS) == 0)
        {
            attachbottom (xmod, bottom, bottom != NULL? MOTIFKNOBS_INTERKNOB_V_SPACING : 0);
            attachright  (xmod, NULL,   MOTIFKNOBS_INTERKNOB_H_SPACING);
        }
        else
        {
            attachright  (xmod, bottom, bottom != NULL? MOTIFKNOBS_INTERKNOB_V_SPACING : 0);
            attachbottom (xmod, NULL,   MOTIFKNOBS_INTERKNOB_H_SPACING);
        }
        bottom = xmod;
    }

    if ((flags & MOTIFKNOBS_HISTPLOT_FLAG_NO_XSCL_SWCH) == 0)
    {
        k_xs = CreateSimpleKnob(" rw look=selector label=''"
                                " items='#T1:1\t1:10\t1:60'",
                                0, hp->form,
                                XScaleKCB, hp);
        xscl = GetKnobWidget(k_xs);
        XtVaSetValues(xscl, XmNtraversalOn, False, NULL);

        if ((flags & MOTIFKNOBS_HISTPLOT_FLAG_HORZ_CTLS) == 0)
        {
            attachbottom (xscl, bottom, bottom != NULL? MOTIFKNOBS_INTERKNOB_V_SPACING : 0);
            attachright  (xscl, NULL,   MOTIFKNOBS_INTERKNOB_H_SPACING);
        }
        else
        {
            attachright  (xscl, bottom, bottom != NULL? MOTIFKNOBS_INTERKNOB_V_SPACING : 0);
            attachbottom (xscl, NULL,   MOTIFKNOBS_INTERKNOB_H_SPACING);
        }
        bottom = xscl;
    }

    hp->bkgdGC    = AllocXhGC  (hp->form, XH_COLOR_GRAPH_BG,     NULL);
    hp->axisGC    = AllocXhGC  (hp->form, XH_COLOR_GRAPH_AXIS,   XH_TINY_FIXED_FONT); hp->axis_finfo = last_finfo;
    hp->gridGC    = AllocDashGC(hp->form, XH_COLOR_GRAPH_GRID,   GRID_DASHLENGTH);
    hp->reprGC    = AllocXhGC  (hp->form, XH_COLOR_GRAPH_REPERS, NULL);
    for (row = 0;
         row < MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX  &&
         row < XH_NUM_DISTINCT_LINE_COLORS;
         row++)
    {
        hp->chanGC [0][row] = AllocXhGC(hp->form, XH_COLOR_GRAPH_LINE1 + row, XH_TINY_FIXED_FONT);
        hp->chan_finfo[row] = last_finfo;
        hp->chanGC [1][row] = AllocThGC(hp->form, XH_COLOR_GRAPH_LINE1 + row, NULL);
    }

    /* Note:
           here we use the fact that widgets are displayed top-to-bottom
           in the order of creation: 'graph' is displayed above 'axis'
           because it is created earlier.
    */
    hp->graph = XtVaCreateManagedWidget("graph_graph", xmDrawingAreaWidgetClass,
                                        hp->form,
                                        XmNbackground,  bg,
                                        XmNwidth,       def_w,
                                        XmNheight,      def_h,
                                        XmNtraversalOn, False,
                                        NULL);
    XtAddCallback(hp->graph, XmNexposeCallback, GraphExposureCB, (XtPointer)hp);
    XtAddCallback(hp->graph, XmNresizeCallback, GraphResizeCB,   (XtPointer)hp);
    XtAddEventHandler(hp->graph,
                      EnterWindowMask | LeaveWindowMask | PointerMotionMask |
                      ButtonPressMask | ButtonReleaseMask |
                      Button1MotionMask,
                      False, PointerHandler, hp);

    hp->horzbar = XtVaCreateManagedWidget("horzbar", xmScrollBarWidgetClass,
                                          hp->form,
                                          XmNbackground,          bg,
                                          XmNorientation,         XmHORIZONTAL,
                                          XmNhighlightThickness,  0,
                                          XmNprocessingDirection, XmMAX_ON_LEFT,
                                          NULL);
    SetHorzbarParams(hp);
    XtAddCallback(hp->horzbar, XmNdragCallback,         HorzScrollCB, hp);
    XtAddCallback(hp->horzbar, XmNvalueChangedCallback, HorzScrollCB, hp);

    hp->axis  = XtVaCreateManagedWidget("graph_axis", xmDrawingAreaWidgetClass,
                                        hp->form,
                                        XmNbackground,  bg,
                                        XmNwidth,       def_w,
                                        XmNheight,      def_h,
                                        XmNtraversalOn, False,
                                        NULL);
    XtAddCallback(hp->axis,  XmNexposeCallback, AxisExposureCB,  (XtPointer)hp);
    XtAddCallback(hp->axis,  XmNresizeCallback, AxisResizeCB,    (XtPointer)hp);

    hp->w_l = XtVaCreateManagedWidget("", widgetClass, hp->form,
                                      XmNborderWidth, 0,
                                      XmNwidth,       1,
                                      XmNheight,      1,
                                      NULL);
    hp->w_r = XtVaCreateManagedWidget("", widgetClass, hp->form,
                                      XmNborderWidth, 0,
                                      XmNwidth,       1,
                                      XmNheight,      1,
                                      NULL);
    
    hp->grid = XhCreateGrid(hp->form, "grid");
    XhGridSetGrid   (hp->grid, 0,                              0);
    XhGridSetSpacing(hp->grid, MOTIFKNOBS_INTERKNOB_H_SPACING, MOTIFKNOBS_INTERKNOB_V_SPACING);
    XhGridSetPadding(hp->grid, 0,                              0);
    XtVaSetValues(hp->grid, XmNbackground, bg, NULL);

    for (row = 0;  row < MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX;  row++)
    {
        hp->label[row] =
            XtVaCreateWidget("onoff_o", xmToggleButtonWidgetClass, hp->grid,
                             XmNbackground,  bg,
                             XmNforeground,  XhGetColor(XH_COLOR_GRAPH_LINE1 + (row % XH_NUM_DISTINCT_LINE_COLORS)),
                             XmNselectColor, XhGetColor(XH_COLOR_GRAPH_LINE1 + (row % XH_NUM_DISTINCT_LINE_COLORS)),
                             XmNlabelString, s = XmStringCreateLtoR("", xh_charset),
                             XmNtraversalOn, False,
                             NULL);
        XmStringFree(s);
        XhGridSetChildPosition (hp->label[row],  0,  row);
        XhGridSetChildFilling  (hp->label[row],  0,  0);
        XhGridSetChildAlignment(hp->label[row], -1, -1);
        XtAddCallback(hp->label[row], XmNvalueChangedCallback, ShowOnOffCB, hp);

        hp->val_dpy[row] =
            XtVaCreateWidget("text_o", xmTextWidgetClass, hp->grid,
                             XmNbackground,            bg,
                             XmNscrollHorizontal,      False,
                             XmNcursorPositionVisible, False,
                             XmNeditable,              False,
                             XmNtraversalOn,           False,
                             NULL);
        XhGridSetChildPosition (hp->val_dpy[row],  1,  row);
        XhGridSetChildFilling  (hp->val_dpy[row],  0,  0);
        XhGridSetChildAlignment(hp->val_dpy[row], -1, -1);
        
        XtInsertEventHandler(hp->label  [row], ButtonPressMask, False, PlotMouse3Handler, hp, XtListHead);
        XtInsertEventHandler(hp->val_dpy[row], ButtonPressMask, False, PlotMouse3Handler, hp, XtListHead);

        if (!hp->fixed)
        {
            hp->b_up[row] = MakePlotButton(hp, hp->grid, 2, row, "^", PlotUpCB);
            hp->b_dn[row] = MakePlotButton(hp, hp->grid, 3, row, "v", PlotDnCB);
            hp->b_rm[row] = MakePlotButton(hp, hp->grid, 4, row, "-", PlotRmCB);
        }
    }

    XtVaGetValues(hp->val_dpy[0],
                  XmNforeground, &(hp->deffg),
                  XmNbackground, &(hp->defbg),
                  NULL);
    
    hp->time_dpy  =
        XtVaCreateManagedWidget("text_o", xmTextWidgetClass, hp->form,
                                XmNbackground,            bg,
                                XmNscrollHorizontal,      False,
                                XmNcursorPositionVisible, False,
                                XmNcolumns,               12, // "HH:MM:SS.nnn"
                                XmNeditMode,              XmSINGLE_LINE_EDIT,
                                XmNeditable,              False,
                                XmNtraversalOn,           False,
                                XmNvalue,                 "",
                                NULL);

    SetAttachments(hp);
    CalcSetMargins(hp);

    return 0;
}

void MotifKnobs_AddToHistplot (MotifKnobs_histplot_t *hp, DataKnob k)
{
  int            row;

    /* Check if this knob is already in a list */
    for (row = 0;
         row < hp->rows_used  &&  hp->target[row] != k;
         row++);
  
    if (row >= hp->rows_used)
    {
    
        if (hp->rows_used >= MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX)
        {
            for (row = 0;  row < hp->rows_used - 1;  row++)
            {
                RenewPlotRow (hp, row, hp->target[row + 1], hp->show[row + 1]);
                UpdatePlotRow(hp, row);
            }
            hp->rows_used--;
        }

        row = hp->rows_used;

        RenewPlotRow (hp, row, k, 1);
        UpdatePlotRow(hp, row);
        SetPlotCount (hp, row + 1);

        CalcSetMargins(hp);
        DrawGraph     (hp, True);
        DrawAxis      (hp, True);
    }
}

void MotifKnobs_UpdateHistplotGraph(MotifKnobs_histplot_t *hp)
{
    DisplayPlot(hp);
}

void MotifKnobs_UpdateHistplotProps(MotifKnobs_histplot_t *hp)
{
    CalcSetMargins(hp);
    DisplayPlot   (hp);
    DrawAxis      (hp, True);
}

int  MotifKnobs_SaveHistplotData   (MotifKnobs_histplot_t *hp, 
                                    const char *filename,
                                    const char *comment)
{
  FILE           *fp;
  struct timeval  timenow;
  struct timeval  timestart;
  struct timeval  timeoft;
  struct timeval  tv_x, tv_y, tv_r;

  int             row;
  int             age;
  DataKnob        k;
  double          v;
  double          mindisp;
  double          maxdisp;
  char            minbuf[400]; // So that 1e308 fits
  char            maxbuf[400]; // So that 1e308 fits
  const char     *cp;

  int             max_used;

    if ((fp = fopen(filename, "w")) == NULL) return -1;

    /* Calculate statistics and time values */
    for (row = 0, max_used = 0;  row < hp->rows_used;  row++)
        if (max_used < hp->target[row]->u.k.histring_used)
            max_used = hp->target[row]->u.k.histring_used;

    gettimeofday(&timenow, NULL);

    /* Title line */
    fprintf(fp, "#Time(s-01.01.1970) YYYY-MM-DD-HH:MM:SS.mss secs-since0");
    for (row = 0;  row < hp->rows_used;  row++)
    {
        k = hp->target[row];
        /* %DPYFMT: */
        fprintf(fp, " %s:", k->u.k.dpyfmt);
        /* Disprange (optional, if specified anyhow) */
        if (GetDispRange(k, &mindisp, &maxdisp))
        {
            fprintf(fp, "disprange=%s-%s:",
                    snprintf_dbl_trim(minbuf, sizeof(minbuf), k->u.k.dpyfmt, mindisp, 1),
                    snprintf_dbl_trim(maxbuf, sizeof(maxbuf), k->u.k.dpyfmt, maxdisp, 1));
        }
        /* Units */
        if (k->u.k.units != NULL  &&  k->u.k.units[0] != '\0')
        {
            fprintf(fp, "units=");
            for (cp = k->u.k.units;  *cp != '\0';  cp++)
                fprintf(fp, "%c", 1? *cp : '_');
            fprintf(fp, ":");
        }
        /* Value */
        fprintf(fp, "%s",
                k->ident != NULL  &&  k->ident[0] != '\0'? k->ident : "UNNAMED");
    }
    fprintf(fp, "\n");

    /* Data */
    for (age = max_used - 1;  age >= 0;  age--)
    {
        /* Calculate timeval of t */
        if (hp->timestamps_ring_used > age)
        {
            timeoft = hp->timestamps_ring[hp->timestamps_ring_used - 1 - age];
        }
        else
        {
            tv_x = timenow;
            tv_y.tv_sec  =      ((double)age) * hp->cyclesize_us / 1000000;
            tv_y.tv_usec = fmod(((double)age) * hp->cyclesize_us , 1000000);
            timeval_subtract(&timeoft, &tv_x, &tv_y);
        }

        /* Store it as start, if needed */
        if (age == max_used - 1) timestart = timeoft;

        /* Calculate time since start */
        tv_x = timeoft;
        tv_y = timestart;
        timeval_subtract(&tv_r, &tv_x, &tv_y);

        fprintf(fp, "%lu.%03d %s.%03d %7lu.%03d",
                (long)    timeoft.tv_sec,       (int)(timeoft.tv_usec / 1000),
                stroftime(timeoft.tv_sec, "-"), (int)(timeoft.tv_usec / 1000),
                (long)    tv_r.tv_sec,          (int)(tv_r.tv_usec    / 1000));

        for (row = 0;  row < hp->rows_used;  row++)
        {
            k = hp->target[row];
            if (k->u.k.histring_used > age)
                v = k->u.k.histring[
                                    (
                                     k->u.k.histring_start
                                     + 
                                     (k->u.k.histring_used - 1 - age)
                                    )
                                    %
                                    k->u.k.histring_len
                                   ];
            else
                v = NAN;

            fprintf(fp, " ");
            fprintf(fp, k->u.k.dpyfmt, v);
        }

        fprintf(fp, "\n");
    }

    fclose(fp);

    return 0;
}
