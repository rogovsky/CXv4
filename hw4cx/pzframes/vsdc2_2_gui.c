#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/Separator.h>


#include "fastadc_data.h"
#include "fastadc_gui.h"
#include "pzframe_cpanel_decor.h"

#include "vsdc2_2_data.h"
#include "vsdc2_2_gui.h"

#include "drv_i/vsdc2_drv_i.h"


static Widget vsdc2_2_mkctls(pzframe_gui_t           *gui,
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

    XhGridSetSize(dwgrid, 2, y);
    
    /* 3. Per-channel parameters and repers */
    for (nl = 0;  nl < VSDC2_NUM_LINES;  nl++)
    {
        y = 0;

        if (nl == 0)
            ;
        MakeStdCtl(pcgrid, nl + 1, y++, gui, mkstdctl, FASTADC_GUI_CTL_LINE_LABEL, nl, 0);

        if (nl == 0)
            MakeStdCtl(pcgrid, 0, y, gui, mkstdctl, FASTADC_GUI_CTL_X_CMPR, 0, 0);
        MakeStdCtl(pcgrid, nl + 1, y++, gui, mkstdctl, FASTADC_GUI_CTL_LINE_MAGN,  nl, 0);

        if (nl == 0)
            MakeALabel(pcgrid, 0, y, "Gain");
        MakeParKnob(pcgrid, nl + 1, y++,
                    " look=selector ro label=''"
                    " options=''"
                    " items='#Terror\t0.2V\t0.5V\t1V\t2V\t5V\t10V\t?111?'",
                    gui, mkparknob, VSDC2_CHAN_GAIN_n_base + nl);

        MakeParKnob(pcgrid, nl + 1, y++,
                    " look=button rw label='Get'",
                    gui, mkparknob, VSDC2_CHAN_GET_OSC0 + nl);

        if (nl == 0)
            MakeALabel(pcgrid, 0, y, "NumPts");
        MakeParKnob(pcgrid, nl + 1, y++,
                    " look=text ro label=''"
                    " options=''"
                    " dpyfmt=%6.0f",
                    gui, mkparknob, VSDC2_CHAN_CUR_NUMPTS0 + nl);

#if 0
        if (nl == 0)
            MakeALabel(pcgrid, 0, y, "@PtsOfs");
        snprintf(spec, sizeof(spec),
                 " look=text rw label='' step=10"
                 " options=''"
                 " minnorm=%d maxnorm=%d mindisp=%d maxdisp=%d dpyfmt='%%6.0f'",
                 1, VSDC2_MAX_NUMPTS, 1, VSDC2_MAX_NUMPTS);
        MakeParKnob(pcgrid, nl + 1, y++,
                    spec,
                    gui, mkparknob, VSDC2_CHAN_CUR_PTSOFS0 + nl);
#endif

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

        if (nl == 0)
            MakeALabel(pcgrid, 0, y, "Integral");
        MakeParKnob(pcgrid, nl + 1, y++,
                    " look=text ro"
                    " options=''"
                    " dpyfmt='%10.3e'",
                    gui, mkparknob, VSDC2_CHAN_INT_n_base + nl);
    }

    return cform;
}

pzframe_gui_dscr_t *vsdc2_2_get_gui_dscr(void)
{
  static fastadc_gui_dscr_t  dscr;
  static int                 dscr_inited;

    if (!dscr_inited)
    {
        FastadcGuiFillStdDscr(&dscr,
                              pzframe2fastadc_type_dscr(vsdc2_2_get_type_dscr()));

        dscr.def_plot_w = 700;
        dscr.min_plot_w = 100;
        dscr.max_plot_w = 3000;
        dscr.def_plot_h = 256;
        dscr.min_plot_h = 100;
        dscr.max_plot_h = 3000;

        dscr.cpanel_loc = XH_PLOT_CPANEL_AT_LEFT;
        dscr.mkctls     = vsdc2_2_mkctls;

        dscr_inited = 1;
    }
    return &(dscr.gkd);
}
