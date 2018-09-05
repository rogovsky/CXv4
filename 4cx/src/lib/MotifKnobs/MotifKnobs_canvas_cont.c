#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/DrawingA.h>
#include "InputOnly.h"

#include <Xm/Form.h>

#include "misc_macros.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_canvas_cont.h"


//////////////////////////////////////////////////////////////////////

enum
{
    SHAPE_RECTANGLE = 0,
    SHAPE_ELLIPSE,
    SHAPE_POLYGON,
    SHAPE_POLYLINE,
    SHAPE_PIPE,
    SHAPE_STRING
};

enum
{
    FONTMOD_NORMAL     = 0,
    FONTMOD_ITALIC     = 1,
    FONTMOD_BOLD       = 2,
    FONTMOD_BOLDITALIC = 3
};

enum
{
    FONTSIZE_TINY   = -2, //  8
    FONTSIZE_SMALL  = -1, // 10
    FONTSIZE_NORMAL = 0,  // 12 (or 14, on large screens)
    FONTSIZE_LARGE  = 1,  // 18
    FONTSIZE_HUGE   = 2,  // 24
    
    FONTSIZE_MIN = FONTSIZE_TINY
};

enum
{
    TEXTALIGN_LEFT   = 0,
    TEXTALIGN_CENTER = 1,
    TEXTALIGN_RIGHT  = 2
};

static char *fontmod_strs[4] = {"medium-r", "medium-i", "bold-r", "bold-i"};
static int   fontsize_sizes[5] = {8, 10, 12, 18, 24};
static unsigned char  textalign_vals[3] = {XmALIGNMENT_BEGINNING, XmALIGNMENT_CENTER, XmALIGNMENT_END};

typedef struct
{
    int    width;
    int    height;
    
    char   sh_color[20];
    int    thickness;
    int    rounded;
    
    int    bd_width;
    char   bd_color[20];
    
    char   tx_color[20];
    char   tx_pos  [100];
    int    tx_align;
    int    fn_mod;
    int    fn_size;
    
    char  *points_list;
} decoropts_t;
typedef struct
{
    decoropts_t opts;

    int         shape;
    int         filled;

    CxPixel     sh_pixel;
    GC          sh_GC;
    
    int         bd_width;
    CxPixel     bd_pixel;
    GC          bd_GC;

    int         tx_present;
    XmString    tx_s;
    CxPixel     tx_pixel;
    XmFontList  tx_fl;
    GC          tx_GC;
    Dimension   tx_wid;
    Dimension   tx_hei;
    char        tx_align;
    int         tx_h_pos;
    int         tx_v_pos;
    int         tx_h_ofs;
    int         tx_v_ofs;
    
    XPoint     *points;
    int         numpoints;
} decor_privrec_t;

static psp_lkp_t tx_align_lkp[] =
{
    {"left",   TEXTALIGN_LEFT},
    {"center", TEXTALIGN_CENTER},
    {"right",  TEXTALIGN_RIGHT},
    {NULL, 0}
};

static psp_lkp_t fn_mod_lkp[] =
{
    {"normal",     FONTMOD_NORMAL},
    {"italic",     FONTMOD_ITALIC},
    {"bold",       FONTMOD_BOLD},
    {"bolditalic", FONTMOD_BOLDITALIC},
    {NULL, 0}
};

static psp_lkp_t fn_size_lkp[] =
{
    {"tiny",   FONTSIZE_TINY},
    {"small",  FONTSIZE_SMALL},
    {"normal", FONTSIZE_NORMAL},
    {"large",  FONTSIZE_LARGE},
    {"huge",   FONTSIZE_HUGE},
    {NULL, 0}
};

