#include <stdio.h>
#include <limits.h>
#include <math.h>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
//#include <Xm/Label.h>
//#include <Xm/PushB.h>
//#include <Xm/Separator.h>
//#include <Xm/Text.h>

#include "misc_macros.h"
#include "misclib.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "Xh_fallbacks.h"
#include "Knobs.h"
#include "MotifKnobsP.h" // For ABSTRZE()/CNCRTZE()
#include "datatreeP.h"   // For access to DATATREE_COLZ_*

#include "wirebpm_gui.h"


//////////////////////////////////////////////////////////////////////

static int  magn_factors[] = {1, 2, 4, 10, 20, 50, 100, 200, 1000, 5000};

enum
{
    NUM_COLS = 15,
    NUM_ROWS = 15,
    PIXELS_PER_COL = 10,
    PIXELS_PER_ROW = 9,
    COL_OF_0 = 7,
    ROW_OF_0 = 7,

    GRF_WIDTH  = PIXELS_PER_COL * NUM_COLS,
    GRF_HEIGHT = PIXELS_PER_ROW * NUM_ROWS,
    GRF_SPACING = 10,

    GRF_FULL_WIDTH  = GRF_WIDTH + GRF_SPACING + GRF_WIDTH,
    GRF_FULL_HEIGHT = GRF_HEIGHT
};

//////////////////////////////////////////////////////////////////////
static XFontStruct     *last_finfo;

static GC AllocOneGC(CxWidget w, CxPixel fg, CxPixel bg, const char *fontspec)
{
  XtGCMask   mask;
  XGCValues  vals;
    
    mask            = GCFunction | GCGraphicsExposures | 
                      GCForeground | GCBackground | GCLineWidth;
    vals.function   = GXcopy;
    vals.graphics_exposures = False;
    vals.foreground = fg;
    vals.background = bg;
    vals.line_width = 0;

    if (fontspec != NULL  &&  fontspec[0] != '\0')
    {
        last_finfo = XLoadQueryFont(XtDisplay(CNCRTZE(w)), fontspec);
        if (last_finfo == NULL)
        {
            fprintf(stderr, "%s Unable to load font \"%s\"; using \"fixed\"\n", 
                    strcurtime(), fontspec);
            last_finfo = XLoadQueryFont(XtDisplay(CNCRTZE(w)), "fixed");
        }
        vals.font = last_finfo->fid;

        mask |= GCFont;
    }

    return XtGetGC(w, mask, &vals);
}

static void DrawMultilineString(Display *dpy, Drawable d,
                                GC gc, XFontStruct *finfo,
                                int x, int y,
                                const char *string)
{
  const char *p;
  const char *endp;
  size_t      len;
    
    for (p = string;  *p != '\0';)
    {
        endp = strchr(p, '\n');
        if (endp != NULL) len = endp - p;
        else              len = strlen(p);
        
        XDrawImageString(dpy, d, gc, x, y + finfo->ascent, p, len);
        
        y += finfo->ascent + finfo->descent;
        p += len;
        if (endp != NULL) p++;
    }
}

static void DrawHist(wirebpm_gui_t *gui, int data_cn,
                     int num_bars, int offset,
                     Display *dpy, Drawable d,
                     int x0, int pix_per_col, int magn,
                     GC posGC, GC negGC, GC outlGC, GC prevGC, int is_prev)
{
  pzframe_data_t      *pfr = &(gui->b.pfr);
  pzframe_type_dscr_t *ftd = pfr->ftd;

  int         i;
  GC          barGC;

  int         val;
  int         hgt;
  int         x1, y1, x2, y2;
  int         y0 = ROW_OF_0 * PIXELS_PER_ROW + (PIXELS_PER_ROW / 2);
  
    for (i = 0;  i < num_bars;  i++)
    {
        val = ((int32*)(pfr->cur_data[data_cn].current_val))[offset + i];

        hgt = (val * magn) * (ROW_OF_0 * PIXELS_PER_ROW) / (1 << 13);
        
        x1 = x0 + i * pix_per_col;
        x2 = x1 + pix_per_col - 1;

        if (is_prev)
            barGC = prevGC;
        else
            barGC = hgt >= 0? posGC : negGC;

        if (hgt >= 0)
        {
            y1 = y0 - hgt;
            y2 = y0;
        }
        else
        {
            y1 = y0;
            y2 = y0 - hgt;
        }

        XFillRectangle(dpy, d, barGC,
                       x1, y1,
                       x2 - x1 + 1, y2 - y1 + 1);
        
        XDrawRectangle(dpy, d, outlGC,
                       x1, y1,
                       x2 - x1 + 0, y2 - y1 + 0);  /*!!! Damn!!!  For some strange reason width/height=1 means 2(!!!) pixels!!! */
    }
}

