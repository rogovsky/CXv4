#ifndef __PZFRAME_KNOBPLUGIN_H
#define __PZFRAME_KNOBPLUGIN_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "datatreeP.h"
#include "pzframe_gui.h"


struct _pzframe_knobplugin_t_struct;

typedef int  (*pzframe_knobplugin_realize_t)
    (struct _pzframe_knobplugin_t_struct *kpn,
     DataKnob                             k);

typedef void (*pzframe_knobplugin_evproc_t)(pzframe_data_t *pfr,
                                            int   reason,
                                            int   info_int,
                                            void *privptr);

typedef struct
{
    pzframe_knobplugin_evproc_t  cb;
    void                        *privptr;
} pzframe_knobplugin_cb_info_t;


typedef struct _pzframe_knobplugin_t_struct
{
    pzframe_gui_t                *g;

    // Callbacks
    int                           low_level_registered;
    pzframe_knobplugin_cb_info_t *cb_list;
    int                           cb_list_allocd;
} pzframe_knobplugin_t;


int      PzframeKnobpluginDoCreate(DataKnob k, CxWidget parent,
                                   pzframe_knobplugin_realize_t  kp_realize,
                                   pzframe_gui_t *gui, pzframe_gui_dscr_t *gkd,
                                   psp_paramdescr_t *table2, void *rec2,
                                   psp_paramdescr_t *table4, void *rec4,
                                   psp_paramdescr_t *table7, void *rec7);



CxWidget PzframeKnobpluginRedRectWidget(CxWidget parent);

int      PzframeKnobpluginHandleCmd_m(DataKnob k, const char *cmd, int info_int);


/* Public services */
pzframe_data_t
    *PzframeKnobpluginRegisterCB(DataKnob                     k,
                                 const char                  *rqd_type_name,
                                 pzframe_knobplugin_evproc_t  cb,
                                 void                        *privptr,
                                 const char                 **name_p);
pzframe_gui_t
    *PzframeKnobpluginGetGui    (DataKnob                     k,
                                 const char                  *rqd_type_name);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __PZFRAME_KNOBPLUGIN_H */
