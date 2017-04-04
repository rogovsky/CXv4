#include "Xh_includes.h"


typedef struct
{
    const char *name;
    const char *defn;
    Pixel       p;
} colinfo_t;

#define COLOR_LINE(n, d) [__CX_CONCATENATE(XH_COLOR_, n)] = {#n, d, 0}

static colinfo_t color_info[] =
{
    COLOR_LINE(FG_DEFAULT,     "#000000"),
    COLOR_LINE(BG_DEFAULT,     "#dbdbdb"),
    COLOR_LINE(TS_DEFAULT,     "#f7f7f7"),
    COLOR_LINE(BS_DEFAULT,     "#a7a7a7"),
    COLOR_LINE(BG_ARMED,       "#efefef"),
    COLOR_LINE(BG_TOOL_ARMED,  "#80ccf0"),
    COLOR_LINE(HIGHLIGHT,      "black"),
    COLOR_LINE(BORDER,         "black"),

    COLOR_LINE(FG_BALLOON,     "#000000"),
    COLOR_LINE(BG_BALLOON,     "#FFFFCC"),
    
    COLOR_LINE(BG_FDLG_INPUTS, "#d3b5b5"),

    COLOR_LINE(BG_DATA_INPUT,  "#FFFFFF"),

    COLOR_LINE(THERMOMETER,    "#0000FF"),

    COLOR_LINE(INGROUP,        "#82D2FF"),

#if 0
    /* Just-slightly-darker gray */
    COLOR_LINE(CTL3D_BG,       "#cccccc"),
    COLOR_LINE(CTL3D_TS,       "#f0f0f0"),
    COLOR_LINE(CTL3D_BS,       "#999999"),
    COLOR_LINE(CTL3D_BG_ARMED, "#e5e5e5"),
#elif 1
    /* Just-a-bit-darker gray */
    COLOR_LINE(CTL3D_BG,       "#c8c8c8"),
    COLOR_LINE(CTL3D_TS,       "#eeeeee"),
    COLOR_LINE(CTL3D_BS,       "#969696"),
    COLOR_LINE(CTL3D_BG_ARMED, "#e3e3e3"),
#elif 1
    /* XV blue-gray */
    COLOR_LINE(CTL3D_BG,       "#b2c0dc"),
    COLOR_LINE(CTL3D_TS,       "#e8ecf4"),
    COLOR_LINE(CTL3D_BS,       "#8690a5"),
    COLOR_LINE(CTL3D_BG_ARMED, "#d8dfed"),
#elif 1
    /* Mozilla "Orbit 3+1 1.2" theme light bluish gray */
    COLOR_LINE(CTL3D_BG,       "#e1e9ed"),
    COLOR_LINE(CTL3D_TS,       "#f6f8f9"),
    COLOR_LINE(CTL3D_BS,       "#a9afb2"),
    COLOR_LINE(CTL3D_BG_ARMED, "#f8f4f6"),
#elif 0
    /* Light cyan */
    COLOR_LINE(CTL3D_BG,       "#c0e6e6"),
    COLOR_LINE(CTL3D_TS,       "#ecf7f7"),
    COLOR_LINE(CTL3D_BS,       "#90adad"),
    COLOR_LINE(CTL3D_BG_ARMED, "#dff2f2"),
#elif 1
    /* Greenish gray */
    COLOR_LINE(CTL3D_BG,       "#dbe6db"),
    COLOR_LINE(CTL3D_TS,       "#f4f7f4"),
    COLOR_LINE(CTL3D_BS,       "#a4ada4"),
    COLOR_LINE(CTL3D_BG_ARMED, "#edf2ed"),
#endif
    
    COLOR_LINE(GRAPH_BG,       "#fffff0"),
    COLOR_LINE(GRAPH_AXIS,     "#000080"),  // Was: #0000C0, as grid
    COLOR_LINE(GRAPH_GRID,     "#0000C0"),
    COLOR_LINE(GRAPH_TITLES,   "#000000"),
    COLOR_LINE(GRAPH_REPERS,   "#c0f0c0"),  // Was: #000000, than #808080, than 80b080
    COLOR_LINE(GRAPH_LINE1,    "#0000FF"),  // Blue
    COLOR_LINE(GRAPH_LINE2,    "#FF0000"),  // Red
    COLOR_LINE(GRAPH_LINE3,    "#00C000"),  // Dark green
    COLOR_LINE(GRAPH_LINE4,    "#FFC000"),  // Darker gold
    COLOR_LINE(GRAPH_LINE5,    "#87ceff"),  // Foggy lightcyan
    COLOR_LINE(GRAPH_LINE6,    "#EE82EE"),  // X11 "violet"
    COLOR_LINE(GRAPH_LINE7,    "#006400"),  // X11 "dark green"
    COLOR_LINE(GRAPH_LINE8,    "#B26818"),  // ==JUST_BROWN=VGAcolor#6
    COLOR_LINE(GRAPH_BAR1,     "#00c080"),  // Was: #00FF00, but changed because of 1.Was too bright; 2.Overlapped with RELAX
    COLOR_LINE(GRAPH_BAR2,     "#0000FF"),
    COLOR_LINE(GRAPH_BAR3,     "#FF0000"),
    COLOR_LINE(GRAPH_PREV,     "#C0C0C0"),
    COLOR_LINE(GRAPH_OUTL,     "#000000"),
    
    COLOR_LINE(BGRAPH_BG,      "#000000"),
    COLOR_LINE(BGRAPH_AXIS,    "#e0e0e0"),
    COLOR_LINE(BGRAPH_GRID,    "#a0a0a0"),
    COLOR_LINE(BGRAPH_TITLES,  "#FFFFFF"),
    COLOR_LINE(BGRAPH_REPERS,  "#c0f0c0"),  // Was: #000000, than #808080, than 80b080
    COLOR_LINE(BGRAPH_LINE1,   "#0000FF"),  // Blue
    COLOR_LINE(BGRAPH_LINE2,   "#FF0000"),  // Red
    COLOR_LINE(BGRAPH_LINE3,   "#00C000"),  // Dark green
    COLOR_LINE(BGRAPH_LINE4,   "#FFC000"),  // Darker gold
    COLOR_LINE(BGRAPH_LINE5,   "#87ceff"),  // Foggy lightcyan
    COLOR_LINE(BGRAPH_LINE6,   "#EE82EE"),  // X11 "violet"
    COLOR_LINE(BGRAPH_LINE7,   "#006400"),  // X11 "dark green"
    COLOR_LINE(BGRAPH_LINE8,   "#B26818"),  // ==JUST_BROWN=VGAcolor#6
    COLOR_LINE(BGRAPH_BAR1,    "#00dd00"),  // Green
    COLOR_LINE(BGRAPH_BAR2,    "#0000FF"),  // Blue
    COLOR_LINE(BGRAPH_BAR3,    "#dd0000"),  // Red
    COLOR_LINE(BGRAPH_PREV,    "#C0C0C0"),
    COLOR_LINE(BGRAPH_OUTL,    "#F0F0F0"),
    
    COLOR_LINE(FG_HILITED,     "#C000FF"),
    COLOR_LINE(FG_IMPORTANT,   "#0000FF"),
    COLOR_LINE(FG_DIM,         "#a7a7a7"), /* In fact -- bottomShadowColor :-) */
    COLOR_LINE(FG_WEIRD,       "#FFFFFF"), // To be legible on BG_WEIRD
    COLOR_LINE(BG_VIC,         "#6dd06d"),
    COLOR_LINE(FG_HEADING,     "#FFFFFF"),
    COLOR_LINE(BG_HEADING,     "#1818b2"), // VGA color #1 -- taken from my fte045's con_x11.cpp
    COLOR_LINE(BG_NORM_YELLOW, "#eded6d"),
    COLOR_LINE(BG_NORM_RED,    "pink"),
    COLOR_LINE(BG_IMP_YELLOW,  "#FFFF00"),
    COLOR_LINE(BG_IMP_RED,     "#FF0000"),
    COLOR_LINE(BG_WEIRD,       "#0000FF"), // Now is blue
    COLOR_LINE(BG_JUSTCREATED, "#c0e6e6"),
    COLOR_LINE(BG_DEFUNCT,     "#4682b4"),
    COLOR_LINE(BG_HWERR,       "maroon"),
    COLOR_LINE(BG_SFERR,       "#8b8b00"),
    COLOR_LINE(BG_OTHEROP,     "orange"),
    COLOR_LINE(BG_PRGLYCHG,    "#d8e3d5"),
    COLOR_LINE(BG_NOTFOUND,    "#404040"),
    
    COLOR_LINE(JUST_RED,       "#FF0000"),
    COLOR_LINE(JUST_ORANGE,    "orange"),
    COLOR_LINE(JUST_YELLOW,    "#FFFF00"),
    COLOR_LINE(JUST_GREEN,     "#00FF00"),
    COLOR_LINE(JUST_BLUE,      "#0000FF"),
    COLOR_LINE(JUST_INDIGO,    "#4B0082"),  // From http://en.wikipedia.org/wiki/Indigo
    COLOR_LINE(JUST_VIOLET,    "#7F00FF"),  // From http://en.wikipedia.org/wiki/Violet_(color)

    COLOR_LINE(JUST_BLACK,     "#000000"),
    COLOR_LINE(JUST_WHITE,     "#FFFFFF"),
    COLOR_LINE(JUST_GRAY,      "#808080"),  // From http://en.wikipedia.org/wiki/Gray_(color) -- halfway between black and white

    COLOR_LINE(JUST_AMBER,     "#FFE000"),
    COLOR_LINE(JUST_BROWN,     "#B26818"),  // VGA color #6 -- taken from my fte045's con_x11.cpp
};

