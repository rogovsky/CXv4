#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/Separator.h>


#include "fastadc_data.h"
#include "fastadc_gui.h"
#include "pzframe_cpanel_decor.h"

#include "adc333_data.h"
#include "adc333_gui.h"

#include "drv_i/adc333_drv_i.h"


static Widget adc333_mkctls(pzframe_gui_t           *gui,
                            fastadc_type_dscr_t     *atd,
                            Widget                   parent,
                            pzframe_gui_mkstdctl_t   mkstdctl,
                            pzframe_gui_mkparknob_t  mkparknob)
{
  Widget  cform;    // Controls form
  Widget  commons;  // 
  Widget  dwgrid;   // Device-Wide-parameters grid
  Widget  vline;    // Separator line
  Widget  pcgrid;   // Per-Channel-parameters grid
  Widget  rpgrid;   // Repers grid
  Widget  ptsform;  // NUMPTS@PTSOFS form
  Widget  icform;   // Intra-cell form

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
    commons = mkstdctl(gui, cform, FASTADC_GUI_CTL_COMMONS, 0, 0);
    /* A grid for device-wide parameters */
    dwgrid = XhCreateGrid(cform, "grid");
    XhGridSetGrid   (dwgrid, 0, 0);
    XhGridSetSpacing(dwgrid, MOTIFKNOBS_INTERKNOB_H_SPACING, MOTIFKNOBS_INTERKNOB_V_SPACING);
    XhGridSetPadding(dwgrid, 0, 0);
    /* A separator line */
    vline = XtVaCreateManagedWidget("vline",
                                    xmSeparatorWidgetClass,
                                    cform,
                                    XmNorientation,     XmVERTICAL,
                                    NULL);
    /* A form for PTS@OFS */
    ptsform = XtVaCreateManagedWidget("form", xmFormWidgetClass, cform,
                                      XmNshadowThickness, 0,
                                      NULL);
    /* A commons<->ptsform "connector" */
    w1 = XtVaCreateManagedWidget("", widgetClass, cform,
                                 XmNwidth,       1,
                                 XmNheight,      1,
                                 XmNborderWidth, 0,
                                 NULL);
    attachleft (w1, commons, 0);
    attachright(w1, ptsform, 0);
    /* A grid for per-channel parameters */
    pcgrid = XhCreateGrid(cform, "grid");
    XhGridSetGrid   (pcgrid, 0, 0);
    XhGridSetSpacing(pcgrid, MOTIFKNOBS_INTERKNOB_H_SPACING, MOTIFKNOBS_INTERKNOB_V_SPACING);
    XhGridSetPadding(pcgrid, 0, 0);
    /* A grid for repers parameters */
    rpgrid = XhCreateGrid(cform, "grid");
    XhGridSetGrid   (rpgrid, 0, 0);
    XhGridSetSpacing(rpgrid, MOTIFKNOBS_INTERKNOB_H_SPACING, MOTIFKNOBS_INTERKNOB_V_SPACING);
    XhGridSetPadding(rpgrid, 0, 0);
    /* Perform attachment */
    attachright (rpgrid,  NULL,    0);
    attachright (ptsform, rpgrid,  MOTIFKNOBS_INTERKNOB_H_SPACING);
    attachright (pcgrid,  rpgrid,  MOTIFKNOBS_INTERKNOB_H_SPACING);
    attachtop   (pcgrid,  ptsform, MOTIFKNOBS_INTERKNOB_V_SPACING);
    attachright (vline,   pcgrid,  MOTIFKNOBS_INTERKNOB_H_SPACING);
    attachtop   (vline,   ptsform, 0);
    attachbottom(vline,   NULL,    0);
    attachright (dwgrid,  vline,   MOTIFKNOBS_INTERKNOB_H_SPACING);
    attachtop   (dwgrid,  ptsform, MOTIFKNOBS_INTERKNOB_V_SPACING);
    attachleft  (commons, NULL,    0);

    /* 2. Device-wide parameters */
    y = 0;

    /* Cmpr */
    MakeStdCtl(dwgrid, 1, y++, gui, mkstdctl, FASTADC_GUI_CTL_X_CMPR, 0, 0);

    /* Timing */
    MakeALabel (dwgrid, 0, y, "Timing");
    MakeParKnob(dwgrid, 1, y++,
                " look=selector rw label=''"
                " options=compact"
                " items='#T500ns\t1us\t2us\t4us\t8us\t16us\t32us\text'",
                gui, mkparknob, ADC333_CHAN_TIMING);
    
    /* IStart */
    MakeALabel (dwgrid, 0, y, "Int. Start");
    MakeParKnob(dwgrid, 1, y++,
                " look=led rw label='Int. Start' options='panel'",
                gui, mkparknob, ADC333_CHAN_ISTART);
    
    /* Wait time */
    MakeALabel (dwgrid, 0, y, "Wait time");
    MakeParKnob(dwgrid, 1, y++,
                " look=text rw"
                " options=''"
                " units=ms minnorm=0 maxnorm=1000000000 dpyfmt=%7.0f step=1000",
                gui, mkparknob, ADC333_CHAN_WAITTIME);
    
    XhGridSetSize(dwgrid, 2, y);

    /* Shot, Stop */
    icform = MakeAForm(dwgrid, 1, y++);
    XhGridSetChildFilling(icform, 1, 0);
    w1 = mkparknob(gui, icform,
                   " look=button rw label='Stop'",
                   ADC333_CHAN_STOP);
    attachright(w1, NULL, 0);
    w2 = mkparknob(gui, icform,
                   " look=button rw label='Shot!'",
                   ADC333_CHAN_SHOT);
    attachright(w2, w1, MOTIFKNOBS_INTERKNOB_H_SPACING);

    /* NUMPTS@PTSOFS */
    snprintf(spec, sizeof(spec),
             " look=text rw label='' step=100"
             " options=''"
             " minnorm=%d maxnorm=%d mindisp=%d maxdisp=%d dpyfmt='%%6.0f'",
             1, ADC333_MAX_NUMPTS, 1, ADC333_MAX_NUMPTS);
    w1 = mkparknob(gui, ptsform, spec, ADC333_CHAN_NUMPTS);
    snprintf(spec, sizeof(spec),
             " look=text rw label='@' step=100"
             " options='withlabel'"
             " minnorm=%d maxnorm=%d mindisp=%d maxdisp=%d dpyfmt='%%6.0f'",
             0, ADC333_MAX_NUMPTS-1, 0, ADC333_MAX_NUMPTS-1);
    w2 = mkparknob(gui, ptsform, spec, ADC333_CHAN_PTSOFS);
    attachleft(w2, w1, 0);

    /* 3. Per-channel parameters and repers */
    for (nl = 0;  nl < ADC333_NUM_LINES;  nl++)
    {
        w1 = MakeStdCtl(pcgrid, 0, nl, gui, mkstdctl, FASTADC_GUI_CTL_LINE_LABEL, nl, 1);
        XhGridSetChildFilling  (w1, 1,      0);
        MakeStdCtl     (pcgrid, 1, nl, gui, mkstdctl, FASTADC_GUI_CTL_LINE_MAGN,  nl, 1);
        MakeParKnob    (pcgrid, 2, nl,
                        " look=selector rw label=''"
                        " options=align=right"
                        " items='#T8.192V\t4.096V\t2.048V\t1.024V\tOff'",
                        gui, mkparknob, ADC333_CHAN_RANGE0 + nl);

        for (nr = 0;
             nr < XH_PLOT_NUM_REPERS;
             nr++)
        {
            if (nl == 0)
            {
                icform = MakeAForm(rpgrid, nr, 0);
                w1 = mkstdctl(gui, icform, FASTADC_GUI_CTL_REPER_ONOFF, 0, nr);
                w2 = mkstdctl(gui, icform, FASTADC_GUI_CTL_REPER_TIME,  0, nr);
                attachleft(w2, w1, NULL);
            }
            MakeStdCtl(rpgrid, nr, nl + 1, gui, mkstdctl, FASTADC_GUI_CTL_REPER_VALUE, nl, nr);
        }
    }
    XhGridSetSize(pcgrid, 3,                      nl);
    XhGridSetSize(rpgrid, XH_PLOT_NUM_REPERS, nl + 1);

    return cform;
}

pzframe_gui_dscr_t *adc333_get_gui_dscr(void)
{
  static fastadc_gui_dscr_t  dscr;
  static int                 dscr_inited;

    if (!dscr_inited)
    {
        FastadcGuiFillStdDscr(&dscr,
                              pzframe2fastadc_type_dscr(adc333_get_type_dscr()));

        dscr.def_plot_w = 700;
        dscr.min_plot_w = 100;
        dscr.max_plot_w = 3000;
        dscr.def_plot_h = 256;
        dscr.min_plot_h = 100;
        dscr.max_plot_h = 3000;

        dscr.cpanel_loc = XH_PLOT_CPANEL_AT_BOTTOM;
        dscr.mkctls     = adc333_mkctls;

        dscr_inited = 1;
    }
    return &(dscr.gkd);
}
