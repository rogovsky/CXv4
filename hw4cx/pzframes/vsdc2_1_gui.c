#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/Separator.h>


#include "fastadc_data.h"
#include "fastadc_gui.h"
#include "pzframe_cpanel_decor.h"

#include "vsdc2_1_data.h"
#include "vsdc2_1_gui.h"

#include "drv_i/vsdc2_drv_i.h"


static Widget vsdc2_1_mkctls(pzframe_gui_t           *gui,
                             fastadc_type_dscr_t     *atd,
                             Widget                   parent,
                             pzframe_gui_mkstdctl_t   mkstdctl,
                             pzframe_gui_mkparknob_t  mkparknob)
{
  Widget  cform;    // Controls form
  Widget  dwform;   // Device-Wide-parameters form
  Widget  rpgrid;   // Repers grid
  Widget  stgrdc;
  Widget  icform;   // Intra-cell form
  Widget  w1;
  Widget  w2;
  Widget  left;

  int     nr;

  char    spec[1000];

    /* 1. General layout */
    /* A container form */
    cform = XtVaCreateManagedWidget("form", xmFormWidgetClass, parent,
                                    XmNshadowThickness, 0,
                                    NULL);
    /* A grid for repers parameters */
    rpgrid = XhCreateGrid(cform, "grid");
    XhGridSetGrid   (rpgrid, 0, 0);
    XhGridSetSpacing(rpgrid, MOTIFKNOBS_INTERKNOB_H_SPACING, MOTIFKNOBS_INTERKNOB_V_SPACING);
    XhGridSetPadding(rpgrid, 0, 0);
    /* Statistics */
    stgrdc = XhCreateGrid(cform, "grid");
    XhGridSetGrid   (stgrdc, 0, 0);
    XhGridSetSpacing(stgrdc, MOTIFKNOBS_INTERKNOB_H_SPACING, MOTIFKNOBS_INTERKNOB_V_SPACING);
    XhGridSetPadding(stgrdc, 0, 0);
    /* Perform attachment */
    attachleft(stgrdc, rpgrid, MOTIFKNOBS_INTERKNOB_H_SPACING);

    /* 3. Repers */
    /* Commons, Device-wide parameters */
    /* A "commons" */
    MakeStdCtl        (rpgrid, 0, 0, gui, mkstdctl, FASTADC_GUI_CTL_COMMONS, 0, 0);
    /* A form for device-wide parameters */
    dwform = MakeAForm(rpgrid, 0, 1);
    w1 = mkparknob(gui, dwform,
                   " look=button rw label='Get'",
                   VSDC2_CHAN_GET_OSC0);
    attachleft  (w1, NULL,   0); left = w1;
    w1 = mkparknob(gui, dwform,
                   " look=text ro label=''"
                   " options='' units=pts"
                   " dpyfmt=%6.0f",
                   VSDC2_CHAN_CUR_NUMPTS0);
    attachleft  (w1, left, MOTIFKNOBS_INTERKNOB_H_SPACING); left = w1;
#if 0
    snprintf(spec, sizeof(spec),
             " look=text rw label='@' step=10"
             " options='withlabel'"
             " minnorm=%d maxnorm=%d mindisp=%d maxdisp=%d dpyfmt='%%6.0f'",
             1, VSDC2_MAX_NUMPTS, 1, VSDC2_MAX_NUMPTS);
    w1 = mkparknob(gui, dwform,
                   spec,
                   VSDC2_CHAN_CUR_PTSOFS0);
    attachleft  (w1, left,   0); left = w1;
#endif
    w1 = mkparknob(gui, dwform,
                   " look=selector ro label='Gain:'"
                   " options=''"
                   " items='#Terror\t0.2V\t0.5V\t1V\t2V\t5V\t10V\t?111?'",
                   VSDC2_CHAN_GAIN0);
    attachleft  (w1, left, MOTIFKNOBS_INTERKNOB_H_SPACING); left = w1;
    /* Cmpr, magn */
    MakeStdCtl(rpgrid, 1, 0, gui, mkstdctl, FASTADC_GUI_CTL_X_CMPR,    0, 0);
    MakeStdCtl(rpgrid, 1, 1, gui, mkstdctl, FASTADC_GUI_CTL_LINE_MAGN, 0, 1);
    /* Repers themselves */
    for (nr = 0;
         nr < XH_PLOT_NUM_REPERS;
         nr++)
    {
        icform = MakeAForm(rpgrid, 2 + nr, 0);
        w1 = mkstdctl(gui, icform, FASTADC_GUI_CTL_REPER_ONOFF, 0, nr);
        w2 = mkstdctl(gui, icform, FASTADC_GUI_CTL_REPER_TIME,  0, nr);
        attachleft(w2, w1, NULL);
        MakeStdCtl(rpgrid, 2 + nr, 0 + 1, gui, mkstdctl, FASTADC_GUI_CTL_REPER_VALUE, 0, nr);
    }
    MakeALabel (rpgrid, 2 + 3, 0, "Integral");
    MakeParKnob(rpgrid, 2 + 3, 1,
                " look=text ro"
                " options=''"
                " dpyfmt='%10.3e'",
                gui, mkparknob, VSDC2_CHAN_INT_n_base + 0);
    XhGridSetSize(rpgrid, 2 + XH_PLOT_NUM_REPERS + 1, 1 + 1);

#if 0
    /* 4. Statistics */
    MakeALabel (stgrdc, 0, 0, "Min");
    MakeParKnob(stgrdc, 0, 0 + 1,
                " look=text ro",
                gui, mkparknob, VSDC2_CHAN_MIN0);
    MakeALabel (stgrdc, 1, 0, "Max");
    MakeParKnob(stgrdc, 1, 0 + 1,
                " look=text ro",
                gui, mkparknob, VSDC2_CHAN_MAX0);
    MakeALabel (stgrdc, 2, 0, "Avg");
    MakeParKnob(stgrdc, 2, 0 + 1,
                " look=text ro",
                gui, mkparknob, VSDC2_CHAN_AVG0);
    MakeALabel (stgrdc, 3, 0, "Sum");
    MakeParKnob(stgrdc, 3, 0 + 1,
                " look=text ro",
                gui, mkparknob, VSDC2_CHAN_INT0);
    XhGridSetSize(stgrdc, 4, 1 + 1);
#endif

    return cform;
}

pzframe_gui_dscr_t *vsdc2_1_get_gui_dscr(void)
{
  static fastadc_gui_dscr_t  dscr;
  static int                 dscr_inited;

    if (!dscr_inited)
    {
        FastadcGuiFillStdDscr(&dscr,
                              pzframe2fastadc_type_dscr(vsdc2_1_get_type_dscr()));

        dscr.def_plot_w = 700;
        dscr.min_plot_w = 100;
        dscr.max_plot_w = 3000;
        dscr.def_plot_h = 256;
        dscr.min_plot_h = 100;
        dscr.max_plot_h = 3000;

        dscr.cpanel_loc = XH_PLOT_CPANEL_AT_TOP;
        dscr.mkctls     = vsdc2_1_mkctls;

        dscr_inited = 1;
    }
    return &(dscr.gkd);
}
