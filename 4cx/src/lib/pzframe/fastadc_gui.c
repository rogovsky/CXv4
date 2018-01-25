#include <stdio.h>
#include <limits.h>
#include <math.h>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/Separator.h>
#include <Xm/Text.h>

#include "misc_macros.h"
#include "misclib.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "Knobs.h"
#include "MotifKnobsP.h" // For ABSTRZE()/CNCRTZE()
#include "datatreeP.h"   // For access to k->currflags
//!!!#include "Chl.h"
//!!!#include "ChlI.h"

#include "fastadc_gui.h"


//////////////////////////////////////////////////////////////////////

static void UpdateView(fastadc_gui_t *gui, int with_axis)
{
    XhPlotUpdate(gui->plot, with_axis);
}

static void UpdateRepers(fastadc_gui_t *gui)
{
  fastadc_type_dscr_t *atd = gui->a.atd;

  int     nr;
  int     nl;
  int     is_r;
  int     x;
  char    buf[100];
  int     rpr_xs [XH_PLOT_NUM_REPERS];
  int     rpr_avl[XH_PLOT_NUM_REPERS][FASTADC_MAX_LINES];
  double  rpr_dsp[XH_PLOT_NUM_REPERS][FASTADC_MAX_LINES];

    for (nr = 0;  nr < XH_PLOT_NUM_REPERS;  nr++)
    {
        is_r = nr < FASTADC_GUI_2_REPERS;
        x = gui->rpr_at[nr];

        rpr_xs[nr] = is_r? FastadcDataX2XS(&(gui->a.mes), x)
                         : rpr_xs[1] - rpr_xs[0];
        snprintf(buf, sizeof(buf), "%c:%d%s",
                 is_r? '1' + nr : 'd',
                 rpr_xs[nr],
                 gui->a.mes.exttim? "" : atd->xs);
        if (gui->rpr_time[nr] != NULL) XmTextSetString(gui->rpr_time[nr], buf);
        
        for (nl = 0;  nl < atd->num_lines;  nl++)
        {
            if (is_r)
                rpr_avl[nr][nl] =
                    gui->rpr_on[nr]             &&
                    gui->a.dcnv.lines[nl].show  &&
                    gui->a.mes.plots [nl].on    &&
                    gui->rpr_at[nr] < gui->a.mes.plots[nl].numpts;
            else
                rpr_avl[nr][nl] =
                    rpr_avl[0][nl]  &&
                    rpr_avl[1][nl]/*  &&
                    gui->rpr_on[nr]*/;

            if (rpr_avl[nr][nl])
            {
                if (is_r)
                    rpr_dsp[nr][nl] = FastadcDataGetDsp(&(gui->a),
                                                        &(gui->a.mes), nl, x);
                else
                    rpr_dsp[nr][nl] = rpr_dsp[1][nl] - rpr_dsp[0][nl];
                snprintf(buf, sizeof(buf),
                         gui->a.dcnv.lines[nl].dpyfmt, rpr_dsp[nr][nl]);
            }
            else
            {
                rpr_dsp[nr][nl] = 0;
                buf[0] = '\0';
            }

            if (gui->rpr_val[nr][nl] != NULL) XmTextSetString(gui->rpr_val[nr][nl], buf);
        }
    }
}

static void ChnSwchKCB(DataKnob k __attribute__((unused)), double v, void *privptr)
{
  pzframe_gui_dpl_t *pp    = privptr;
  fastadc_gui_t     *gui   = pp->p;
  int                nl    = pp->n;
  int32              value = round(v);

    gui->a.dcnv.lines[nl].show = value != 0;
    XhPlotOneSetShow(gui->plot, nl, gui->a.dcnv.lines[nl].show);
    UpdateView  (gui, 1);
    UpdateRepers(gui);
}

static void MagnKCB(DataKnob k __attribute__((unused)), double v, void *privptr)
{
  pzframe_gui_dpl_t *pp    = privptr;
  fastadc_gui_t     *gui   = pp->p;
  int32              value = round(v);

    gui->a.dcnv.lines[pp->n].magn = fastadc_data_magn_factors[value];
    XhPlotOneSetMagn(gui->plot, pp->n, gui->a.dcnv.lines[pp->n].magn);
    UpdateView(gui, 1);
}