//////////////////////////////////////////////////////////////////////

static void DoDraw(wirebpm_gui_t *gui, int do_refresh)
{
  Display *dpy = XtDisplay(gui->hgrm);
  Window   win = XtWindow (gui->hgrm);

    if (win == 0  ||  !do_refresh) return;
    XFillRectangle(dpy, win, gui->bkgdGC, 0, 0, gui->wid, gui->hei);
    gui->wiregui_vmt_p->do_draw(gui);
}

static void ExposureCB(Widget     w,
                       XtPointer  closure,
                       XtPointer  call_data)
{
  wirebpm_gui_t *gui = (wirebpm_gui_t *)closure;

    XhRemoveExposeEvents(ABSTRZE(w));
    DoDraw(gui, 1);
}

static void MagnKCB(DataKnob k __attribute__((unused)), double v, void *privptr)
{
  wirebpm_gui_t       *gui = privptr;

    gui->look.magn = magn_factors[(int)(round(v))];
    DoDraw(gui, 1);
}

static Widget wirebpm_gui_mkstdctl (pzframe_gui_t *pzframe_gui,
                                    Widget parent,
                                    int kind, int a, int b)
{
  wirebpm_gui_t       *gui = pzframe2wirebpm_gui(pzframe_gui);
  wirebpm_type_dscr_t *wtd = gui->b.wtd;

  pzframe_gui_dpl_t *prmp;

  Widget     w;
  DataKnob   k;
  int        i;
  int        sel_n;
  char       spec[1000];

    if      (kind == WIREBPM_GUI_CTL_COMMONS)
    {
        PzframeGuiMkCommons(&(gui->g), parent);
        return gui->g.commons;
    }
    else if (kind == WIREBPM_GUI_CTL_MAGN)
    {
        strcpy(spec, "look=selector rw label='' options=align=right items='#T");
        for (i = 0, sel_n = 0;  i < countof(magn_factors);  i++)
        {
            sprintf(spec+strlen(spec), "x%d%s",
                    magn_factors[i],
                    i < countof(magn_factors)-1? "\t" : "");
            if (gui->look.magn == magn_factors[i]) sel_n = i;
        }
        strcat(spec, "' tip='Display magnification factor'");
        k = CreateSimpleKnob(spec, 0, ABSTRZE(parent), MagnKCB, gui);
        SetSimpleKnobValue(k, sel_n);
        return CNCRTZE(GetKnobWidget(k));
    }
    else
    {
        fprintf(stderr, "%s(): request for unknown kind=%d (a=%d, b=%d)\n",
                __FUNCTION__, kind, a, b);

        return NULL;
    }
}

static void PopulateCpanel(wirebpm_gui_t *gui,
                           wirebpm_gui_mkctls_t mkctls)
{
  Widget    cpanel;
  Widget    ctls;

    cpanel = gui->cpanel;
    if (cpanel != NULL  &&  mkctls != NULL)
    {
        ctls = mkctls(&(gui->g), gui->b.wtd, cpanel,
                      wirebpm_gui_mkstdctl, PzframeGuiMkparknob);

        /* Populate the "leds" widget */
        PzframeGuiMkLeds(&(gui->g));

        /* And do fold, if requested */
        if (gui->g.look.foldctls)
            /*!!!XhPlotSetCpanelState(gui->plot, 0)*/;
    }
}