static psp_paramdescr_t text2decoropts[] =
{
    PSP_P_INT    ("width",       decor_privrec_t, opts.width,     1, 0, 0),
    PSP_P_INT    ("height",      decor_privrec_t, opts.height,    1, 0, 0),

    PSP_P_STRING ("color",       decor_privrec_t, opts.sh_color,  ""),
    PSP_P_INT    ("thickness",   decor_privrec_t, opts.thickness, 0, 0, 0),
    PSP_P_FLAG   ("rounded",     decor_privrec_t, opts.rounded,   1, 0),
    
    PSP_P_INT    ("border",      decor_privrec_t, opts.bd_width,  0, 0, 0),
    PSP_P_STRING ("bordercolor", decor_privrec_t, opts.bd_color,  ""),

    PSP_P_STRING ("textcolor",   decor_privrec_t, opts.tx_color,  ""),
    PSP_P_STRING ("textpos",     decor_privrec_t, opts.tx_pos,    ""),
    PSP_P_LOOKUP ("textalign",   decor_privrec_t, opts.tx_align,  TEXTALIGN_LEFT,  tx_align_lkp),
    PSP_P_LOOKUP ("fontmod",     decor_privrec_t, opts.fn_mod,    FONTMOD_NORMAL,  fn_mod_lkp),
    PSP_P_LOOKUP ("fontsize",    decor_privrec_t, opts.fn_size,   FONTSIZE_NORMAL, fn_size_lkp),

    PSP_P_MSTRING("points",      decor_privrec_t, opts.points_list,  "", 10000),
    
    PSP_P_END()
};

static void DrawDecor(DataKnob k, int on_parent)
{
  decor_privrec_t *me = (decor_privrec_t *)(k->privptr);

  int              x0;
  int              y0;
  Widget           target;
  Display         *dpy;
  Window           win;
  
  Position         x_w;
  Position         y_w;
  Dimension        wid;
  Dimension        hei;
  XmDirection      dir;

  int              x;
  int              y;
    
    XtVaGetValues(CNCRTZE(k->w),
                  XmNx,               &x_w,
                  XmNy,               &y_w,
                  XmNwidth,           &wid,
                  XmNheight,          &hei,
                  XmNlayoutDirection, &dir,
                  NULL);

    
    target = CNCRTZE(k->w);
    x0 = 0;
    y0 = 0;
    if (on_parent)
    {
        target = XtParent(target);
        x0 = x_w;
        y0 = y_w;
    }
    dpy = XtDisplay(target);
    win = XtWindow (target);
    
    switch (me->shape)
    {
        case SHAPE_RECTANGLE:
            if (me->filled)
            {
                XFillRectangle(dpy, win, me->sh_GC,
                               x0, y0,
                               wid, hei);
            }
            else
            {
                XDrawRectangle(dpy, win, me->sh_GC,
                               x0, y0,
                               wid-1, hei-1);
            }
            if (me->bd_width > 0)
                XDrawRectangle(dpy, win, me->bd_GC,
                               x0, y0,
                               wid-1, hei-1);
            break;

        case SHAPE_ELLIPSE:
            if (me->filled)
            {
                XFillArc(dpy, win, me->sh_GC,
                         x0, y0,
                         wid, hei,
                         0, 360*64);
            }
            else
            {
                XDrawArc(dpy, win, me->sh_GC,
                         x0, y0,
                         wid-1, hei-1,
                         0, 360*64);
            }
            if (me->bd_width > 0)
                XDrawArc(dpy, win, me->bd_GC,
                         x0, y0,
                         wid-1, hei-1,
                         0, 360*64);
            break;

        case SHAPE_POLYGON:
            if (me->numpoints == 0) break;
            me->points[0].x += x0;
            me->points[0].y += y0;

            if (me->filled)
            {
                XFillPolygon(dpy, win, me->sh_GC,
                             me->points, me->numpoints,
                             Complex, CoordModePrevious);
            }
            else
            {
                XDrawLines(dpy, win, me->sh_GC,
                           me->points, me->numpoints + 1,
                           CoordModePrevious);
            }
            if (me->bd_width > 0)
                XDrawLines(dpy, win, me->bd_GC,
                           me->points, me->numpoints + 1,
                           CoordModePrevious);

            me->points[0].x -= x0;
            me->points[0].y -= y0;
            break;

        case SHAPE_POLYLINE:
        case SHAPE_PIPE:
            if (me->numpoints == 0) break;
            me->points[0].x += x0;
            me->points[0].y += y0;

            XDrawLines(dpy, win, me->sh_GC,
                       me->points, me->numpoints,
                       CoordModePrevious);
            
            me->points[0].x -= x0;
            me->points[0].y -= y0;
            break;
    }

    if (me->tx_present)
    {
        if      (me->tx_h_pos < 0) x = 0 + me->tx_h_ofs;
        else if (me->tx_h_pos > 0) x = wid - me->tx_wid - me->tx_h_ofs;
        else                       x = wid / 2 - me->tx_wid / 2 + me->tx_h_ofs;

        if      (me->tx_v_pos < 0) y = 0 + me->tx_v_ofs;
        else if (me->tx_v_pos > 0) y = hei - me->tx_hei - me->tx_v_ofs;
        else                       y = hei / 2 - me->tx_hei / 2 + me->tx_v_ofs;

        XmStringDraw(dpy, win,
                     me->tx_fl, me->tx_s, me->tx_GC,
                     x0 + x, y0 + y, me->tx_wid, me->tx_align,
                     dir, NULL);
    }
}