static void InvpKCB(DataKnob k __attribute__((unused)), double v, void *privptr)
{
  pzframe_gui_dpl_t *pp    = privptr;
  fastadc_gui_t     *gui   = pp->p;
  int32              value = round(v);

    gui->a.dcnv.lines[pp->n].invp = (value != 0);
    XhPlotOneSetInvp(gui->plot, pp->n, gui->a.dcnv.lines[pp->n].invp);
    UpdateView(gui, 1);
}

static void CmprKCB(DataKnob k __attribute__((unused)), double v, void *privptr)
{
  fastadc_gui_t *gui       = (fastadc_gui_t *)privptr;
  int32          value     = round(v);

    gui->a.dcnv.cmpr = fastadc_data_cmpr_factors[value];
    XhPlotSetCmpr(gui->plot, gui->a.dcnv.cmpr);
}

static void RprSwchKCB(DataKnob k __attribute__((unused)), double v, void *privptr)
{
  pzframe_gui_dpl_t *pp    = privptr;
  fastadc_gui_t     *gui   = pp->p;
  int32              value = round(v);
  int                nr    = pp->n;

    gui->rpr_on[nr] = value != 0;
    if (nr < FASTADC_GUI_2_REPERS)
        XhPlotSetReper(gui->plot, nr, gui->rpr_on[nr]? gui->rpr_at[nr] : -1);
    UpdateView(gui, 0);
    UpdateRepers(gui);
    PzframeGuiCallCBs(&(gui->g), FASTADC_REASON_REPER_CHANGE, 0);
}

