#ifndef __MOTIFKNOBS_HISTPLOT_H
#define __MOTIFKNOBS_HISTPLOT_H


#include <X11/Intrinsic.h>

#include "datatree.h"

#include "Xh.h"


enum {MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX = XH_NUM_DISTINCT_LINE_COLORS * 2};

enum
{
    MOTIFKNOBS_HISTPLOT_FLAG_FIXED        = 1 << 0,
    MOTIFKNOBS_HISTPLOT_FLAG_WITH_CLOSE_B = 1 << 1,
    MOTIFKNOBS_HISTPLOT_FLAG_NO_SAVE_BUTT = 1 << 2,
    MOTIFKNOBS_HISTPLOT_FLAG_NO_MODE_SWCH = 1 << 3,
    MOTIFKNOBS_HISTPLOT_FLAG_NO_XSCL_SWCH = 1 << 4,   // "XSCL" -- X-SCaLe
    MOTIFKNOBS_HISTPLOT_FLAG_HORZ_CTLS    = 1 << 5,
    MOTIFKNOBS_HISTPLOT_FLAG_VIEW_ONLY    = 1 << 6,
};

enum
{
    MOTIFKNOBS_HISTPLOT_MODE_LINE = 0,
    MOTIFKNOBS_HISTPLOT_MODE_WIDE = 1,
    MOTIFKNOBS_HISTPLOT_MODE_DOTS = 2,
    MOTIFKNOBS_HISTPLOT_MODE_BLOT = 3,
};

enum
{
    MOTIFKNOBS_HISTPLOT_REASON_CLOSE = 1,
    MOTIFKNOBS_HISTPLOT_REASON_SAVE  = 2,
    MOTIFKNOBS_HISTPLOT_REASON_MB3   = 3,
};

struct MotifKnobs_histplot_t_struct;

typedef void (*MotifKnobs_histplot_evproc_t)(struct MotifKnobs_histplot_t_struct *hp,
                                             void                  *privptr,
                                             int                    reason,
                                             void                  *info_ptr);

typedef struct MotifKnobs_histplot_t_struct
{
    int          cyclesize_us;
    MotifKnobs_histplot_evproc_t  evproc;
    void                         *evproc_privptr;

    CxWidget     form;
    CxWidget     axis;
    CxWidget     graph;
    CxWidget     horzbar;
    CxWidget     w_l;
    CxWidget     w_r;

    int          fixed;
    
    int          rows_used;
    CxWidget     ok;
    CxWidget     save;
    CxWidget     grid;
    CxWidget     label   [MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX];
    CxWidget     val_dpy [MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX];
    CxWidget     b_up    [MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX];
    CxWidget     b_dn    [MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX];
    CxWidget     b_rm    [MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX];
    DataKnob     target  [MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX];
    knobstate_t  curstate[MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX];
    int          show    [MOTIFKNOBS_HISTPLOT_MAX_LINES_PER_BOX];
    CxPixel      deffg;
    CxPixel      defbg;

    GC           bkgdGC;
    GC           axisGC;
    GC           gridGC;
    GC           reprGC;
    GC           chanGC [2][XH_NUM_DISTINCT_LINE_COLORS];
    XFontStruct *axis_finfo;
    XFontStruct *chan_finfo[XH_NUM_DISTINCT_LINE_COLORS];
    int          mode;
    int          x_scale;
    
    int          m_lft;
    int          m_rgt;
    int          m_top;
    int          m_bot;

    int          ignore_axis_expose;
    int          ignore_graph_expose;

    int          horz_maximum;
    int          horz_offset;
    int          o_horzbar_maximum;
    int          o_horzbar_slidersize;

    // (Values-from-history viewing)-related stuff
    int             view_only;
    int             x_index;
    CxWidget        time_dpy;
    struct timeval *timestamps_ring;
    int             timestamps_ring_used;
} MotifKnobs_histplot_t;

typedef struct
{
    DataKnob  k;
    XEvent   *event;
    Boolean  *continue_to_dispatch;
} MotifKnobs_histplot_mb3_evinfo_t;


int  MotifKnobs_CreateHistplot(MotifKnobs_histplot_t *hp,
                               Widget parent,
                               int def_w, int def_h,
                               int flags,
                               int cyclesize_us,
                               MotifKnobs_histplot_evproc_t  evproc,
                               void                         *evproc_privptr);
void MotifKnobs_AddToHistplot (MotifKnobs_histplot_t *hp, DataKnob k);
void MotifKnobs_UpdateHistplotGraph(MotifKnobs_histplot_t *hp);
void MotifKnobs_UpdateHistplotProps(MotifKnobs_histplot_t *hp);

int  MotifKnobs_SaveHistplotData   (MotifKnobs_histplot_t *hp,
                                    const char *filename,
                                    const char *comment);

#endif /* __MOTIFKNOBS_HISTPLOT_H */