static void DecorExposeCB(Widget     w          __attribute__((unused)),
                          XtPointer  closure,
                          XtPointer  call_data  __attribute__((unused)))
{
  DataKnob k = (DataKnob) closure;
  
    DrawDecor(k, 0);
}

static CxPixel Str2Pixel(DataKnob k, const char *name, const char *spec, const char *def)
{
  Widget    cw  = (CNCRTZE(k->uplink->w));
  Display  *dpy = XtDisplay(cw);
  Colormap  cmap;
  XColor    xcol;
    
    if (*spec == '\0') spec = def;

    if (*spec == '%')
    {
        // For now -- no %-specs
        spec = "red";
    }

    XtVaGetValues(cw, XmNcolormap, &cmap, NULL);

    if (!XAllocNamedColor(dpy, cmap, spec, &xcol, &xcol))
    {
        fprintf(stderr, "Knob %s/\"%s\": failed to get %s=\"%s\"\n",
                k->ident, k->label, name, spec);
        xcol.pixel = BlackPixelOfScreen(XtScreen(cw));
    }
    
    return xcol.pixel;
}

static GC AllocPixelGC(DataKnob k, CxPixel px, int thickness, int join_round)
{
  Widget     cw  = (CNCRTZE(k->uplink->w));
  XtGCMask   mask;  
  XGCValues  vals;

    mask = GCFunction | GCForeground;
    vals.function   = GXcopy;
    vals.foreground = px;
    if (thickness > 1)
    {
        mask |= GCLineWidth;
        vals.line_width = thickness;
        
        mask |= GCJoinStyle;
        vals.join_style = join_round? JoinRound : JoinMiter;
    }

    return XtGetGC(cw, mask, &vals);
}

static void ParseTextPosition(DataKnob k, const char *pos)
{
  decor_privrec_t *me = (decor_privrec_t *)(k->privptr);
  const char      *p  = pos;

    if (strcasecmp(pos, "none") == 0)
    {
        me->tx_present = 0;
        return;
    }

    me->tx_h_ofs = strtol(p, &p, 10);
    switch (tolower(*p))
    {
        case 'l': me->tx_h_pos = -1;  break;
        case 'c': me->tx_h_pos =  0;  break;
        case 'r': me->tx_h_pos = +1;  break;
        default:  return;
    }
    p++;
    me->tx_v_ofs = strtol(p, &p, 10);
    switch (tolower(*p))
    {
        case 't': me->tx_v_pos = -1;  break;
        case 'm': me->tx_v_pos =  0;  break;
        case 'b': me->tx_v_pos = +1;  break;
    }
}