static Widget fastadc_gui_mkstdctl (pzframe_gui_t *pzframe_gui,
                                    Widget parent,
                                    int kind, int a, int b)
{
  fastadc_gui_t       *gui = pzframe2fastadc_gui(pzframe_gui);
  fastadc_type_dscr_t *atd = gui->a.atd;

  pzframe_gui_dpl_t *prmp;

  Widget     w;
  DataKnob   k;
  int        i;
  char       spec[1000];
  int        pn;
  int        nl;
  int        nr;
  int        sel_n;
  XmString   s;
  Pixel      lnc;
  Pixel      bgc;
  
    pn = a;
    nl = a;
    nr = b;

    if      (kind == FASTADC_GUI_CTL_COMMONS)
    {
        PzframeGuiMkCommons(&(gui->g), parent);
        return gui->g.commons;
    }
    else if (kind == FASTADC_GUI_CTL_X_CMPR)
    {
        snprintf(spec, sizeof(spec),
                 " look=selector rw label='' options=align=right items='#T");
        for (i = 0, sel_n = 0;  i < countof(fastadc_data_cmpr_factors);  i++)
        {
            if (fastadc_data_cmpr_factors[i] > 0)
                sprintf(spec + strlen(spec), "%s1:%d",
                        i == 0? "":"\t",
                        +fastadc_data_cmpr_factors[i]);
            else
                sprintf(spec + strlen(spec), "%s%d:1",
                        i == 0? "":"\t",
                        -fastadc_data_cmpr_factors[i]);
            if (gui->a.dcnv.cmpr == fastadc_data_cmpr_factors[i]) sel_n = i;
        }
        strcat(spec, "'");
        k = CreateSimpleKnob(spec, 0, ABSTRZE(parent), CmprKCB, gui);
        SetSimpleKnobValue(k, sel_n);
        return CNCRTZE(GetKnobWidget(k));
    }
    else if (kind == FASTADC_GUI_CTL_LINE_LABEL)
    {
        lnc = XhGetColor((gui->look.black? XH_COLOR_BGRAPH_LINE1 : XH_COLOR_GRAPH_LINE1) + (nl % XH_NUM_DISTINCT_LINE_COLORS));
        bgc = XhGetColor((gui->look.black? XH_COLOR_BGRAPH_BG    : XH_COLOR_GRAPH_BG));
        snprintf(spec, sizeof(spec),
                 " look=onoff rw label='%s'",
                 gui->a.dcnv.lines[nl].descr);
        prmp = gui->Line_prm + nl;
        prmp->p = gui;
        prmp->n = nl;
        k = CreateSimpleKnob(spec, 0, ABSTRZE(parent), ChnSwchKCB, prmp);
        gui->label_ks[nl] = k;
        SetSimpleKnobValue(k, gui->a.dcnv.lines[nl].show);
        SetSimpleKnobState(k, KNOBSTATE_NONE);
        w = CNCRTZE(GetKnobWidget(k));
        if (b == 0)
        {
            XtVaSetValues(w,
                          XmNforeground,  lnc,
                          XmNselectColor, lnc,
                          NULL);
        }
        else
        {
            XtVaSetValues(w,
                          XmNbackground,  bgc,
                          XmNforeground,  lnc,
                          XmNselectColor, lnc,
                          NULL);
        }
        return w;
    }
    else if (kind == FASTADC_GUI_CTL_LINE_MAGN)
    {
        snprintf(spec, sizeof(spec),
                 " look=selector rw label='' options=align=right items='#T");
        for (i = 0, sel_n = 0;  i < countof(fastadc_data_magn_factors);  i++)
        {
            sprintf(spec + strlen(spec), "%sx%d",
                    i == 0? "":"\t", fastadc_data_magn_factors[i]);
            if (gui->a.dcnv.lines[nl].magn == fastadc_data_magn_factors[i]) sel_n = i;
        }
        strcat(spec, "'");
        prmp = gui->Line_prm + nl;
        prmp->p = gui;
        prmp->n = nl;
        k = CreateSimpleKnob(spec, 0, ABSTRZE(parent), MagnKCB, prmp);
        SetSimpleKnobValue(k, sel_n);
        return CNCRTZE(GetKnobWidget(k));
    }
    else if (kind == FASTADC_GUI_CTL_LINE_INVP)
    {
        snprintf(spec, sizeof(spec),
                 " look=onoff rw label='^v' options='panel'");
        prmp = gui->Line_prm + nl;
        prmp->p = gui;
        prmp->n = nl;
        k = CreateSimpleKnob(spec, 0, ABSTRZE(parent), InvpKCB, prmp);
        SetSimpleKnobValue(k, gui->a.dcnv.lines[nl].invp);
        return CNCRTZE(GetKnobWidget(k));
    }
    else if (kind == FASTADC_GUI_CTL_REPER_VALUE)
    {
        check_snprintf(spec, sizeof(spec), gui->a.dcnv.lines[nl].dpyfmt,
                       FastadcDataPvl2Dsp(&(gui->a), nl, 
                                          (reprof_cxdtype(atd->ftd.chan_dscrs[atd->line_dscrs[nl].data_cn].dtype) == CXDTYPE_REPR_INT)
                                          ? atd->line_dscrs[nl].range.int_r[1]
                                          : atd->line_dscrs[nl].range.dbl_r[1]));
        return gui->rpr_val[nr][nl] =
            XtVaCreateManagedWidget("text_o", xmTextWidgetClass, parent,
                                    XmNcolumns,               strlen(spec),
                                    XmNscrollHorizontal,      False,
                                    XmNcursorPositionVisible, False,
                                    XmNeditable,              False,
                                    XmNtraversalOn,           False,
                                    XmNvalue, "",
                                    NULL);
    }
    else if (kind == FASTADC_GUI_CTL_REPER_TIME)
    {
        sprintf(spec, "1:%d%s",
                FastadcDataX2XS(&(gui->a.mes), atd->max_numpts - 1) + 1, atd->xs);
        return gui->rpr_time[nr] =
            XtVaCreateManagedWidget("text_o", xmTextWidgetClass, parent,
                                    XmNcolumns,               strlen(spec) + 1,
                                    XmNscrollHorizontal,      False,
                                    XmNcursorPositionVisible, False,
                                    XmNeditable,              False,
                                    XmNtraversalOn,           False,
                                    NULL);
    }
    else if (kind == FASTADC_GUI_CTL_REPER_ONOFF)
    {
        if (nr < FASTADC_GUI_2_REPERS)
        {
            snprintf(spec, sizeof(spec),
                     " look=onoff rw label=''"
                     " options='color=green'");
            prmp = gui->RprSwch_prm + nr;
            prmp->p = gui;
            prmp->n = nr;
            gui->rpr_kswch[nr] = CreateSimpleKnob(spec, 0, ABSTRZE(parent), RprSwchKCB, prmp);
            w = CNCRTZE(GetKnobWidget(gui->rpr_kswch[nr]));
            SetSimpleKnobValue(gui->rpr_kswch[nr], gui->rpr_on[nr]);
        }
        else
        {
            w = XtVaCreateManagedWidget("", widgetClass, parent,
                                        XmNborderWidth, 0,
                                        XmNwidth,       1,
                                        XmNheight,      1,
                                        NULL);
        }
        XtVaSetValues(w, XmNtraversalOn, False, NULL);
        return w;
    }
    else if (kind == FASTADC_GUI_CTL_REPER_UNITS)
    {
        w = XtVaCreateManagedWidget("rowlabel", xmLabelWidgetClass, parent,
                                    XmNlabelString, s = XmStringCreateLtoR(gui->a.dcnv.lines[nl].units, xh_charset),
                                    NULL);
        XmStringFree(s);
        return w;
    }
    else
    {
        fprintf(stderr, "%s(): request for unknown kind=%d (a=%d, b=%d)\n",
                __FUNCTION__, kind, a, b);

        return NULL;
    }
}

