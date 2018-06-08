#ifndef __WIREBPM_KNOBPLUGIN_H
#define __WIREBPM_KNOBPLUGIN_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "pzframe_knobplugin.h"
#include "wirebpm_gui.h"


typedef struct _wirebpm_knobplugin_t_struct
{
    pzframe_knobplugin_t  kp;
    wirebpm_gui_t         g;
} wirebpm_knobplugin_t;

static inline wirebpm_knobplugin_t *wirebpm_gui2knobplugin(wirebpm_gui_t *g)
{
    return
        (wirebpm_knobplugin_t*)
        (((char *)g) - offsetof(wirebpm_knobplugin_t, g));
}


int WirebpmKnobpluginDoCreate(DataKnob k, CxWidget parent,
                              pzframe_knobplugin_realize_t  kp_realize,
                              pzframe_gui_dscr_t *gkd,
                              psp_paramdescr_t *privrec_tbl, void *privrec);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __WIREBPM_KNOBPLUGIN_H */