static void PrepareTextParams(DataKnob k, decoropts_t *opts_p)
{
  decor_privrec_t *me  = (decor_privrec_t *)(k->privptr);
  Widget           cw  = (CNCRTZE(k->uplink->w));
  Display         *dpy = XtDisplay(cw);

  char             fontspec[400];
  XFontStruct     *fs;
  XGCValues        vals;

    me->tx_s = XmStringCreateLtoR(k->label, xh_charset);
    me->tx_align = textalign_vals[opts_p->tx_align];
  
    /* 1. Obtain a fontlist */
    check_snprintf(fontspec, sizeof(fontspec),
                   "-*lucida-%s-*-*-%d-*-*-*-*-*-*-*",
                   fontmod_strs[opts_p->fn_mod],
                   fontsize_sizes[opts_p->fn_size - FONTSIZE_MIN]);

    fs = XLoadQueryFont(dpy, fontspec);
    if (fs == NULL)
    {
        fprintf(stderr, "Knob %s/\"%s\": unable to load font \"%s\"; using \"fixed\"\n",
                k->ident, k->label, fontspec);
        fs = XLoadQueryFont(dpy, "fixed");
    }
    
    me->tx_fl = XmFontListCreate(fs, xh_charset);

    /* 1.5. ...and calc text size */
    XmStringExtent(me->tx_fl, me->tx_s, &(me->tx_wid), &(me->tx_hei));

    /* 2. Make a GC */
    me->tx_pixel = Str2Pixel(k, "textcolor", opts_p->tx_color, "black");
    vals.foreground = me->tx_pixel;
    me->tx_GC = XtAllocateGC(cw, 0, GCForeground, &vals, GCFont, 0);
}

static void ParsePoints(DataKnob k, const char *points_spec)
{
  decor_privrec_t *me = (decor_privrec_t *)(k->privptr);
  const char      *p  = points_spec;

  XPoint           pts[100];
  int              idx;
  int              rel;
  int              curx;
  int              cury;

    for (idx = 0, curx = 0, cury = 0;
         idx < countof(pts)  &&  *p != '\0';
         /* 'idx' is incremented at the end of body */)
    {
        rel = 0;
        if (*p == '.')
        {
            p++;
            rel = 1;
        }
        
        if (*p != '+'  &&  *p != '-') break;
        pts[idx].x = strtol(p, &p, 10);
        if (*p != '+'  &&  *p != '-') break;
        pts[idx].y = strtol(p, &p, 10);
        
        if (!rel  &&  idx > 0)
        {
            pts[idx].x -= curx;
            pts[idx].y -= cury;
        }

        curx += pts[idx].x;
        cury += pts[idx].y;
        
        idx++;

        if (*p != '/') break;
        p++;
    }
    
    me->numpoints = idx;
    if (idx > 0)
    {
        /* Note: we allocate an extra point-cell at the end --
                 for the "final" point */
        me->points = (void *)XtMalloc(sizeof(*me->points) * (idx + 1));

        memcpy(me->points, pts, sizeof(*me->points) * idx);
        me->points[idx].x = me->points[0].x - curx;
        me->points[idx].y = me->points[0].y - cury;
    }
}

