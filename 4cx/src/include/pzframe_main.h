#ifndef __PZFRAME_MAIN_H
#define __PZFRAME_MAIN_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "pzframe_gui.h"


#define PZFRAME_MAIN_CMD_LOOP     "pzframeLoop"
#define PZFRAME_MAIN_CMD_ONCE     "pzframeOnce"
#define PZFRAME_MAIN_CMD_SET_GOOD "pzframeSetGood"
#define PZFRAME_MAIN_CMD_RST_GOOD "pzframeRstGood"
#define PZFRAME_MAIN_CMD_SET_BKGD "pzframeSetBkgd"
#define PZFRAME_MAIN_CMD_RST_BKGD "pzframeRstBkgd"


int  PzframeMain(int argc, char *argv[],
                 const char *def_app_name, const char *def_app_class,
                 pzframe_gui_t *gui, pzframe_gui_dscr_t *gkd,
                 psp_paramdescr_t *table2, void *rec2,
                 psp_paramdescr_t *table4, void *rec4);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __PZFRAME_MAIN_H */