typedef struct
{
    const char *spec;
    int         n;
} defcolspec_t;

defcolspec_t defcoldb[] =
{
    {"*background",                                 XH_COLOR_BG_DEFAULT},
    {"*foreground",                                 XH_COLOR_FG_DEFAULT},
    {"*topShadowColor",                             XH_COLOR_TS_DEFAULT},
    {"*bottomShadowColor",                          XH_COLOR_BS_DEFAULT},
    {"*armColor",                                   XH_COLOR_BG_ARMED},
    {"*toolButton.armColor",                        XH_COLOR_BG_TOOL_ARMED},
    {"*miniToolButton.armColor",                    XH_COLOR_BG_TOOL_ARMED},
    {"*highlightColor",                             XH_COLOR_BORDER},
    {"*borderColor",                                XH_COLOR_BORDER},
    
    {"*XmTextField.background",                     XH_COLOR_BG_FDLG_INPUTS},
    {"*XmList.background",                          XH_COLOR_BG_FDLG_INPUTS},
    
    {"*tipLabel.background",                        XH_COLOR_BG_BALLOON},
    {"*tipShell.background",                        XH_COLOR_BG_BALLOON},
    {"*TipLabel.background",                        XH_COLOR_BG_BALLOON},
    {"*TipShell.background",                        XH_COLOR_BG_BALLOON},
    {"*tipLabel.foreground",                        XH_COLOR_FG_BALLOON},
    {"*tipShell.foreground",                        XH_COLOR_FG_BALLOON},
    {"*TipLabel.foreground",                        XH_COLOR_FG_BALLOON},
    {"*TipShell.foreground",                        XH_COLOR_FG_BALLOON},
    
    {"*text_i.background",                          XH_COLOR_BG_DATA_INPUT},
    {"*dialDial.ringBackground",                    XH_COLOR_BG_DATA_INPUT},

    {"*slider_thermometer.troughColor",             XH_COLOR_THERMOMETER},

    {"*onoff_i.unselectColor",                      XH_COLOR_CTL3D_BG},
    {"*onoff_i.topShadowColor",                     XH_COLOR_CTL3D_TS},
    {"*onoff_i.bottomShadowColor",                  XH_COLOR_CTL3D_BS},
    
    {"*push_i.background",                          XH_COLOR_CTL3D_BG},
    {"*push_i.topShadowColor",                      XH_COLOR_CTL3D_TS},
    {"*push_i.bottomShadowColor",                   XH_COLOR_CTL3D_BS},
    {"*push_i.armColor",                            XH_COLOR_CTL3D_BG_ARMED},
    
    {"*arrow_i.background",                         XH_COLOR_CTL3D_BG},
    {"*arrow_i.topShadowColor",                     XH_COLOR_CTL3D_TS},
    {"*arrow_i.bottomShadowColor",                  XH_COLOR_CTL3D_BS},
    {"*arrow_i.armColor",                           XH_COLOR_CTL3D_BG_ARMED},

    {"*slider_i.XmScrollBar.background",            XH_COLOR_CTL3D_BG},
    {"*slider_i.XmScrollBar.topShadowColor",        XH_COLOR_CTL3D_TS},
    {"*slider_i.XmScrollBar.bottomShadowColor",     XH_COLOR_CTL3D_BS},

    {"*selector_i.OptionButton.background",         XH_COLOR_CTL3D_BG},
    {"*selector_i.OptionButton.topShadowColor",     XH_COLOR_CTL3D_TS},
    {"*selector_i.OptionButton.bottomShadowColor",  XH_COLOR_CTL3D_BS},
    {"*selectorPullDown*background",                XH_COLOR_CTL3D_BG},
    {"*selectorPullDown*topShadowColor",            XH_COLOR_CTL3D_TS},
    {"*selectorPullDown*bottomShadowColor",         XH_COLOR_CTL3D_BS},

#if 0 /* Put "1" here to make dialog buttons as darker as knobs */
    {"*XmPushButtonGadget.background",              XH_COLOR_CTL3D_BG},
    {"*XmPushButtonGadget.topShadowColor",          XH_COLOR_CTL3D_TS},
    {"*XmPushButtonGadget.bottomShadowColor",       XH_COLOR_CTL3D_BS},
    {"*XmPushButtonGadget.armColor",                XH_COLOR_CTL3D_BG_ARMED},
#endif

    /* Countermeasure against KDE/GNOME stupid resources
       (/usr/share/apps/kdisplay/app-defaults and
       /usr/share/control-center-2.0/xrdb) */
    {"*statusLine.background",                      XH_COLOR_BG_DEFAULT},
    {"*text_o.background",                          XH_COLOR_BG_DEFAULT},
    
    {NULL, 0}
};