static int CreateDecor(DataKnob k, CxWidget parent,
                       int shape, int filled)
{
  decor_privrec_t *me = (decor_privrec_t *)(k->privptr);
  int              standalone;
  Dimension        wid;
  Dimension        hei;
  
  int              idx;
  int              curx;
  int              cury;
    
    me->shape  = shape;
    me->filled = filled;

    if (shape == SHAPE_STRING  &&  me->opts.tx_color[0] == '\0')
        strzcpy(me->opts.tx_color, me->opts.sh_color, sizeof(me->opts.tx_color));

    ParsePoints(k, me->opts.points_list);
    
    me->sh_GC = AllocPixelGC(k, me->sh_pixel = Str2Pixel(k, "color",   me->opts.sh_color, "#bbe0e3"), me->opts.thickness, me->opts.rounded);

    me->bd_width = me->opts.bd_width;
    me->bd_GC = AllocPixelGC(k, me->bd_pixel = Str2Pixel(k, "bdcolor", me->opts.bd_color, "black"), me->opts.bd_width > 1? me->opts.bd_width : 0, me->opts.rounded);

    me->tx_present = (k->label != NULL  &&  k->label[0] != '\0');
    if (me->tx_present) ParseTextPosition(k, me->opts.tx_pos);
    if (me->tx_present) PrepareTextParams(k, &(me->opts));
    
    wid = me->opts.width;  if (wid < 1) wid = 1;
    hei = me->opts.height; if (hei < 1) hei = 1;

    switch (shape)
    {
        case SHAPE_STRING:
            if (wid < me->tx_wid) wid = me->tx_wid;
            if (hei < me->tx_hei) hei = me->tx_hei;
            break;

        case SHAPE_POLYGON:
        case SHAPE_POLYLINE:
        case SHAPE_PIPE:
            for (idx = 0, curx = 0, cury = 0;  idx < me->numpoints;  idx++)
            {
                curx += me->points[idx].x;
                cury += me->points[idx].y;
                
                if (wid < curx + 1) wid = curx + 1;
                if (hei < cury + 1) hei = cury + 1;
            }
            break;
    }
    
    /* Create the widget */
    standalone = k->uplink == NULL  ||  k->uplink->vmtlink != &motifknobs_canvas_vmt;
//    standalone = 1;
    
    k->w = XtVaCreateManagedWidget("decor",
                                   standalone? xmDrawingAreaWidgetClass : xmInputOnlyWidgetClass,
                                   CNCRTZE(parent),
                                   XmNtraversalOn,     False,
                                   XmNshadowThickness, 0,
                                   XmNwidth,           wid,
                                   XmNheight,          hei,
                                   NULL);
    if (standalone) XtAddCallback(k->w, XmNexposeCallback, DecorExposeCB, (XtPointer) k);
    
    return 0;
}

////////////////////////////

static int CreateRectangleKnob(DataKnob k, CxWidget parent)
{
    return CreateDecor(k, parent, SHAPE_RECTANGLE, 0);
}

static int CreateFRectangleKnob(DataKnob k, CxWidget parent)
{
    return CreateDecor(k, parent, SHAPE_RECTANGLE, 1);
}

static int CreateEllipseKnob(DataKnob k, CxWidget parent)
{
    return CreateDecor(k, parent, SHAPE_ELLIPSE, 0);
}

static int CreateFEllipseKnob(DataKnob k, CxWidget parent)
{
    return CreateDecor(k, parent, SHAPE_ELLIPSE, 1);
}

static int CreatePolygonKnob(DataKnob k, CxWidget parent)
{
    return CreateDecor(k, parent, SHAPE_POLYGON, 0);
}

static int CreateFPolygonKnob(DataKnob k, CxWidget parent)
{
    return CreateDecor(k, parent, SHAPE_POLYGON, 1);
}

static int CreatePolylineKnob(DataKnob k, CxWidget parent)
{
    return CreateDecor(k, parent, SHAPE_POLYLINE, 0);
}

static int CreatePipeKnob(DataKnob k, CxWidget parent)
{
    return CreateDecor(k, parent, SHAPE_PIPE, 0);
}

static int CreateStringKnob(DataKnob k, CxWidget parent)
{
    return CreateDecor(k, parent, SHAPE_STRING, 0);
}

static void DestroyDecorKnob(DataKnob k)
{
  decor_privrec_t *me = (decor_privrec_t *)k->privptr;

    MotifKnobs_CommonDestroy_m(k);
    if (me->points != NULL) XtFree((void *)(me->points));
}
dataknob_noop_vmt_t motifknobs_rectangle_vmt =
{
    {DATAKNOB_NOOP, "rectangle",
        sizeof(decor_privrec_t), text2decoropts,
        0,
        CreateRectangleKnob, DestroyDecorKnob, MotifKnobs_NoColorize_m, NULL}
};

dataknob_noop_vmt_t motifknobs_frectangle_vmt =
{
    {DATAKNOB_NOOP, "frectangle",
        sizeof(decor_privrec_t), text2decoropts,
        0,
        CreateFRectangleKnob, DestroyDecorKnob, MotifKnobs_NoColorize_m, NULL}
};

