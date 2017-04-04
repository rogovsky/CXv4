#include "fastadc_data.h"
#include "fastadc_gui.h"
#include "fastadc_main.h"

#include "DEVTYPE_LCASE_gui.h"


int main(int argc, char *argv[])
{
  fastadc_gui_t       gui;
  pzframe_gui_dscr_t *gkd = DEVTYPE_LCASE_get_gui_dscr();

    FastadcGuiInit(&gui, pzframe2fastadc_gui_dscr(gkd));
    return FastadcMain(argc, argv,
                       "DEVTYPE_LCASE", "DEVTYPE_UCASE",
                       &(gui.g), gkd);
}
