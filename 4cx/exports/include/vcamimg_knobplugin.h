#ifndef __VCAMIMG_KNOBPLUGIN_H
#define __VCAMIMG_KNOBPLUGIN_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "pzframe_knobplugin.h"
#include "vcamimg_gui.h"


typedef struct _vcamimg_knobplugin_t_struct
{
    pzframe_knobplugin_t  kp;
    vcamimg_gui_t         g;
} vcamimg_knobplugin_t;

static inline vcamimg_knobplugin_t *vcamimg_gui2knobplugin(vcamimg_gui_t *g)
{
    return
        (vcamimg_knobplugin_t*)
        (((char *)g) - offsetof(vcamimg_knobplugin_t, g));
}


int VcamimgKnobpluginDoCreate(DataKnob k, CxWidget parent,
                              pzframe_knobplugin_realize_t  kp_realize,
                              pzframe_gui_dscr_t *gkd,
                              psp_paramdescr_t *privrec_tbl, void *privrec);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __VCAMIMG_KNOBPLUGIN_H */