int    XhSetColorBinding(const char *name, const char *defn)
{
  int  x;
    
    for (x = 0;  x < countof(color_info);  x++)
        if (strcasecmp(name, color_info[x].name) == 0)
        {
            color_info[x].defn = defn;
            return 0;
        }

    return -1;
}

Pixel  XhGetColor(int n)
{
    if (n < countof(color_info))
        return color_info[n].p;

    fprintf(stderr, "%s(): ERROR: color %d requested, while max allowed is %d\n",
            __FUNCTION__, n, countof(color_info) - 1);
    return color_info[XH_COLOR_FG_DEFAULT].p;
}

Pixel XhGetColorByName (const char *name)
{
  int  x;
    
    for (x = 0;  x < countof(color_info);  x++)
        if (strcasecmp(name, color_info[x].name) == 0)
            return XhGetColor(x);

    return XhGetColor(XH_COLOR_JUST_RED);
}

const char *XhStrColindex(int n)
{
  static char  buf[100];
    
    if (n < countof(color_info))
        return color_info[n].name;

    sprintf(buf, "?COL#%d?", n);
    return buf;
}

int     XhColorCount     (void)
{
    return countof(color_info);
}


static int   colors_are_allocated = 0;

void _XhAllocateColors(Widget w)
{
  Colormap     cmap;
  XColor       xcol;
  int          x;

    if (colors_are_allocated) return;
  
    /* Get a colormap reference */
    XtVaGetValues(w, XmNcolormap, &cmap, NULL);

    /* Allocate colors */
    for (x = 0;  x < countof(color_info); x++)
    {
        if (!XAllocNamedColor(XtDisplay(w), cmap, color_info[x].defn, &xcol, &xcol))
        {
            fprintf(stderr, "Failed to allocate color %s=%d '%s'\n",
                    color_info[x].name, x, color_info[x].defn);
            /* Ideally, if we could obtain r,g,b, we can test
             if r*3+g*6+b*1<2550/2 and return black else white */
            xcol.pixel =
                x == XH_COLOR_BG_DEFAULT ?  BlackPixelOfScreen(XtScreen(w))
                                         :  WhitePixelOfScreen(XtScreen(w));
        }
        color_info[x].p = xcol.pixel;
    }
    
    colors_are_allocated = 1;
}