dataknob_noop_vmt_t motifknobs_ellipse_vmt =
{
    {DATAKNOB_NOOP, "ellipse",
        sizeof(decor_privrec_t), text2decoropts,
        0,
        CreateEllipseKnob, DestroyDecorKnob, MotifKnobs_NoColorize_m, NULL}
};

dataknob_noop_vmt_t motifknobs_fellipse_vmt =
{
    {DATAKNOB_NOOP, "fellipse",
        sizeof(decor_privrec_t), text2decoropts,
        0,
        CreateFEllipseKnob, DestroyDecorKnob, MotifKnobs_NoColorize_m, NULL}
};

dataknob_noop_vmt_t motifknobs_polygon_vmt =
{
    {DATAKNOB_NOOP, "polygon",
        sizeof(decor_privrec_t), text2decoropts,
        0,
        CreatePolygonKnob, DestroyDecorKnob, MotifKnobs_NoColorize_m, NULL}
};

dataknob_noop_vmt_t motifknobs_fpolygon_vmt =
{
    {DATAKNOB_NOOP, "fpolygon",
        sizeof(decor_privrec_t), text2decoropts,
        0,
        CreateFPolygonKnob, DestroyDecorKnob, MotifKnobs_NoColorize_m, NULL}
};

dataknob_noop_vmt_t motifknobs_polyline_vmt =
{
    {DATAKNOB_NOOP, "polyline",
        sizeof(decor_privrec_t), text2decoropts,
        0,
        CreatePolylineKnob, DestroyDecorKnob, MotifKnobs_NoColorize_m, NULL}
};

dataknob_noop_vmt_t motifknobs_pipe_vmt =
{
    {DATAKNOB_NOOP, "pipe",
        sizeof(decor_privrec_t), text2decoropts,
        0,
        CreatePipeKnob, DestroyDecorKnob, MotifKnobs_NoColorize_m, NULL}
};

dataknob_noop_vmt_t motifknobs_string_vmt =
{
    {DATAKNOB_NOOP, "string",
        sizeof(decor_privrec_t), text2decoropts,
        0,
        CreateStringKnob, DestroyDecorKnob, MotifKnobs_NoColorize_m, NULL}
};

static int IsACanvasDecorKnob(DataKnob k)
{
  dataknob_unif_vmt_t *vmt_in_q = k->vmtlink;
  int                  n;

  static dataknob_noop_vmt_t *canvas_decor_vmts[] =
  {
      &motifknobs_rectangle_vmt,
      &motifknobs_frectangle_vmt,
      &motifknobs_ellipse_vmt,
      &motifknobs_fellipse_vmt,
      &motifknobs_polygon_vmt,
      &motifknobs_fpolygon_vmt,
      &motifknobs_polyline_vmt,
      &motifknobs_pipe_vmt,
      &motifknobs_string_vmt,
  };

    for (n = 0;  n < countof(canvas_decor_vmts);  n++)
        if (vmt_in_q == canvas_decor_vmts[n]) return 1;
    
    return 0;
}

//////////////////////////////////////////////////////////////////////


typedef struct
{
    MotifKnobs_containeropts_t  cont;

    int   nodecor;
    int   nattl;
} canvas_privrec_t;

static psp_paramdescr_t text2canvasopts[] =
{
    PSP_P_INCLUDE("container",
                  text2_MotifKnobs_containeropts,
                  PSP_OFFSET_OF(canvas_privrec_t, cont)),

    PSP_P_FLAG  ("nodecor",     canvas_privrec_t, nodecor,     1, 0),

    PSP_P_END()
};

//////////////////////////////////////////////////////////////////////

enum
{
    SIDE_LEFT   = 0,
    SIDE_RIGHT  = 1,
    SIDE_TOP    = 2,
    SIDE_BOTTOM = 3,

    NUM_SIDES   = 4
};

