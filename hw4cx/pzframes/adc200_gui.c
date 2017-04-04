#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/Separator.h>


#include "fastadc_data.h"
#include "fastadc_gui.h"
#include "pzframe_cpanel_decor.h"

#include "adc200_data.h"
#include "adc200_gui.h"

#include "drv_i/adc200_drv_i.h"


static Widget adc200_mkctls(pzframe_gui_t           *gui,
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
  Widget  stsubw;
  Widget  stgrdc;

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

    /* 2. Device-wide parameters */
    y = 0;

    /* NUMPTS@PTSOFS */
    MakeALabel (dwgrid, 0, y, "Numpoints");
    icform = MakeAForm(dwgrid, 1, y++);
    snprintf(spec, sizeof(spec),
             " look=text rw label='' step=100"
             " options=''"
             " minnorm=%d maxnorm=%d mindisp=%d maxdisp=%d dpyfmt='%%6.0f'",
             1, ADC200_MAX_NUMPTS, 1, ADC200_MAX_NUMPTS);
    w1 = mkparknob(gui, icform, spec, ADC200_CHAN_NUMPTS);
    snprintf(spec, sizeof(spec),
             " look=text rw label='@' step=100"
             " options='withlabel'"
             " minnorm=%d maxnorm=%d mindisp=%d maxdisp=%d dpyfmt='%%6.0f'",
             0, ADC200_MAX_NUMPTS-1, 0, ADC200_MAX_NUMPTS-1);
    w2 = mkparknob(gui, icform, spec, ADC200_CHAN_PTSOFS);
    attachleft(w2, w1, 0);

    /* Timing */
    MakeALabel (dwgrid, 0, y, "Timing");
    MakeParKnob(dwgrid, 1, y++,
                " look=selector rw label=''"
                " options=compact"
                " items='#TQuartz 200MHz\tImpact 195MHz\tExt \"TIMER\"'",
                gui, mkparknob, ADC200_CHAN_TIMING);

    /* Frqdiv */
    MakeALabel (dwgrid, 0, y, "Frq. div");
    MakeParKnob(dwgrid, 1, y++,
                " look=selector rw label=''"
                " options=compact"
                " items='#Tf/1 (5ns)\tf/2 (10ns)\tf/4 (20ns)\tf/8 (40ns)'",
                gui, mkparknob, ADC200_CHAN_FRQDIV);

    /* IStart, Shot, Stop */
    icform = MakeAForm(dwgrid, 1, y++);
    XhGridSetChildFilling(icform, 1, 0);
    w1 = mkparknob(gui, icform,
                   " look=led rw label='Int. Start' options='panel'",
                   ADC200_CHAN_ISTART);
    w1 = mkparknob(gui, icform,
                   " look=button rw label='Stop'",
                   ADC200_CHAN_STOP);
    attachright(w1, NULL, 0);
    w2 = mkparknob(gui, icform,
                   " look=button rw label='Shot!'",
                   ADC200_CHAN_SHOT);
    attachright(w2, w1, MOTIFKNOBS_INTERKNOB_H_SPACING);
    
    /* Wait time */
    MakeALabel (dwgrid, 0, y, "Wait time");
    MakeParKnob(dwgrid, 1, y++,
                " look=text rw"
                " options=''"
                " units=ms minnorm=0 maxnorm=1000000000 dpyfmt=%7.0f step=1000",
                gui, mkparknob, ADC200_CHAN_WAITTIME);
    
    XhGridSetSize(dwgrid, 2, y);
    
    /* 3. Per-channel parameters and repers */
    for (nl = 0;  nl < ADC200_NUM_LINES;  nl++)
    {
        y = 0;

        if (nl == 0)
            ;
        MakeStdCtl(pcgrid, nl + 1, y++, gui, mkstdctl, FASTADC_GUI_CTL_LINE_LABEL, nl, 0);

        if (nl == 0)
            MakeStdCtl(pcgrid, 0, y, gui, mkstdctl, FASTADC_GUI_CTL_X_CMPR, 0, 0);
        MakeStdCtl(pcgrid, nl + 1, y++, gui, mkstdctl, FASTADC_GUI_CTL_LINE_MAGN,  nl, 0);
        
        if (nl == 0)
            MakeALabel(pcgrid, 0, y, "Scale, V");
        MakeParKnob(pcgrid, nl + 1, y++,
                    " look=selector rw label=''"
                    " options=align=right"
                    " items='#T0.256\t0.512\t1.024\t2.048'",
                    gui, mkparknob, ADC200_CHAN_RANGE1 + nl);

        if (nl == 0)
            MakeALabel(pcgrid, 0, y, "Zero");
        MakeParKnob(pcgrid, nl + 1, y++,
                    " look=text rw label=''"
                    " minnorm=0 maxnorm=255 dpyfmt='%3.0f'",
                    gui, mkparknob, ADC200_CHAN_ZERO1 + nl);

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

    /* Stats */
    stsubw = MakeSubwin(pcgrid, 0, y,
                        "Stats...", "Statistics");

    XhGridSetSize(pcgrid, 3, y);

    /* Statistics */
    stgrdc = XhCreateGrid(stsubw, "grid");
    XhGridSetGrid   (stgrdc, 0, 0);
    XhGridSetSpacing(stgrdc, MOTIFKNOBS_INTERKNOB_H_SPACING, MOTIFKNOBS_INTERKNOB_V_SPACING);
    XhGridSetPadding(stgrdc, 0, 0);
    for (nl = 0;  nl < ADC200_NUM_LINES;  nl++)
    {
        snprintf(spec, sizeof(spec),
                 "%s", fastadc_gui->a.dcnv.lines[nl].descr);
        MakeALabel(stgrdc, 0, nl + 1, spec);

        if (nl == 0)
            MakeALabel(stgrdc, 1, 0, "Min");
        MakeParKnob   (stgrdc, 1, nl + 1,
                       " look=text ro",
                       gui, mkparknob, ADC200_CHAN_MIN1 + nl);

        if (nl == 0)
            MakeALabel(stgrdc, 2, 0, "Max");
        MakeParKnob   (stgrdc, 2, nl + 1,
                       " look=text ro",
                       gui, mkparknob, ADC200_CHAN_MAX1 + nl);

        if (nl == 0)
            MakeALabel(stgrdc, 3, 0, "Avg");
        MakeParKnob   (stgrdc, 3, nl + 1,
                       " look=text ro",
                       gui, mkparknob, ADC200_CHAN_AVG1 + nl);

        if (nl == 0)
            MakeALabel(stgrdc, 4, 0, "Sum");
        MakeParKnob   (stgrdc, 4, nl + 1,
                       " look=text ro",
                       gui, mkparknob, ADC200_CHAN_INT1 + nl);
    }
    MakeParKnob(stgrdc, 0, 0,
                " look=onoff rw options=color=blue label='Calc'",
                gui, mkparknob, ADC200_CHAN_CALC_STATS);
    XhGridSetSize(stgrdc, 5, nl + 1);

    return cform;
}

pzframe_gui_dscr_t *adc200_get_gui_dscr(void)
{
  static fastadc_gui_dscr_t  dscr;
  static int                 dscr_inited;

    if (!dscr_inited)
    {
        FastadcGuiFillStdDscr(&dscr,
                              pzframe2fastadc_type_dscr(adc200_get_type_dscr()));

        dscr.def_plot_w = 700;
        dscr.min_plot_w = 100;
        dscr.max_plot_w = 3000;
        dscr.def_plot_h = 256;
        dscr.min_plot_h = 100;
        dscr.max_plot_h = 3000;

        dscr.cpanel_loc = XH_PLOT_CPANEL_AT_LEFT;
        dscr.mkctls     = adc200_mkctls;

        dscr_inited = 1;
    }
    return &(dscr.gkd);
}
