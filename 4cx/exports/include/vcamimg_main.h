#ifndef __VCAMIMG_MAIN_H
#define __VCAMIMG_MAIN_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "pzframe_main.h"
#include "vcamimg_gui.h"


int  VcamimgMain(int argc, char *argv[],
                 const char *def_app_name, const char *def_app_class,
                 pzframe_gui_t *gui, pzframe_gui_dscr_t *gkd);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __VCAMIMG_MAIN_H */
