#ifndef __WIREBPM_GUI_H
#define __WIREBPM_GUI_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "wirebpm_data.h"
#include "pzframe_gui.h"


typedef enum
{
    WIREBPM_GUI_CTL_COMMONS,
    WIREBPM_GUI_CTL_MAGN,
} wirebpm_gui_ctl_kind_t;

enum
{
    WIREBPM_KIND_HIST,
    WIREBPM_KIND_ESPECTR,
};

enum
{
    MAX_CELLS_PER_UNIT = 32
};


/********************************************************************/
struct _wirebpm_gui_t_struct;
/********************************************************************/

typedef Widget (*wirebpm_gui_mkctls_t)   (struct _pzframe_gui_t_struct *gui,
                                          wirebpm_type_dscr_t     *wtd,
                                          Widget                   parent,
                                          pzframe_gui_mkstdctl_t   mkstdctl,
                                          pzframe_gui_mkparknob_t  mkparknob);

/********************************************************************/

typedef struct
{
    pzframe_gui_dscr_t    gkd;
    wirebpm_type_dscr_t  *wtd;

    int                   cpanel_loc;
    wirebpm_gui_mkctls_t  mkctls;
} wirebpm_gui_dscr_t;

static inline wirebpm_gui_dscr_t *pzframe2wirebpm_gui_dscr(pzframe_gui_dscr_t *gkd)
{
    return (gkd == NULL)? NULL
        : (wirebpm_gui_dscr_t *)(gkd); /*!!! Should use OFFSETOF() -- (uint8*)gkd-OFFSETOF(wirebpm_gui_dscr_t, gkd) */
}

/********************************************************************/

typedef struct
{
    int     kind;
    int     magn;
} wirebpm_gui_look_t;

typedef void (*wirebpm_gui_do_prep_t)(struct _wirebpm_gui_t_struct *gui,
                                      Widget parent);
typedef void (*wirebpm_gui_do_tune_t)(struct _wirebpm_gui_t_struct *gui,
                                      Widget parent);
typedef void (*wirebpm_gui_do_draw_t)(struct _wirebpm_gui_t_struct *gui);

typedef struct
{
    wirebpm_gui_do_tune_t  do_prep;
    wirebpm_gui_do_tune_t  do_tune;
    wirebpm_gui_do_draw_t  do_draw;
} wirebpm_gui_vmt_t;

typedef struct _wirebpm_gui_t_struct
{
    pzframe_gui_t           g;
    wirebpm_data_t          b;
    wirebpm_gui_dscr_t     *d;

    wirebpm_gui_vmt_t      *wiregui_vmt_p;

    wirebpm_gui_look_t      look;

    Widget                  container;
    Widget                  cpanel;

    int                     wid;
    int                     hei;
    Widget                  hgrm;

    GC                      bkgdGC;
    GC                      axisGC;
    GC                      reprGC;
    GC                      prevGC;
    GC                      dat1GC;
    GC                      dat2GC;
    GC                      outlGC;
    GC                      textGC;
    XFontStruct            *text_finfo;

    int32                   cur_data[MAX_CELLS_PER_UNIT];

    int                     b_gmin;
    int                     b_gmax;
} wirebpm_gui_t;

static inline wirebpm_gui_t *pzframe2wirebpm_gui(pzframe_gui_t *pzframe_gui)
{
    return (pzframe_gui == NULL)? NULL
        : (wirebpm_gui_t *)(pzframe_gui); /*!!! Should use OFFSETOF() -- (uint8*)pzframe_gui-OFFSETOF(wirebpm_gui_t, g) */
}

//////////////////////////////////////////////////////////////////////    

void  WirebpmGuiFillStdDscr(wirebpm_gui_dscr_t *gkd, wirebpm_type_dscr_t *wtd);

void  WirebpmGuiInit       (wirebpm_gui_t *gui, wirebpm_gui_dscr_t *gkd);
int   WirebpmGuiRealize    (wirebpm_gui_t *gui, Widget parent,
                            cda_context_t  present_cid,
                            const char    *base);

void  WirebpmGuiRenewView  (wirebpm_gui_t *gui, int info_changed);

psp_paramdescr_t *WirebpmGuiCreateText2LookTable(wirebpm_gui_dscr_t *gkd);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __WIREBPM_GUI_H */