//////////////////////////////////////////////////////////////////////

static int DoRealize(pzframe_gui_t *pzframe_gui,
                     Widget parent,
                     cda_context_t  present_cid,
                     const char    *base)
{
  wirebpm_gui_t *gui = pzframe2wirebpm_gui(pzframe_gui);

    return WirebpmGuiRealize(gui, parent,
                             present_cid, base);
}

static void UpdateBG(pzframe_gui_t *inherited_gui)
{
  wirebpm_gui_t *gui = (wirebpm_gui_t *)inherited_gui;

  CxPixel fg = XhGetColor(XH_COLOR_GRAPH_TITLES);
  CxPixel bg = XhGetColor(XH_COLOR_GRAPH_BG);
  GC  new_bkgdGC;

    MotifKnobs_ChooseColors(DATAKNOB_COLZ_NORMAL, gui->g.curstate, fg, bg, &fg, &bg);

    if (gui->hgrm != NULL) 
    {
        new_bkgdGC = AllocOneGC(gui->hgrm, bg, bg, NULL);
        if (new_bkgdGC != NULL)
        {
            XtReleaseGC(gui->hgrm, gui->bkgdGC);
            gui->bkgdGC = new_bkgdGC;
        }

        XtVaSetValues(gui->hgrm, XmNbackground, bg, NULL);
    }
}

static void DoRenew(pzframe_gui_t *inherited_gui,
                    int  info_changed)
{
  wirebpm_gui_t *gui = pzframe2wirebpm_gui(inherited_gui);

#if 0
  static int ctr = 0;

  int       n;

  int32    *src = (int32*)(gui->g.pfr->cur_data[gui->b.wtd->data_cn].current_val);

    for (n = 0;  n < MAX_CELLS_PER_UNIT;  n++)
        src[n] = (sin((n+ctr)*3.1415/5*2/NUM_COLS + n/30) * (1<<13) / 10);

    ctr = (ctr + 1) & 0xFF;
#endif

    DoDraw(gui, 1);
}

static pzframe_gui_vmt_t wirebpm_gui_std_pzframe_vmt =
{
    .realize  = DoRealize,
    .evproc   = NULL,
    .newstate = UpdateBG,
    .do_renew = DoRenew,
};
void  WirebpmGuiFillStdDscr(wirebpm_gui_dscr_t *gkd, wirebpm_type_dscr_t *wtd)
{
    bzero(gkd, sizeof(*gkd));
    
    gkd->wtd     = wtd;
    gkd->gkd.ftd = &(gkd->wtd->ftd);
    gkd->gkd.bhv = PZFRAME_GUI_BHV_NORESIZE;
    gkd->gkd.vmt = wirebpm_gui_std_pzframe_vmt;
    //gkd->gkd.update  = NULL;
    
}

//////////////////////////////////////////////////////////////////////

static void HIST_prep   (wirebpm_gui_t *gui, Widget parent)
{
  CxPixel  bar_col = XhGetColor(XH_COLOR_GRAPH_BAR1);

    gui->dat1GC = AllocOneGC(parent, bar_col, bar_col, NULL);
}

