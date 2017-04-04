#include "fastadc_data.h"
#include "fastadc_gui.h"
#include "fastadc_main.h"

#include "two812ch_gui.h"


int main(int argc, char *argv[])
{
  fastadc_gui_t       gui;
  pzframe_gui_dscr_t *gkd = two812ch_get_gui_dscr();

    FastadcGuiInit(&gui, pzframe2fastadc_gui_dscr(gkd));
    return FastadcMain(argc, argv,
                       "two812ch", "TWO812CH",
                       &(gui.g), gkd);
}
