#include "datatreeP.h" // For addressing k->w only

#include "vcamimg_knobplugin.h"


int VcamimgKnobpluginDoCreate(DataKnob k, CxWidget parent,
                              pzframe_knobplugin_realize_t  kp_realize,
                              pzframe_gui_dscr_t *gkd,
                              psp_paramdescr_t *privrec_tbl, void *privrec)
{
  vcamimg_knobplugin_t *kpn = k->privptr;
  int                   ret = -1;

  vcamimg_gui_t        *my_gui = &(kpn->g);
  vcamimg_gui_dscr_t   *my_gkd = pzframe2vcamimg_gui_dscr(gkd);

  char                 *buf_to_free = NULL;
  psp_paramdescr_t     *gui_table   = NULL;

    VcamimgGuiInit(my_gui, my_gkd);

    if ((gui_table = VcamimgGuiCreateText2LookTable(my_gkd)) == NULL)
        goto FINISH;

    ret = PzframeKnobpluginDoCreate(k, parent,
                                    kp_realize,
                                    &(my_gui->g), gkd,
                                    NULL/*dcnv*/,NULL,
                                    gui_table,   &(my_gui->look),
                                    privrec_tbl, privrec);

 FINISH:
    safe_free(buf_to_free);
    safe_free(gui_table);

    ////fprintf(stderr, "\"%s\"/%s: %d\n", ki->label, kpn->kp.g->d->ftd->type_name, kpn->kp.g->pfr->bigc_sid);
    return ret;
}