static void PopulateCpanel(fastadc_gui_t *gui,
                           fastadc_gui_mkctls_t mkctls)
{
  Widget    cpanel;
  Widget    ctls;

    cpanel = XhCpanelOfPlot(gui->plot);
    if (cpanel != NULL  &&  mkctls != NULL)
    {
        ctls = mkctls(&(gui->g), gui->a.atd, cpanel,
                      fastadc_gui_mkstdctl, PzframeGuiMkparknob);

        /* Populate the "leds" widget */
        PzframeGuiMkLeds(&(gui->g));

        /* And do fold, if requested */
        if (gui->g.look.foldctls)
            XhPlotSetCpanelState(gui->plot, 0);
    }
}

//////////////////////////////////////////////////////////////////////

static int DoRealize(pzframe_gui_t *pzframe_gui,
                     Widget parent,
                     cda_context_t  present_cid,
                     const char    *base)
{
  fastadc_gui_t *gui = pzframe2fastadc_gui(pzframe_gui);

    return FastadcGuiRealize(gui, parent,
                             present_cid, base);
}

static void UpdateBG(pzframe_gui_t *inherited_gui)
{
  fastadc_gui_t *gui = pzframe2fastadc_gui(inherited_gui);

  CxPixel fg = XhGetColor(XH_COLOR_GRAPH_TITLES);
  CxPixel bg = XhGetColor((gui->look.black? XH_COLOR_BGRAPH_BG : XH_COLOR_GRAPH_BG));

    MotifKnobs_ChooseColors(DATAKNOB_COLZ_NORMAL, gui->g.curstate, fg, bg, &fg, &bg);
    if (gui->plot != NULL) XhPlotSetBG(gui->plot, bg);
}

static void DoRenew(pzframe_gui_t *inherited_gui,
                    int  info_changed)
{
  fastadc_gui_t *gui = (fastadc_gui_t *)inherited_gui;

    FastadcGuiRenewPlot(gui, gui->a.mes.cur_numpts, info_changed);
}

static pzframe_gui_vmt_t fastadc_gui_std_pzframe_vmt =
{
    .realize  = DoRealize,
    .evproc   = NULL,
    .newstate = UpdateBG,
    .do_renew = DoRenew,
};
void  FastadcGuiFillStdDscr(fastadc_gui_dscr_t *gkd, fastadc_type_dscr_t *atd)
{
    bzero(gkd, sizeof(*gkd));
    
    gkd->atd     = atd;
    gkd->gkd.ftd = &(gkd->atd->ftd);
    gkd->gkd.vmt = fastadc_gui_std_pzframe_vmt;
}

//////////////////////////////////////////////////////////////////////

static double      gui_pvl2dsp(void *privptr, int nl, double pvl)
{
  fastadc_gui_t *gui = privptr;

    return FastadcDataPvl2Dsp(&(gui->a), nl, pvl);
}