static void HIST_draw   (wirebpm_gui_t *gui)
{
  Display  *dpy = XtDisplay(gui->hgrm);
  Drawable  drw = XtWindow (gui->hgrm);

  int32    *src = (int32*)(gui->g.pfr->cur_data[gui->b.wtd->data_cn].current_val);

  int       dim;
  int       gen;
  int       xg, yg;
  int       x0, y0;

  int       sum;
  int       i;
  char      buf[100];

    for (dim = 0;  dim < 2;  dim++)
    {
        /* Calc geometry */
        xg = (GRF_WIDTH + GRF_SPACING) * dim;
        yg = 0;
        x0 = COL_OF_0 * PIXELS_PER_COL + (PIXELS_PER_COL / 2) + xg;
        y0 = ROW_OF_0 * PIXELS_PER_ROW + (PIXELS_PER_ROW / 2) + yg;

        /* Draw axis */
        XDrawLine(dpy, drw, gui->axisGC,
                  xg, y0, xg + GRF_WIDTH - 1, y0);
        XDrawLine(dpy, drw, gui->axisGC,
                  x0, yg, x0, yg + GRF_HEIGHT - 1);

        /* Draw histograms.
           Generations are processed from 1 to 0 (from oldest to newest) --
           for correct display order */
        for (gen = 0;  gen >= 0;  gen--)
        {
            DrawHist(gui, gui->b.wtd->data_cn,
                     NUM_COLS, NUM_COLS*dim,
                     dpy, drw,
                     xg, PIXELS_PER_COL, gui->look.magn,
                     gui->dat1GC, gui->dat1GC, gui->outlGC, gui->prevGC, gen);
        }

        /* Display stats */
        for (sum = 0, i = 0;  i < NUM_COLS;  i++)
            sum += src[NUM_COLS*dim + i];
        snprintf(buf, sizeof(buf), "% d", sum);
        XDrawString(dpy, drw, gui->textGC,
                    1 + xg, 1 + gui->text_finfo->ascent,
                    buf, strlen(buf));
    }
}

//--------------------------------------------------------------------

enum
{
    ESPECTR_NUM_CHANS = 30
};

// Electron Charge 
#define eQ      (1.62e-19) 
// Input Line Capacity 
#define CAmp    (5.7e-9) 

static inline double Energy_Mev(double ISpectr, int i)
{
    return (8.1e-4 * ISpectr * (3.7 + (i + ESPECTR_NUM_CHANS) * 65. / 59.));
}

        
static void MouseHandler(Widget     w                    __attribute__((unused)),
                         XtPointer  closure,
                         XEvent    *event,
                         Boolean   *continue_to_dispatch __attribute__((unused)))
{
  wirebpm_gui_t *gui = (wirebpm_gui_t *) closure;
  XMotionEvent  *mev = (XMotionEvent *)  event;
  XButtonEvent  *bev = (XButtonEvent *)  event;
  
  int            x;
  int            b;
  int            touch_min = 0;
  int            touch_max = 0;

    if (event->type == MotionNotify)
    {
        x = mev->x;
        if (mev->state & Button1Mask) touch_min = 1;
        if (mev->state & Button3Mask) touch_max = 1;
    }
    else
    {
        x = bev->x;
        if (bev->button == Button1) touch_min = 1;
        if (bev->button == Button3) touch_max = 1;
    }
  
    b = x / PIXELS_PER_COL;
    if (b < 0)                   b = 0;
    if (b > ESPECTR_NUM_CHANS-1) b = ESPECTR_NUM_CHANS-1;
    
    if      (touch_min  &&  touch_max)
        gui->b_gmin = gui->b_gmax = b;
    else if (touch_min)
    {
        if (b > gui->b_gmax) b = gui->b_gmax;
        gui->b_gmin = b;
    }
    else if (touch_max)
    {
        if (b < gui->b_gmin) b = gui->b_gmin;
        gui->b_gmax = b;
    }
    else return;

    DoDraw(gui, 1);
}

static void ESPECTR_prep(wirebpm_gui_t *gui, Widget parent)
{
  CxPixel  pos_col = XhGetColor(XH_COLOR_JUST_RED);
  CxPixel  neg_col = XhGetColor(XH_COLOR_JUST_BLUE);
  CxPixel  rpr_col = XhGetColor(XH_COLOR_GRAPH_REPERS);

    gui->dat1GC = AllocOneGC(parent, pos_col, pos_col, NULL);
    gui->dat2GC = AllocOneGC(parent, neg_col, neg_col, NULL);
    gui->reprGC = AllocOneGC(parent, rpr_col, rpr_col, NULL);

    gui->b_gmin = 0;  gui->b_gmax = ESPECTR_NUM_CHANS - 1;
}

