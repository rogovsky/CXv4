#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include "wirebpm_data.h"
#include "wirebpm_gui.h"
#include "pzframe_cpanel_decor.h"

#include "u0632_data.h"
#include "u0632_gui.h"

#include "drv_i/u0632_drv_i.h"
#include "u0632_internals.h"


static Widget u0632_mkctls(pzframe_gui_t           *gui,
                           wirebpm_type_dscr_t     *atd,
                           Widget                   parent,
                           pzframe_gui_mkstdctl_t   mkstdctl,
                           pzframe_gui_mkparknob_t  mkparknob)
{
  wirebpm_gui_t *wirebpm_gui = pzframe2wirebpm_gui(gui);

  Widget  cform;    // Controls form
  Widget  w1;
  Widget  w2;

    /* A container form */
    cform = XtVaCreateManagedWidget("form", xmFormWidgetClass, parent,
                                    XmNshadowThickness, 0,
                                    NULL);
    attachleft (cform, NULL, 0);
    attachright(cform, NULL, 0);

    w1 = mkparknob(gui, cform, 
                   " look=selector rw label='M' tip='Sensitivity'"
                   " items='#T400.00e3\t100.00e3\t25.00e3\t6.25e3'",
                   C_M);

    w2 = mkparknob(gui, cform,
                   " look=selector rw label='P' tip='Delay of start'"
                   " items='#F32f,0.1088,0.0,%4.2fms'",
                   C_P);
    attachleft(w2, w1, MOTIFKNOBS_INTERKNOB_H_SPACING);
    w1 = w2;

    w2 = mkstdctl(gui, cform, WIREBPM_GUI_CTL_MAGN, 0, 0);
    attachright(w2, NULL, 0);

    return cform;
}

pzframe_gui_dscr_t *u0632_get_gui_dscr(void)
{
  static wirebpm_gui_dscr_t  dscr;
  static int                 dscr_inited;

    if (!dscr_inited)
    {
        WirebpmGuiFillStdDscr(&dscr,
                              pzframe2wirebpm_type_dscr(u0632_get_type_dscr()));

        dscr.cpanel_loc = 0/*!!!BOTTOM!!!*/;
        dscr.mkctls     = u0632_mkctls;

        dscr_inited = 1;
    }
    return &(dscr.gkd);
}