static struct
{
    char *name;
    char *att;
    char *wid;
    char *ofs;
} side_info[NUM_SIDES] =
{
    {"left",   XmNleftAttachment,   XmNleftWidget,   XmNleftOffset  },
    {"right",  XmNrightAttachment,  XmNrightWidget,  XmNrightOffset },
    {"top",    XmNtopAttachment,    XmNtopWidget,    XmNtopOffset   },
    {"bottom", XmNbottomAttachment, XmNbottomWidget, XmNbottomOffset},
};

typedef struct
{
    char  ainfo[NUM_SIDES][100];
} attachopts_t;

static psp_paramdescr_t text2attachopts[] =
{
    PSP_P_STRING("left",   attachopts_t, ainfo[SIDE_LEFT],   ""),
    PSP_P_STRING("right",  attachopts_t, ainfo[SIDE_RIGHT],  ""),
    PSP_P_STRING("top",    attachopts_t, ainfo[SIDE_TOP],    ""),
    PSP_P_STRING("bottom", attachopts_t, ainfo[SIDE_BOTTOM], ""),
    PSP_P_END()
};

typedef struct
{
    int   opposite;
    int   offset;
    char  sibling[100];
} attachdata_t;

static int ParseAttachmentSpec(const char *spec, attachdata_t *data)
{
  const char *p = spec;
  int         tail;

    bzero(data, sizeof(*data));
  
    if (spec == NULL) return 0;
    
    /* Skip leading spaces */
    while (*p != '\0'  &&  isspace(*p)) p++;
    if (*p == '\0') return 0;

    /* An "opposite" flag */
    if (*p == '!')
    {
        data->opposite = 1;
        p++;
    }

    /* Offset (all parsing and checking is performed by strtol(), which treates empty string as 0) */
    data->offset = strtol(p, &p, 10);

    /* Finally, a "@SIBLING" */
    if (*p == '@')
    {
        p++;
        strzcpy(data->sibling, p, sizeof(data->sibling));

        /* Trim possible trailing spaces */
        tail = strlen(data->sibling) - 1;
        while (tail >= 0  &&  isspace(data->sibling[tail]))
            data->sibling[tail--] = '\0';
    }
    
    return 1;
}

//////////////////////////////////////////////////////////////////////

static DataKnob FindChild(const char *name, DataKnob kl, int ncnvs)
{
  int  n;

    for (n = 0;  n < ncnvs;  n++)
        if (kl[n].ident != NULL  &&
            strcasecmp(kl[n].ident, name) == 0) return kl + n;
  
    return NULL;
}

static int IsACanvasDecorKnob(DataKnob k);
static void CanvasExposeHandler(Widget     w                     __attribute__((unused)),
                                XtPointer  closure,
                                XEvent    *event                 __attribute__((unused)),
                                Boolean   *continue_to_dispatch  __attribute__((unused)))
{
  DataKnob          k  = (DataKnob)closure;
  canvas_privrec_t *me = (canvas_privrec_t *)k->privptr;
  int               n;
  DataKnob          ck;
  
    for (n = 0, ck = k->u.c.content + me->nattl;
         n < k->u.c.count - me->nattl;
         n++,   ck++)
        if (IsACanvasDecorKnob(ck))
            DrawDecor(ck, 1);
}

static Widget FormMaker(DataKnob k, CxWidget parent, void *privptr)
{
    return XtVaCreateManagedWidget("form",
                                   xmFormWidgetClass,
                                   CNCRTZE(parent),
                                   XmNshadowThickness, 0,
                                   NULL);
}

