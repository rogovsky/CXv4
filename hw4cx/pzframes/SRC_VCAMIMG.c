#include "vcamimg_data.h"
#include "vcamimg_gui.h"
#include "vcamimg_main.h"

#include "DEVTYPE_LCASE_gui.h"


int main(int argc, char *argv[])
{
  vcamimg_gui_t       gui;
  pzframe_gui_dscr_t *gkd = DEVTYPE_LCASE_get_gui_dscr();

    VcamimgGuiInit(&gui, pzframe2vcamimg_gui_dscr(gkd));
    return VcamimgMain(argc, argv,
                       "DEVTYPE_LCASE", "DEVTYPE_UCASE",
                       &(gui.g), gkd);
}
