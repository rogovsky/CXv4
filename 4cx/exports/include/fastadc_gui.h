#ifndef __FASTADC_GUI_H
#define __FASTADC_GUI_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "plotdata.h"
#include "Xh_plot.h"

#include "fastadc_data.h"
#include "pzframe_gui.h"


enum
{
    FASTADC_GUI_2_REPERS = 2,
};


typedef enum
{
    FASTADC_GUI_CTL_COMMONS,
    FASTADC_GUI_CTL_X_CMPR,
    FASTADC_GUI_CTL_LINE_LABEL,
    FASTADC_GUI_CTL_LINE_MAGN,
    FASTADC_GUI_CTL_LINE_INVP,
    FASTADC_GUI_CTL_REPER_VALUE,
    FASTADC_GUI_CTL_REPER_ONOFF,
    FASTADC_GUI_CTL_REPER_TIME,
    FASTADC_GUI_CTL_REPER_UNITS,
} fastadc_gui_ctl_kind_t;

enum
{
    FASTADC_REASON_REPER_CHANGE = 1000,
};


/********************************************************************/
struct _fastadc_gui_t_struct;
/********************************************************************/

typedef Widget (*fastadc_gui_mkctls_t)   (struct _pzframe_gui_t_struct *gui,
                                          fastadc_type_dscr_t     *atd,
                                          Widget                   parent,
                                          pzframe_gui_mkstdctl_t   mkstdctl,
                                          pzframe_gui_mkparknob_t  mkparknob);

/********************************************************************/

typedef struct
{
    pzframe_gui_dscr_t    gkd;
    fastadc_type_dscr_t  *atd;

    XhPlot_plotdraw_t     plotdraw;

    int                   def_plot_w;
    int                   min_plot_w;
    int                   max_plot_w;
    int                   def_plot_h;
    int                   min_plot_h;
    int                   max_plot_h;

    int                   cpanel_loc;
    fastadc_gui_mkctls_t  mkctls;
} fastadc_gui_dscr_t;

static inline fastadc_gui_dscr_t *pzframe2fastadc_gui_dscr(pzframe_gui_dscr_t *gkd)
{
    return (gkd == NULL)? NULL
        : (fastadc_gui_dscr_t *)(gkd); /*!!! Should use OFFSETOF() -- (uint8*)gkd-OFFSETOF(fastadc_gui_dscr_t, gkd) */
}

/********************************************************************/

typedef struct
{
    int     width;
    int     height;
    int     black;
    int     noscrollbar;
    int     nofold;
} fastadc_gui_look_t;

typedef struct _fastadc_gui_t_struct
{
    pzframe_gui_t           g;
    fastadc_data_t          a;
    fastadc_gui_dscr_t     *d;

    fastadc_gui_look_t      look;

    XhPlot                  plot;

    int                     rpr_on   [XH_PLOT_NUM_REPERS];
    int                     rpr_at   [XH_PLOT_NUM_REPERS];
    int                     rpr_press[XH_PLOT_NUM_REPERS];
    DataKnob                rpr_kswch[XH_PLOT_NUM_REPERS];
    Widget                  rpr_time [XH_PLOT_NUM_REPERS];
    Widget                  rpr_val  [XH_PLOT_NUM_REPERS][FASTADC_MAX_LINES];

    DataKnob                label_ks [FASTADC_MAX_LINES];

    /* Param-passing-duplets */
    pzframe_gui_dpl_t       Line_prm [FASTADC_MAX_LINES];

    pzframe_gui_dpl_t       RprSwch_prm[XH_PLOT_NUM_REPERS];
} fastadc_gui_t;

static inline fastadc_gui_t *pzframe2fastadc_gui(pzframe_gui_t *pzframe_gui)
{
    return (pzframe_gui == NULL)? NULL
        : (fastadc_gui_t *)(pzframe_gui); /*!!! Should use OFFSETOF() -- (uint8*)pzframe_gui-OFFSETOF(fastadc_gui_t, g) */
}

//////////////////////////////////////////////////////////////////////    

void  FastadcGuiFillStdDscr(fastadc_gui_dscr_t *gkd, fastadc_type_dscr_t *atd);

void  FastadcGuiInit       (fastadc_gui_t *gui, fastadc_gui_dscr_t *gkd);
int   FastadcGuiRealize    (fastadc_gui_t *gui, Widget parent,
                            cda_context_t  present_cid,
                            const char    *base);

void  FastadcGuiRenewPlot  (fastadc_gui_t *gui, int max_numpts, int info_changed);
void  FastadcGuiSetReper   (fastadc_gui_t *gui, int nr, int x);

psp_paramdescr_t *FastadcGuiCreateText2LookTable(fastadc_gui_dscr_t *gkd);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __FASTADC_GUI_H */