static void ESPECTR_tune(wirebpm_gui_t *gui, Widget parent)
{
    XtAddEventHandler(gui->hgrm,
                      ButtonPressMask   | ButtonReleaseMask | PointerMotionMask |
                      Button1MotionMask | Button2MotionMask | Button3MotionMask,
                      False, MouseHandler, (XtPointer)gui);
}

static void ESPECTR_draw(wirebpm_gui_t *gui)
{
  Display  *dpy = XtDisplay(gui->hgrm);
  Drawable  drw = XtWindow (gui->hgrm);

  int32    *src = (int32*)(gui->g.pfr->cur_data[gui->b.wtd->data_cn].current_val);

  int       x;

  int       i;
  double    curI;
  cda_dataref_t ref;
  double    count;
  double    mevs;
  double    num_sum;
  double    gate_num_sum;

  char      buf[200];

    DrawHist(gui, gui->b.wtd->data_cn,
             ESPECTR_NUM_CHANS, 0,
             dpy, drw,
             0, PIXELS_PER_COL, gui->look.magn,
             gui->dat1GC, gui->dat2GC, gui->outlGC, gui->prevGC, 0);

    x = gui->b_gmin * PIXELS_PER_COL;
    XDrawLine(dpy, drw, gui->reprGC, x, 0, x, GRF_FULL_HEIGHT-1);
    x = gui->b_gmax * PIXELS_PER_COL + (PIXELS_PER_COL-1);
    XDrawLine(dpy, drw, gui->reprGC, x, 0, x, GRF_FULL_HEIGHT-1);

    if (cda_ref_is_sensible(gui->b.ext_i_ref))
    {
        curI = gui->b.mes.ext_i;
        ref  = gui->g.pfr->refs    [gui->b.wtd->data_cn];
        for (i = 0, num_sum = 0.0, gate_num_sum = 0.0;
             i < ESPECTR_NUM_CHANS;
             i++)
        {
            cda_rd_convert(ref, src[i], &count);
            mevs = count * Energy_Mev(curI, i);

            num_sum += count;

            if (i >= gui->b_gmin  &&  i <= gui->b_gmax)
                gate_num_sum += count;
        }

        sprintf(buf, "N=% .2e\n[%.2f,%.2f]N=%.2e",
                num_sum,
                Energy_Mev(curI, gui->b_gmin), Energy_Mev(curI, gui->b_gmax),
                gate_num_sum);
        DrawMultilineString(dpy, drw, gui->textGC, gui->text_finfo,
                            0, 0, buf);
    }
}

static wirebpm_gui_vmt_t HIST_vmt    = {HIST_prep,    NULL,         HIST_draw};
static wirebpm_gui_vmt_t ESPECTR_vmt = {ESPECTR_prep, ESPECTR_tune, ESPECTR_draw};


//////////////////////////////////////////////////////////////////////

void  WirebpmGuiInit       (wirebpm_gui_t *gui, wirebpm_gui_dscr_t *gkd)
{
    bzero(gui, sizeof(*gui));
    WirebpmDataInit(&(gui->b), gkd->wtd);
    gui->d = gkd;
    PzframeGuiInit(&(gui->g), &(gui->b.pfr), &(gkd->gkd));
}

