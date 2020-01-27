#ifndef __XH_PLOT_H
#define __XH_PLOT_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <X11/Intrinsic.h>

#include "cx_common_types.h"
#include "plotdata.h"


enum
{
    XH_PLOT_Y_OF_X = 1,
    XH_PLOT_X_OF_Y = 2,
    XH_PLOT_Y_OF_T = 3,
    XH_PLOT_XY     = 4,
};

enum
{
    XH_PLOT_CPANEL_ABSENT    = 0,
    XH_PLOT_CPANEL_AT_LEFT   = 1,
    XH_PLOT_CPANEL_AT_RIGHT  = 2,
    XH_PLOT_CPANEL_AT_TOP    = 3,
    XH_PLOT_CPANEL_AT_BOTTOM = 4,
    XH_PLOT_CPANEL_MASK      = 7,

    XH_PLOT_NO_CPANEL_FOLD_MASK = 1 << 16,
    XH_PLOT_NO_SCROLLBAR_MASK   = 1 << 17,

    XH_PLOT_WIDE_MASK           = 1 << 18,
};

enum
{
    XH_PLOT_NUM_REPERS = 3,
    XH_PLOT_NUM_LEVELS = 3,
};


typedef struct _XhPlot_info_t_struct *XhPlot;

typedef double       (*XhPlot_raw2pvl_t) (void *privptr, int nl, int raw);
typedef double       (*XhPlot_pvl2dsp_t) (void *privptr, int nl, double pvl);
typedef int          (*XhPlot_x2xs_t)    (void *privptr, int x);
typedef const char * (*XhPlot_xs_t)      (void *privptr);

typedef void         (*XhPlot_plotdraw_t)(void *privptr, int nl, int age,
                                          int magn, GC gc);
typedef void         (*XhPlot_set_rpr_t) (void *privptr, int nr, int x);

XhPlot    XhCreatePlot(Widget parent, int type, int maxplots,
                       void              *privptr,
                       XhPlot_raw2pvl_t   raw2pvl,
                       XhPlot_pvl2dsp_t   pvl2dsp,
                       XhPlot_x2xs_t      x2xs,
                       XhPlot_xs_t        xs,
                       XhPlot_plotdraw_t  draw_one,
                       XhPlot_set_rpr_t   set_rpr,
                       int init_w, int init_h, int options, int black,
                       ...);

void      XhPlotCalcMargins(XhPlot plot);
void      XhPlotUpdate     (XhPlot plot, int with_axis);
int       XhPlotSetMaxPts  (XhPlot plot, int maxpts);
void      XhPlotSetNumSaved(XhPlot plot, int num_saved);
void      XhPlotSetCmpr    (XhPlot plot, int cmpr);
void      XhPlotSetReper   (XhPlot plot, int nr, int x);
void      XhPlotSetLineMode(XhPlot plot, int mode);

int       XhPlotOneSetData(XhPlot plot, int n, plotdata_t *data);
int       XhPlotOneSetShow(XhPlot plot, int n, int show);
int       XhPlotOneSetMagn(XhPlot plot, int n, int magn);
int       XhPlotOneSetInvp(XhPlot plot, int n, int invp);
int       XhPlotOneSetStrs(XhPlot plot, int n,
                           const char *descr,
                           const char *dpyfmt,
                           const char *units);

void      XhPlotOneDraw (XhPlot plot, plotdata_t *data, int magn, GC gc);

void     *XhPlotPrivptr (XhPlot plot);
Widget    XhWidgetOfPlot(XhPlot plot);
Widget    XhAngleOfPlot (XhPlot plot);
Widget    XhCpanelOfPlot(XhPlot plot);
void      XhPlotSetCpanelState(XhPlot plot, int state);

void      XhPlotSetBG   (XhPlot plot, CxPixel bg);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __XH_PLOT_H */
