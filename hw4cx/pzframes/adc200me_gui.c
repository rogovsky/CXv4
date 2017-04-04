#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/Separator.h>


#include "fastadc_data.h"
#include "fastadc_gui.h"
#include "pzframe_cpanel_decor.h"

#include "adc200me_data.h"
#include "adc200me_gui.h"

#include "drv_i/adc200me_drv_i.h"


static Widget adc200me_mkctls(pzframe_gui_t           *gui,
                              fastadc_type_dscr_t     *atd,
                              Widget                   parent,
                              pzframe_gui_mkstdctl_t   mkstdctl,
                              pzframe_gui_mkparknob_t  mkparknob)
{
  fastadc_gui_t *fastadc_gui = pzframe2fastadc_gui(gui);

  Widget  cform;    // Controls form
  Widget  dwgrid;   // Device-Wide-parameters grid
  Widget  hline;    // Separator line
  Widget  pcgrid;   // Per-Channel-parameters grid
  Widget  icform;   // Intra-cell form
  Widget  clsubw;
  Widget  clform;
  Widget  clgrdg;
  Widget  clgrdc;

  Widget  w1;
  Widget  w2;

  int     y;
  int     nl;
  int     nr;

  char    spec[1000];

    /* 1. General layout */
    /* A container form */
    cform = XtVaCreateManagedWidget("form", xmFormWidgetClass, parent,
                                    XmNshadowThickness, 0,
                                    NULL);
    /* A "commons" */
    w1 = mkstdctl(gui, cform, FASTADC_GUI_CTL_COMMONS, 0, 0);
    attachleft  (w1, NULL,   0);
    attachtop   (w1, NULL,   0);
    /* A grid for device-wide parameters */
    dwgrid = XhCreateGrid(cform, "grid");
    XhGridSetGrid   (dwgrid, 0, 0);
    XhGridSetSpacing(dwgrid, MOTIFKNOBS_INTERKNOB_H_SPACING, MOTIFKNOBS_INTERKNOB_V_SPACING);
    XhGridSetPadding(dwgrid, 0, 0);
    attachright     (dwgrid, NULL, 0);
    attachtop       (dwgrid, w1,   0);
    /* A separator line */
    hline = XtVaCreateManagedWidget("hline",
                                    xmSeparatorWidgetClass,
                                    cform,
                                    XmNorientation,     XmHORIZONTAL,
                                    NULL);
    attachleft (hline, NULL,   0);
    attachright(hline, NULL,   0);
    attachtop  (hline, dwgrid, MOTIFKNOBS_INTERKNOB_V_SPACING);
    /* A grid for per-channel parameters */
    pcgrid = XhCreateGrid(cform, "grid");
    XhGridSetGrid   (pcgrid, 0, 0);
    XhGridSetSpacing(pcgrid, MOTIFKNOBS_INTERKNOB_H_SPACING, MOTIFKNOBS_INTERKNOB_V_SPACING);
    XhGridSetPadding(pcgrid, 0, 0);
    attachleft      (pcgrid, NULL,  0);
    attachright     (pcgrid, NULL,  0);
    attachtop       (pcgrid, hline, MOTIFKNOBS_INTERKNOB_V_SPACING);

//return cform;
    /* 2. Device-wide parameters */
    y = 0;

    /* NUMPTS@PTSOFS */
    MakeALabel (dwgrid, 0, y, "Numpoints");
    icform = MakeAForm(dwgrid, 1, y++);
    snprintf(spec, sizeof(spec),
             " look=text rw label='' step=100"
             " options=''"
             " minalwd=%d maxalwd=%d dpyfmt='%%6.0f'",
             1, ADC200ME_MAX_NUMPTS);
    w1 = mkparknob(gui, icform, spec, ADC200ME_CHAN_NUMPTS);
    snprintf(spec, sizeof(spec),
             " look=text rw label='@' step=100"
             " options='withlabel'"
             " minalwd=%d maxalwd=%d dpyfmt='%%6.0f'",
             0, ADC200ME_MAX_NUMPTS-1);
    w2 = mkparknob(gui, icform, spec, ADC200ME_CHAN_PTSOFS);
    attachleft(w2, w1, 0);

    /* Timing */
    MakeALabel (dwgrid, 0, y, "Timing");
    MakeParKnob(dwgrid, 1, y++,
                " look=selector rw label=''"
                " options=compact"
                " items='#TQuartz 200MHz\tExt \"TIMER\"'",
                gui, mkparknob, ADC200ME_CHAN_TIMING);

    /* IStart, Shot, Stop */
    MakeParKnob(dwgrid, 0, y,
                " look=text ro dpyfmt=%-6.0f label='#' options=withlabel tip='Serial #'",
                gui, mkparknob, ADC200ME_CHAN_SERIAL);
    icform = MakeAForm(dwgrid, 1, y++);
    XhGridSetChildFilling(icform, 1, 0);
    w1 = mkparknob(gui, icform,
                   " look=led rw label='Int. Start' options='panel'",
                   ADC200ME_CHAN_ISTART);
    w1 = mkparknob(gui, icform,
                   " look=button rw label='Stop'",
                   ADC200ME_CHAN_STOP);
    attachright(w1, NULL, 0);
    w2 = mkparknob(gui, icform,
                   " look=button rw label='Shot!'",
                   ADC200ME_CHAN_SHOT);
    attachright(w2, w1, MOTIFKNOBS_INTERKNOB_H_SPACING);
    
    /* Wait time */
    MakeALabel (dwgrid, 0, y, "Wait time");
    MakeParKnob(dwgrid, 1, y++,
                " look=text rw"
                " options=''"
                " units=ms minalwd=0 maxalwd=1000000000 dpyfmt=%7.0f step=1000",
                gui, mkparknob, ADC200ME_CHAN_WAITTIME);
    
    XhGridSetSize(dwgrid, 2, y);
    
    /* 3. Per-channel parameters and repers */
    for (nl = 0;  nl < ADC200ME_NUM_LINES;  nl++)
    {
        y = 0;

        if (nl == 0)
            clsubw = MakeSubwin(pcgrid, 0, y,
                                "Calibr...", "Calibrations");
        MakeStdCtl(pcgrid, nl + 1, y++, gui, mkstdctl, FASTADC_GUI_CTL_LINE_LABEL, nl, 0);

        if (nl == 0)
            MakeStdCtl(pcgrid, 0, y, gui, mkstdctl, FASTADC_GUI_CTL_X_CMPR, 0, 0);
        MakeStdCtl(pcgrid, nl + 1, y++, gui, mkstdctl, FASTADC_GUI_CTL_LINE_MAGN,  nl, 0);
        
        if (nl == 0)
            MakeALabel(pcgrid, 0, y, "Scale, V");
        MakeParKnob(pcgrid, nl + 1, y++,
                    " look=selector rw label=''"
                    " options=align=right"
                    " items='#T4.096\t2.048\t1.024\t0.512'",
                    gui, mkparknob, ADC200ME_CHAN_RANGE1 + nl);

        for (nr = 0;  nr < XH_PLOT_NUM_REPERS;  nr++, y++)
        {
            if (nl == 0)
            {
                icform = MakeAForm(pcgrid, 0, y);
                w1 = mkstdctl(gui, icform, FASTADC_GUI_CTL_REPER_ONOFF, 0, nr);
                w2 = mkstdctl(gui, icform, FASTADC_GUI_CTL_REPER_TIME,  0, nr);
                attachleft(w2, w1, NULL);
            }
            MakeStdCtl(pcgrid, nl + 1, y, gui, mkstdctl, FASTADC_GUI_CTL_REPER_VALUE, nl, nr);
        }
    }

    XhGridSetSize(pcgrid, 3, y);

    /* 4. Calibration */
    clform = XtVaCreateManagedWidget("form", xmFormWidgetClass, clsubw,
                                     XmNshadowThickness, 0,
                                     NULL);
    clgrdg = XhCreateGrid(clform, "grid");
    XhGridSetGrid   (clgrdg, 0, 0);
    XhGridSetSpacing(clgrdg, MOTIFKNOBS_INTERKNOB_H_SPACING, MOTIFKNOBS_INTERKNOB_V_SPACING);
    XhGridSetPadding(clgrdg, 0, 0);
    clgrdc = XhCreateGrid(clform, "grid");
    XhGridSetGrid   (clgrdc, 0, 0);
    XhGridSetSpacing(clgrdc, MOTIFKNOBS_INTERKNOB_H_SPACING, MOTIFKNOBS_INTERKNOB_V_SPACING);
    XhGridSetPadding(clgrdc, 0, 0);
    attachtop(clgrdc, clgrdg, MOTIFKNOBS_INTERKNOB_V_SPACING);

    y = 0;
    MakeParKnob(clgrdg, 0, y++,
                " look=button rw label='Calibrate'",
                gui, mkparknob, ADC200ME_CHAN_CALIBRATE);
    MakeParKnob(clgrdg, 0, y++,
                " look=button rw label='Reset calibration'",
                gui, mkparknob, ADC200ME_CHAN_FGT_CLB);
    MakeParKnob(clgrdg, 0, y++,
                " look=onoff rw options=color=blue label='Visible calibration'",
                gui, mkparknob, ADC200ME_CHAN_VISIBLE_CLB);
    MakeParKnob(clgrdg, 0, y++,
                " look=led ro label='Active'",
                gui, mkparknob, ADC200ME_CHAN_CLB_STATE);
    XhGridSetSize(clgrdg, 1, y);

    for (nl = 0;  nl < ADC200ME_NUM_LINES;  nl++)
    {
        y = 0;

        snprintf(spec, sizeof(spec),
                 "%s", fastadc_gui->a.dcnv.lines[nl].descr);
        MakeALabel(clgrdc, nl + 1, y++, spec);

        if (nl == 0)
            MakeALabel(clgrdc, 0, y, "d0");
        MakeParKnob(clgrdc, nl + 1, y++,
                    " look=text ro",
                    gui, mkparknob, ADC200ME_CHAN_CLB_OFS1 + nl);

        if (nl == 0)
            MakeALabel(clgrdc, 0, y, "Cor");
        MakeParKnob(clgrdc, nl + 1, y++,
                    " look=text ro",
                    gui, mkparknob, ADC200ME_CHAN_CLB_COR1 + nl);
    }

    XhGridSetSize(clgrdc, 3, y);

    return cform;
}

pzframe_gui_dscr_t *adc200me_get_gui_dscr(void)
{
  static fastadc_gui_dscr_t  dscr;
  static int                 dscr_inited;

    if (!dscr_inited)
    {
        FastadcGuiFillStdDscr(&dscr,
                              pzframe2fastadc_type_dscr(adc200me_get_type_dscr()));

        dscr.def_plot_w = 700;
        dscr.min_plot_w = 100;
        dscr.max_plot_w = 3000;
        dscr.def_plot_h = 256;
        dscr.min_plot_h = 100;
        dscr.max_plot_h = 3000;

        dscr.cpanel_loc = XH_PLOT_CPANEL_AT_LEFT;
        dscr.mkctls     = adc200me_mkctls;

        dscr_inited = 1;
    }
    return &(dscr.gkd);
}