static int         gui_x2xs   (void *privptr, int x)
{
  fastadc_gui_t *gui = privptr;

    return FastadcDataX2XS(&(gui->a.mes), x);
//    return gui->a.atd->x2xs(gui->a.pfr.cur_data, x);
}

static const char *gui_xs     (void *privptr)
{
  fastadc_gui_t *gui = privptr;

    return gui->a.mes.exttim? "" : gui->a.atd->xs;
}

static void        gui_draw   (void *privptr, int nl, int age,
                               int magn, GC gc)
{
  fastadc_gui_t *gui = privptr;

    XhPlotOneDraw(gui->plot,
                  (/*!!!age == 0? */gui->a.mes.plots/*!!! : gui->a.svd.plots*/) + nl,
                  magn, gc);
}

static void        set_rpr_to(fastadc_gui_t *gui, int nr, int x)
{
    XhPlotSetReper(gui->plot, nr, x);
    if (x < 0) gui->rpr_on[nr] = 0;
    else
    {
        gui->rpr_on[nr] = 1;
        gui->rpr_at[nr] = x;
    }
    if (gui->rpr_kswch[nr] != NULL)
        SetSimpleKnobValue(gui->rpr_kswch[nr], gui->rpr_on[nr]);
    UpdateRepers(gui);
}

static void        gui_set_rpr(void *privptr, int nr, int x)
{
  fastadc_gui_t *gui = privptr;

    if      (nr == 0);
    else if (nr == 2) nr = 1;
    else return;

    set_rpr_to(gui, nr, x);
    PzframeGuiCallCBs(&(gui->g), FASTADC_REASON_REPER_CHANGE, 0);
}

static void LineStyleKCB(DataKnob k __attribute__((unused)), double v, void *privptr)
{
  fastadc_gui_t *gui = privptr;

    XhPlotSetLineMode(gui->plot, (int)(round(v)));
}

//////////////////////////////////////////////////////////////////////

void  FastadcGuiInit       (fastadc_gui_t *gui, fastadc_gui_dscr_t *gkd)
{
    bzero(gui, sizeof(*gui));
    FastadcDataInit(&(gui->a), gkd->atd);
    gui->d = gkd;
    PzframeGuiInit(&(gui->g), &(gui->a.pfr), &(gkd->gkd));
}

int   FastadcGuiRealize    (fastadc_gui_t *gui, Widget parent,
                            cda_context_t  present_cid,
                            const char    *base)
{
  int         r;
  int         options;
  Widget      angle;
  DataKnob    ak;
  int         nl;

    if ((r = FastadcDataRealize(&(gui->a),
                                present_cid, base)) < 0) return r;
    PzframeGuiRealize(&(gui->g));

    options = gui->d->cpanel_loc & XH_PLOT_CPANEL_MASK;
    if (gui->g.look.nocontrols) options  = XH_PLOT_CPANEL_ABSENT;
    if (gui->look.nofold)       options |= XH_PLOT_NO_CPANEL_FOLD_MASK;
    if (gui->look.noscrollbar)  options |= XH_PLOT_NO_SCROLLBAR_MASK;

    gui->plot       = XhCreatePlot(parent, 
                                   XH_PLOT_Y_OF_X, 
                                   gui->a.atd->num_lines,
                                   gui, 
                                   NULL, gui_pvl2dsp, gui_x2xs, gui_xs, 
                                   gui->d->plotdraw != NULL ? gui->d->plotdraw
                                                            : gui_draw,
                                   gui_set_rpr,
                                   gui->look.width, gui->look.height,
                                   options, gui->look.black,
                                   0);
    gui->g.top = XhWidgetOfPlot(gui->plot);

    angle = XhAngleOfPlot(gui->plot);
    XtManageChild(angle);
    ak = CreateSimpleKnob(" rw look=choicebs label='' items='#T-\b\bsize=small\t...\b\bsize=small'",
                          0, ABSTRZE(angle), LineStyleKCB, gui);
    SetSimpleKnobValue(ak, 0);

    for (nl = 0;  nl < gui->a.atd->num_lines;  nl++)
    {
        XhPlotOneSetData(gui->plot, nl, gui->a.mes.plots + nl);
        XhPlotOneSetShow(gui->plot, nl, gui->a.dcnv.lines[nl].show);
        XhPlotOneSetMagn(gui->plot, nl, gui->a.dcnv.lines[nl].magn);
        XhPlotOneSetInvp(gui->plot, nl, gui->a.dcnv.lines[nl].invp);
        XhPlotOneSetStrs(gui->plot, nl, gui->a.dcnv.lines[nl].descr,
                         gui->a.dcnv.lines[nl].dpyfmt, gui->a.dcnv.lines[nl].units);
    }
    XhPlotSetCmpr(gui->plot, gui->a.dcnv.cmpr);
    XhPlotCalcMargins(gui->plot);
    UpdateBG(&(gui->g));

    PopulateCpanel(gui, gui->d->mkctls);
    UpdateRepers  (gui);

    return 0;
}

