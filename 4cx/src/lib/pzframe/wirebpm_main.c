#include "wirebpm_main.h"


int  WirebpmMain(int argc, char *argv[],
                 const char *def_app_name, const char *def_app_class,
                 pzframe_gui_t *gui, pzframe_gui_dscr_t *gkd)
{
  wirebpm_gui_t      *my_gui = pzframe2wirebpm_gui     (gui);
  wirebpm_gui_dscr_t *my_gkd = pzframe2wirebpm_gui_dscr(gkd);

  psp_paramdescr_t   *dcnv_table  = WirebpmDataGetDcnvTable();
  char               *buf_to_free = NULL;
  psp_paramdescr_t   *gui_table   = NULL;

    if ((gui_table  = WirebpmGuiCreateText2LookTable (my_gkd)) == NULL)
        return 1;

    return PzframeMain(argc, argv,
                       def_app_name, def_app_class,
                       gui, gkd,
                       dcnv_table, &(my_gui->b.dcnv),
                       gui_table,  &(my_gui->look));

    /* Note: dcnv_table, buf_to_free, gui_table should be free()d, but
       there's no big reason to do this just before exit() */
}
