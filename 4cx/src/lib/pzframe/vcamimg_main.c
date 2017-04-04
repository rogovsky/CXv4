#include "vcamimg_main.h"


int  VcamimgMain(int argc, char *argv[],
                 const char *def_app_name, const char *def_app_class,
                 pzframe_gui_t *gui, pzframe_gui_dscr_t *gkd)
{
  vcamimg_gui_t      *my_gui = pzframe2vcamimg_gui     (gui);
  vcamimg_gui_dscr_t *my_gkd = pzframe2vcamimg_gui_dscr(gkd);

  char               *buf_to_free = NULL;
  psp_paramdescr_t   *gui_table   = NULL;

    if ((gui_table  = VcamimgGuiCreateText2LookTable (my_gkd)) == NULL)
        return 1;

    return PzframeMain(argc, argv,
                       def_app_name, def_app_class,
                       gui, gkd,
                       NULL/*dcnv_table*/, NULL,
                       gui_table,  &(my_gui->look));

    /* Note: dcnv_table, buf_to_free, gui_table should be free()d, but
       there's no big reason to do this just before exit() */
}
