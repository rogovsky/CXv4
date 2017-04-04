#ifndef __XH_VIEWPORT_H
#define __XH_VIEWPORT_H


#include <Xm/Xm.h>

#include "Xh.h"


enum
{
    XH_VIEWPORT_HORZBAR = 1 << 0,
    XH_VIEWPORT_VERTBAR = 1 << 1,
    XH_VIEWPORT_CPANEL  = 1 << 2,
    XH_VIEWPORT_CPANEL_LOC_MASK  = 3 << 3,
    XH_VIEWPORT_CPANEL_AT_LEFT   = 0 << 3,
    XH_VIEWPORT_CPANEL_AT_RIGHT  = 1 << 3,
    XH_VIEWPORT_CPANEL_AT_TOP    = 2 << 3,
    XH_VIEWPORT_CPANEL_AT_BOTTOM = 3 << 3,
    XH_VIEWPORT_NOFOLD  = 1 << 5,

    XH_VIEWPORT_VIEW    = 1 << 10,
    XH_VIEWPORT_AXIS    = 1 << 11,
    XH_VIEWPORT_STATS   = 1 << 12,
    XH_VIEWPORT_ALL     = ~(0 | XH_VIEWPORT_NOFOLD) /* In fact, ~0 should be OK */
};

enum
{
    XH_VIEWPORT_MSEV_ENTER   = 1,
    XH_VIEWPORT_MSEV_LEAVE   = 2,
    XH_VIEWPORT_MSEV_MOVE    = 3,
    XH_VIEWPORT_MSEV_PRESS   = 4,
    XH_VIEWPORT_MSEV_RELEASE = 5,

    XH_VIEWPORT_MSEV_MASK_SHIFT = 1 << 0,
    XH_VIEWPORT_MSEV_MASK_CTRL  = 1 << 1,
    XH_VIEWPORT_MSEV_MASK_ALT   = 1 << 2,
};

struct _Xh_viewport_t_struct;

typedef void (*Xh_viewport_draw_t)(struct _Xh_viewport_t_struct *vp, int parts);
typedef void (*Xh_viewport_msev_t)(struct _Xh_viewport_t_struct *vp,
                                   int ev_type, int mod_mask,
                                   int bn, int x, int y);

typedef struct _Xh_viewport_t_struct
{
    void               *privptr;
    Xh_viewport_draw_t  draw;
    Xh_viewport_msev_t  msev;

    /* Widgets */
    Widget              container;
    Widget              sidepanel;
    Widget              cpanel;
    Widget              gframe;
    Widget              gform;
    Widget              btn_cpanel_off;
    Widget              btn_cpanel_on;
    Widget              angle;
    Widget              view;
    Widget              axis;
    Widget              horzbar;
    Widget              vertbar;

    /* Geometry */
    int                 m_lft;
    int                 m_rgt;
    int                 m_top;
    int                 m_bot;

    /* Scrollbars' management */
    int                 horz_maximum; /* Is set bu user */
    int                 horz_offset;  /* Is set by viewport */
    int                 o_horzbar_maximum;
    int                 o_horzbar_slidersize;

    /* Internal-operation */
    GC                  bkgdGC;
    int                 ignore_view_expose;
    int                 ignore_axis_expose;
} Xh_viewport_t;


int  XhViewportCreate    (Xh_viewport_t *vp, Widget parent,
                          void *privptr,
                          Xh_viewport_draw_t draw, Xh_viewport_msev_t msev,
                          int parts, 
                          int init_w, int init_h, int bg_idx,
                          int m_lft, int m_rgt, int m_top, int m_bot);

void XhViewportSetMargins(Xh_viewport_t *vp,
                          int m_lft, int m_rgt, int m_top, int m_bot);

int  XhViewportSetHorzbarParams(Xh_viewport_t *vp);

void XhViewportUpdate    (Xh_viewport_t *vp, int parts);

void XhViewportSetCpanelState(Xh_viewport_t *vp, int state);

void XhViewportSetBG     (Xh_viewport_t *vp, CxPixel bg);


#endif /* __XH_VIEWPORT_H */