static Bool ResourceChecker(XrmDatabase       *database __attribute__((unused)),
                            XrmBindingList     bindings,
                            XrmQuarkList       quarks,
                            XrmRepresentation *type     __attribute__((unused)),
                            XrmValue          *value    __attribute__((unused)),
                            XPointer           arg)
{
  const char *spec = (const char *)arg;
  char        buf[1000];
  size_t      used;
  size_t      len;
  int         fns;
  const char *q;
  const char *s;
  
    for (used = 0, buf[0] = '\0', fns = 0;
         *quarks;
         bindings++, quarks++, fns = 1)
    {
        q = XrmQuarkToString(*quarks);
        if      (*bindings == XrmBindLoosely) s = "*";
        else if (fns)                         s = ".";
        else                                  s = "";

        len = strlen(s) + strlen(q);

        if (used + len > sizeof(buf) - 1) return False;

        strcat(buf, s);
        strcat(buf, q);
    }

    /* Note: resource specifications are case-SENSITIVE */

////    if (strcmp(buf, spec) == 0) fprintf(stderr, "%s: yes\n", spec);
    
    return strcmp(buf, spec) == 0;
}

static int ResourcePresent(XrmDatabase  db, const char *spec)
{
  XrmQuark empty = NULLQUARK;
    
    return XrmEnumerateDatabase(db, &empty, &empty, XrmEnumAllLevels,
                                ResourceChecker, (XPointer)spec);
}

void _XhColorPatchXrmDB(Display *dpy)
{
  XrmDatabase  db;
  int          x;

    db = XtDatabase(dpy);
    for (x = 0;  defcoldb[x].spec != NULL;  x++)
        if (!ResourcePresent(db, defcoldb[x].spec))
            XrmPutStringResource(&db, defcoldb[x].spec, color_info[defcoldb[x].n].defn);
}