static void SetChnSwchState(fastadc_gui_t *gui, int nl)
{
  fastadc_type_dscr_t *atd = gui->a.atd;
  DataKnob             k;

    if ((k = gui->label_ks[nl])     != NULL  &&
        atd->line_dscrs             != NULL  &&
        atd->line_dscrs[nl].data_cn >= 0)
    {
        k->currflags = gui->a.pfr.cur_data[atd->line_dscrs[nl].data_cn].rflags;
        SetSimpleKnobState(k, choose_knobstate(k, k->currflags));
    }
}
void  FastadcGuiRenewPlot  (fastadc_gui_t *gui, int max_numpts, int info_changed)
{
  fastadc_type_dscr_t *atd = gui->a.atd;
  int                  nl;

    for (nl = 0;  nl < gui->a.atd->num_lines;  nl++)
        SetChnSwchState(gui, nl);

    UpdateView(gui,
               XhPlotSetMaxPts(gui->plot, max_numpts)
               ||
               info_changed
              );
    UpdateRepers(gui);
}

void  FastadcGuiSetReper   (fastadc_gui_t *gui, int nr, int x)
{
    set_rpr_to(gui, nr, x);
}

static psp_paramdescr_t  fastadc_gui_text2look[] =
{
    PSP_P_FLAG   ("nofold",       fastadc_gui_look_t, nofold,  1, 0),

    PSP_P_FLAG   ("noscrollbar",  fastadc_gui_look_t, noscrollbar,  1, 0),

    PSP_P_INT    ("width",        fastadc_gui_look_t, width,        -1, 0, 0),
    PSP_P_INT    ("height",       fastadc_gui_look_t, height,       -1, 0, 0),

    PSP_P_FLAG   ("black",        fastadc_gui_look_t, black,        1, 0),
    PSP_P_FLAG   ("white",        fastadc_gui_look_t, black,        0, 1),

    PSP_P_END()
};

psp_paramdescr_t *FastadcGuiCreateText2LookTable(fastadc_gui_dscr_t *gkd)
{
  psp_paramdescr_t *ret = NULL;
  size_t            ret_size;

  int               x;

    /* Allocate a table */
    ret_size = countof(fastadc_gui_text2look) * sizeof(*ret);
    ret = malloc(ret_size);
    if (ret == NULL)
    {
        fprintf(stderr, "%s %s(type=%s): unable to allocate %zu bytes for PSP-table\n",
                strcurtime(), __FUNCTION__, gkd->gkd.ftd->type_name, ret_size);
        return ret;
    }

    /* And fill it */
    for (x = 0;  x < countof(fastadc_gui_text2look);  x++)
    {
        ret[x] = fastadc_gui_text2look[x];
        if      (ret[x].name   == NULL);
        else if (ret[x].offset == PSP_OFFSET_OF(fastadc_gui_look_t, width))
        {
            ret[x].var.p_int.defval = gkd->def_plot_w;
            ret[x].var.p_int.minval = gkd->min_plot_w;
            ret[x].var.p_int.maxval = gkd->max_plot_w;
        }
        else if (ret[x].offset == PSP_OFFSET_OF(fastadc_gui_look_t, height))
        {
            ret[x].var.p_int.defval = gkd->def_plot_h;
            ret[x].var.p_int.minval = gkd->min_plot_h;
            ret[x].var.p_int.maxval = gkd->max_plot_h;
        }
    }

    return ret;
}