static int CreateCanvas(DataKnob k, CxWidget parent)
{
  canvas_privrec_t *me = (canvas_privrec_t *)k->privptr;

  DataKnob          kl;
  int               nattl;    // # at title
  int               ncnvs;    // # in canvas body (=count-nattl)

  int               n;
  DataKnob          ck;

  attachopts_t          attopts;
  int                   side;
  attachdata_t          attdata;
  DataKnob              sk;
  Widget                sibling;
  unsigned char         attachment_type;

    if (me->nodecor) me->cont.noshadow = me->cont.notitle = me->cont.nohline = 1;

    /* Obtain parameters */
    nattl    = k->u.c.param3;

    /* Sanitize parameters */
    if (me->cont.notitle)     nattl = 0;
    if (nattl < 0)            nattl = 0;
    if (nattl > k->u.c.count) nattl = k->u.c.count;
    ncnvs = k->u.c.count   - nattl;
    kl    = k->u.c.content + nattl;
    me->nattl = nattl;

    /* Create a container... */
    if (MotifKnobs_CreateContainer(k, parent,
                                   FormMaker,
                                   NULL,
                                   (me->cont.noshadow? MOTIFKNOBS_CF_NOSHADOW : 0) |
                                   (me->cont.notitle?  MOTIFKNOBS_CF_NOTITLE  : 0) |
                                   (me->cont.nohline?  MOTIFKNOBS_CF_NOHLINE  : 0) |
                                   (me->cont.nofold?   MOTIFKNOBS_CF_NOFOLD   : 0) |
                                   (me->cont.folded?   MOTIFKNOBS_CF_FOLDED   : 0) |
                                   (me->cont.fold_h?   MOTIFKNOBS_CF_FOLD_H   : 0) |
                                   (me->cont.sepfold?  MOTIFKNOBS_CF_SEPFOLD  : 0) |
                                   (nattl != 0      ?  MOTIFKNOBS_CF_ATTITLE  : 0) |
                                   (me->cont.title_side << MOTIFKNOBS_CF_TITLE_SIDE_shift)) < 0)
        return -1;

    XtAddEventHandler(CNCRTZE(k->u.c.innage), ExposureMask, False, CanvasExposeHandler, (XtPointer)k);

    MotifKnobs_PopulateContainerAttitle(k, nattl, me->cont.title_side);

    /* Populate the element */
    /* (Walk in reverse direction in order to make intuitive stacking-order) */
    for (n = ncnvs - 1;  n >= 0;  n--)
    {
        ck = kl + n;
        if (CreateKnob(ck, k->u.c.innage) < 0) return -1;
    }

    /* And perform attachments */
    for (n = ncnvs - 1;  n >= 0;  n--)
    {
        ck = kl + n;

        if (psp_parse(ck->layinfo, NULL,
                      &attopts,
                      Knobs_wdi_equals_c, Knobs_wdi_separators, Knobs_wdi_terminators,
                      text2attachopts) != PSP_R_OK)
        {
            fprintf(stderr, "Knob %s/\"%s\".placement: %s\n",
                    ck->ident, ck->label, psp_error());
            bzero(&attopts, sizeof(attopts));
        }

        for (side = 0;  side < NUM_SIDES;  side++)
            if (ParseAttachmentSpec(attopts.ainfo[side], &attdata))
            {
                sibling = NULL;
                if (attdata.sibling[0] != '\0')
                {
                    sk = FindChild(attdata.sibling, kl, ncnvs);
                    if (sk != NULL)
                        sibling = CNCRTZE(sk->w);
                    else
                        fprintf(stderr, "Knob %s/\"%s\".placement: %s sibling \"%s\" not found\n",
                                ck->ident, ck->label,
                                side_info[side].name, attdata.sibling);
                }

                if (sibling != NULL)
                    attachment_type = attdata.opposite? XmATTACH_OPPOSITE_WIDGET
                                                      : XmATTACH_WIDGET;
                else
                    attachment_type = attdata.opposite? XmATTACH_OPPOSITE_FORM
                                                      : XmATTACH_FORM;
                
                XtVaSetValues(CNCRTZE(ck->w),
                              side_info[side].att, attachment_type,
                              side_info[side].wid, sibling,
                              side_info[side].ofs, attdata.offset,
                              NULL);
            }
    }

    return 0;
}

dataknob_cont_vmt_t motifknobs_canvas_vmt =
{
    {DATAKNOB_CONT, "canvas",
        sizeof(canvas_privrec_t), text2canvasopts,
        0,
        CreateCanvas, MotifKnobs_CommonDestroy_m, NULL, NULL},
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};
