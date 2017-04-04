#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/Separator.h>


#include "vcamimg_data.h"
#include "vcamimg_gui.h"
#include "pzframe_cpanel_decor.h"

#include "ottcam_data.h"
#include "ottcam_gui.h"

#include "drv_i/ottcam_drv_i.h"


static Widget ottcam_mkctls(pzframe_gui_t           *gui,
                            vcamimg_type_dscr_t     *atd,
                            Widget                   parent,
                            pzframe_gui_mkstdctl_t   mkstdctl,
                            pzframe_gui_mkparknob_t  mkparknob)
{
  vcamimg_gui_t *vcamimg_gui = pzframe2vcamimg_gui(gui);

  Widget  cform;    // Controls form
  Widget  line1;
  Widget  line2;

  Widget  w1;
  Widget  w2;

    /* 0. General layout */
    /* A container form */
    cform = XtVaCreateManagedWidget("form", xmFormWidgetClass, parent,
                                    XmNshadowThickness, 0,
                                    NULL);
    /* Two lines */
    line1 = XtVaCreateManagedWidget("form", xmFormWidgetClass, cform,
                                    XmNshadowThickness, 0,
                                    NULL);
    line2 = XtVaCreateManagedWidget("form", xmFormWidgetClass, cform,
                                    XmNshadowThickness, 0,
                                    NULL);
    attachtop(line2, line1, MOTIFKNOBS_INTERKNOB_V_SPACING);

    /* 1. Line 1: standard controls */
    /* A "commons" */
    w1 = mkstdctl(gui, line1, VCAMIMG_GUI_CTL_COMMONS, 0, 0);

    w2 = mkstdctl(gui, line1, VCAMIMG_GUI_CTL_DPYMODE, 0, 0);
    attachleft(w2, w1, MOTIFKNOBS_INTERKNOB_H_SPACING);
    w1 = w2;

    w2 = mkstdctl(gui, line1, VCAMIMG_GUI_CTL_NORMALIZE, 0, 0);
    attachleft(w2, w1, MOTIFKNOBS_INTERKNOB_H_SPACING);
    w1 = w2;

    w2 = mkstdctl(gui, line1, VCAMIMG_GUI_CTL_MAX_RED, 0, 0);
    attachleft(w2, w1, MOTIFKNOBS_INTERKNOB_H_SPACING);
    w1 = w2;

    w2 = mkstdctl(gui, line1, VCAMIMG_GUI_CTL_0_VIOLET, 0, 0);
    attachleft(w2, w1, MOTIFKNOBS_INTERKNOB_H_SPACING);
    w1 = w2;

    /* 1. Line 2: parameters */
    w1 = mkparknob(gui, line2,
                   " look=led rw label='Int. Start' options='panel'",
                   OTTCAM_CHAN_ISTART);

    w2 = mkparknob(gui, line2,
                   " look=button rw label='Stop'",
                   OTTCAM_CHAN_STOP);
    attachleft(w2, w1, MOTIFKNOBS_INTERKNOB_H_SPACING);
    w1 = w2;

    w2 = mkparknob(gui, line2,
                   " look=button rw label='Shot!'",
                   OTTCAM_CHAN_SHOT);
    attachleft(w2, w1, MOTIFKNOBS_INTERKNOB_H_SPACING);
    w1 = w2;

    w2 = mkparknob(gui, line2,
                   " look=text rw label='Ampl=0.0625*' options='withlabel'"
                   " minnorm=16 maxnorm=64 step=1 dpyfmt='%2.0f'",
                   OTTCAM_CHAN_K);
    attachleft(w2, w1, MOTIFKNOBS_INTERKNOB_H_SPACING + 40);
    w1 = w2;

    w2 = mkparknob(gui, line2,
                   " look=text rw label='Expo=1/30us*' options='withlabel'"
                   " minnorm=0 maxnorm=65535 step=30 dpyfmt='%5.0f'",
                   OTTCAM_CHAN_T);
    attachleft(w2, w1, MOTIFKNOBS_INTERKNOB_H_SPACING);
    w1 = w2;

    w2 = mkparknob(gui, line2,
                   " look=choicebs rw label=''"
                   " items='#TImmed\bStart by itself immediately\blit=amber\tSync\bWait for SYNC impulse\blit=white'",
                   OTTCAM_CHAN_SYNC);
    attachleft(w2, w1, MOTIFKNOBS_INTERKNOB_H_SPACING);
    w1 = w2;

    return cform;
}

pzframe_gui_dscr_t *ottcam_get_gui_dscr(void)
{
  static vcamimg_gui_dscr_t  dscr;
  static int                 dscr_inited;

    if (!dscr_inited)
    {
        VcamimgGuiFillStdDscr(&dscr,
                              pzframe2vcamimg_type_dscr(ottcam_get_type_dscr()));

        dscr.cpanel_loc = 0/*!!!BOTTOM!!!*/;
        dscr.mkctls     = ottcam_mkctls;

        dscr_inited = 1;
    }
    return &(dscr.gkd);
}
