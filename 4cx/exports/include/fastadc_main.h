#ifndef __FASTADC_MAIN_H
#define __FASTADC_MAIN_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "pzframe_main.h"
#include "fastadc_gui.h"


int  FastadcMain(int argc, char *argv[],
                 const char *def_app_name, const char *def_app_class,
                 pzframe_gui_t *gui, pzframe_gui_dscr_t *gkd);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __FASTADC_MAIN_H */
