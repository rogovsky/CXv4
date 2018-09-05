#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/Separator.h>


#include "fastadc_data.h"
#include "fastadc_gui.h"
#include "pzframe_cpanel_decor.h"

#include "f4226_data.h"
#include "f4226_gui.h"

#include "drv_i/f4226_drv_i.h"


static Widget f4226_mkctls(pzframe_gui_t           *gui,
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
    /* A form for device-wide parameters */
    dwform = XtVaCreateManagedWidget("form", xmFormWidgetClass, cform,
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
    attachtop (rpgrid, dwform, MOTIFKNOBS_INTERKNOB_V_SPACING);
    attachtop (stgrdc, dwform, MOTIFKNOBS_INTERKNOB_V_SPACING);
    attachleft(stgrdc, rpgrid, MOTIFKNOBS_INTERKNOB_H_SPACING);

    /* A "commons" */
    w1 = mkstdctl(gui, dwform, FASTADC_GUI_CTL_COMMONS, 0, 0);
    attachleft  (w1, NULL,   0); left = w1;

    /* 2. Device-wide parameters */
    /* NUMPTS@PTSOFS */
    snprintf(spec, sizeof(spec),
             " look=text rw label='Numpoints' step=100"
             " options='withlabel'"
             " minnorm=%d maxnorm=%d mindisp=%d maxdisp=%d dpyfmt='%%4.0f'",
             1, F4226_MAX_NUMPTS, 1, F4226_MAX_NUMPTS);
    w1 = mkparknob(gui, dwform, spec, F4226_CHAN_NUMPTS);
    attachleft(w1, left, MOTIFKNOBS_INTERKNOB_H_SPACING); left = w1;
    snprintf(spec, sizeof(spec),
             " look=text rw label='@' step=100"
             " options='withlabel'"
             " minnorm=%d maxnorm=%d mindisp=%d maxdisp=%d dpyfmt='%%4.0f'",
             0, F4226_MAX_NUMPTS-1, 0, F4226_MAX_NUMPTS-1);
    w1 = mkparknob(gui, dwform, spec, F4226_CHAN_PTSOFS);
    attachleft(w1, left, 0); left = w1;
    /* History shift */
    w1 = mkparknob(gui, dwform,
                   " look=selector rw label=',' tip='Pre-history points'"
                   " options='align=right'"
                   " items='0\t-64\t-128\t-192'"
                          "'\t-256\t-320\t-384\t-448'"
                          "'\t-512\t-576\t-640\t-704'"
                          "'\t-768\t-832\t-896\t-960'",
                   F4226_CHAN_PREHIST64);
    attachleft(w1, left, 0); left = w1;

    /* IStart, Shot, Stop */
    w1 = mkparknob(gui, dwform,
                   " look=led ro label='Int. Start' options='panel'",
                   F4226_CHAN_ISTART);
    attachleft(w1, left, MOTIFKNOBS_INTERKNOB_H_SPACING); left = w1;
    w1 = mkparknob(gui, dwform,
                   " look=button rw label='Stop'",
                   F4226_CHAN_STOP);
    attachleft(w1, left, 0); left = w1;
    w1 = mkparknob(gui, dwform,
                   " look=button rw label='Shot!'",
                   F4226_CHAN_SHOT);
    attachleft(w1, left, 0); left = w1;

    /* 3. Repers */
    /* Timing, Range */
    MakeParKnob(rpgrid, 0, 0,
                " look=selector rw label='Timing' "
                " items='50ns\t100ns\t200ns\t400ns\t800ns\t1.6us\t3.2us\tExt'",
                gui, mkparknob, F4226_CHAN_TIMING);
    MakeParKnob(rpgrid, 0, 1,
                " look=selector rw label='Range' "
                " items='256mV\t512mV\t1V\tTst256mV'",
                gui, mkparknob, F4226_CHAN_RANGE);
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
    XhGridSetSize(rpgrid, 2 + XH_PLOT_NUM_REPERS, 1 + 1);

    /* 4. Statistics */
    MakeALabel (stgrdc, 0, 0, "Min");
    MakeParKnob(stgrdc, 0, 0 + 1,
                " look=text ro",
                gui, mkparknob, F4226_CHAN_MIN);
    MakeALabel (stgrdc, 1, 0, "Max");
    MakeParKnob(stgrdc, 1, 0 + 1,
                " look=text ro",
                gui, mkparknob, F4226_CHAN_MAX);
    MakeALabel (stgrdc, 2, 0, "Avg");
    MakeParKnob(stgrdc, 2, 0 + 1,
                " look=text ro",
                gui, mkparknob, F4226_CHAN_AVG);
    MakeALabel (stgrdc, 3, 0, "Sum");
    MakeParKnob(stgrdc, 3, 0 + 1,
                " look=text ro",
                gui, mkparknob, F4226_CHAN_INT);
    XhGridSetSize(stgrdc, 4, 1 + 1);

    return cform;
}

pzframe_gui_dscr_t *f4226_get_gui_dscr(void)
{
  static fastadc_gui_dscr_t  dscr;
  static int                 dscr_inited;

    if (!dscr_inited)
    {
        FastadcGuiFillStdDscr(&dscr,
                              pzframe2fastadc_type_dscr(f4226_get_type_dscr()));

        dscr.def_plot_w = 700;
        dscr.min_plot_w = 100;
        dscr.max_plot_w = 3000;
        dscr.def_plot_h = 256;
        dscr.min_plot_h = 100;
        dscr.max_plot_h = 3000;

        dscr.cpanel_loc = XH_PLOT_CPANEL_AT_TOP;
        dscr.mkctls     = f4226_mkctls;

        dscr_inited = 1;
    }
    return &(dscr.gkd);
}
