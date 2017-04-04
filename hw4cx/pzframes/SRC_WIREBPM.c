#include "wirebpm_data.h"
#include "wirebpm_gui.h"
#include "wirebpm_main.h"

#include "DEVTYPE_LCASE_gui.h"


int main(int argc, char *argv[])
{
  wirebpm_gui_t       gui;
  pzframe_gui_dscr_t *gkd = DEVTYPE_LCASE_get_gui_dscr();

    WirebpmGuiInit(&gui, pzframe2wirebpm_gui_dscr(gkd));
    return WirebpmMain(argc, argv,
                       "DEVTYPE_LCASE", "DEVTYPE_UCASE",
                       &(gui.g), gkd);
}
