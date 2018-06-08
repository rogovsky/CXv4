#ifndef __FASTADC_KNOBPLUGIN_H
#define __FASTADC_KNOBPLUGIN_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "pzframe_knobplugin.h"
#include "fastadc_gui.h"


typedef struct _fastadc_knobplugin_t_struct
{
    pzframe_knobplugin_t  kp;
    fastadc_gui_t         g;
} fastadc_knobplugin_t;

static inline fastadc_knobplugin_t *fastadc_gui2knobplugin(fastadc_gui_t *g)
{
    return
        (fastadc_knobplugin_t*)
        (((char *)g) - offsetof(fastadc_knobplugin_t, g));
}


int FastadcKnobpluginDoCreate(DataKnob k, CxWidget parent,
                              pzframe_knobplugin_realize_t  kp_realize,
                              pzframe_gui_dscr_t *gkd,
                              psp_paramdescr_t *privrec_tbl, void *privrec);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __FASTADC_KNOBPLUGIN_H */