int   WirebpmGuiRealize    (wirebpm_gui_t *gui, Widget parent,
                            cda_context_t  present_cid,
                            const char    *base)
{
  int         r;
  int         cpanel_loc;
  CxPixel     bg = XhGetColor(XH_COLOR_GRAPH_BG);
  
    if ((r = WirebpmDataRealize(&(gui->b),
                                present_cid, base)) < 0) return r;
    PzframeGuiRealize(&(gui->g));

    if (gui->look.kind == WIREBPM_KIND_ESPECTR)
        gui->wiregui_vmt_p = &ESPECTR_vmt;
    else /* default */
        gui->wiregui_vmt_p = &HIST_vmt;

    cpanel_loc = gui->d->cpanel_loc;
    if (gui->g.look.nocontrols) cpanel_loc = 0/*!!!!!!!!!!!!!!!!*/;

    gui->container = XtVaCreateManagedWidget("container", xmFormWidgetClass,
                                             parent,
                                             XmNshadowThickness, 0,
                                             NULL);
    gui->cpanel    = XtVaCreateManagedWidget("cpanel",    xmFormWidgetClass,
                                             gui->container,
                                             XmNshadowThickness, 0,
                                             NULL);
    attachleft (gui->cpanel, NULL, 0);
    attachright(gui->cpanel, NULL, 0);

    gui->g.top = gui->container;

    gui->wid = GRF_FULL_WIDTH;
    gui->hei = GRF_FULL_HEIGHT;

    if (gui->wiregui_vmt_p->do_prep != NULL)
        gui->wiregui_vmt_p->do_prep(gui, parent);

    gui->bkgdGC = AllocOneGC(parent, bg,                                 bg, NULL);
    gui->axisGC = AllocOneGC(parent, XhGetColor(XH_COLOR_GRAPH_AXIS),    bg, NULL);
    gui->reprGC = AllocOneGC(parent, XhGetColor(XH_COLOR_GRAPH_REPERS),  bg, NULL);
    gui->prevGC = AllocOneGC(parent, XhGetColor(XH_COLOR_GRAPH_PREV),    bg, NULL);
    gui->outlGC = AllocOneGC(parent, XhGetColor(XH_COLOR_GRAPH_OUTL),    bg, NULL);

    gui->textGC = AllocOneGC(parent, XhGetColor(XH_COLOR_GRAPH_TITLES),
                             XhGetColor(XH_COLOR_GRAPH_BG), XH_FIXED_BOLD_FONT);
    gui->text_finfo = last_finfo;

    gui->hgrm = XtVaCreateManagedWidget("hgrm", xmDrawingAreaWidgetClass,
                                        gui->container,
                                        XmNwidth,      gui->wid,
                                        XmNheight,     gui->hei,
                                        XmNbackground, bg,
                                        NULL);
    XtAddCallback(gui->hgrm, XmNexposeCallback, ExposureCB, gui);
    attachtop(gui->hgrm, gui->cpanel, 0);

    if (gui->wiregui_vmt_p->do_tune != NULL)
        gui->wiregui_vmt_p->do_tune(gui, parent);

    UpdateBG(&(gui->g));

    PopulateCpanel(gui, gui->d->mkctls);

    return 0;
}


void  WirebpmGuiRenewView  (wirebpm_gui_t *gui, int info_changed)
{

}

static psp_lkp_t kind_lkp[] =
{
    {"hist",   WIREBPM_KIND_HIST},
    {"spectr", WIREBPM_KIND_ESPECTR},
    {NULL, 0}
};
static psp_paramdescr_t  wirebpm_gui_text2look[] =
{
    PSP_P_LOOKUP("kind", wirebpm_gui_look_t, kind, WIREBPM_KIND_HIST, kind_lkp),
    PSP_P_INT   ("magn", wirebpm_gui_look_t, magn, 1, 1, 10000),

    PSP_P_END()
};

psp_paramdescr_t *WirebpmGuiCreateText2LookTable(wirebpm_gui_dscr_t *gkd)
{
  psp_paramdescr_t *ret = NULL;
  size_t            ret_size;

  int               x;

    /* Allocate a table */
    ret_size = countof(wirebpm_gui_text2look) * sizeof(*ret);
    ret = malloc(ret_size);
    if (ret == NULL)
    {
        fprintf(stderr, "%s %s(type=%s): unable to allocate %zu bytes for PSP-table\n",
                strcurtime(), __FUNCTION__, gkd->gkd.ftd->type_name, ret_size);
        return ret;
    }

    /* And fill it */
    for (x = 0;  x < countof(wirebpm_gui_text2look);  x++)
    {
        ret[x] = wirebpm_gui_text2look[x];
        if      (ret[x].name   == NULL);
    }

    return ret;
}
