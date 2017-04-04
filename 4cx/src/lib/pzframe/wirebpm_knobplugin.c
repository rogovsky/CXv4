#include "datatreeP.h" // For addressing k->w only

#include "wirebpm_knobplugin.h"


int WirebpmKnobpluginDoCreate(DataKnob k, CxWidget parent,
                              pzframe_knobplugin_realize_t  kp_realize,
                              pzframe_gui_dscr_t *gkd,
                              psp_paramdescr_t *privrec_tbl, void *privrec)
{
  wirebpm_knobplugin_t *kpn = k->privptr;
  int                   ret = -1;

  wirebpm_gui_t        *my_gui = &(kpn->g);
  wirebpm_gui_dscr_t   *my_gkd = pzframe2wirebpm_gui_dscr(gkd);

  psp_paramdescr_t     *dcnv_table  = WirebpmDataGetDcnvTable();
  char                 *buf_to_free = NULL;
  psp_paramdescr_t     *gui_table   = NULL;

    WirebpmGuiInit(my_gui, my_gkd);

    if ((gui_table = WirebpmGuiCreateText2LookTable(my_gkd)) == NULL)
        goto FINISH;

    ret = PzframeKnobpluginDoCreate(k, parent,
                                    kp_realize,
                                    &(my_gui->g), gkd,
                                    dcnv_table,  &(my_gui->b.dcnv),
                                    gui_table,   &(my_gui->look),
                                    privrec_tbl, privrec);

 FINISH:
    safe_free(buf_to_free);
    safe_free(gui_table);

    ////fprintf(stderr, "\"%s\"/%s: %d\n", ki->label, kpn->kp.g->d->ftd->type_name, kpn->kp.g->pfr->bigc_sid);
    return ret;
}
