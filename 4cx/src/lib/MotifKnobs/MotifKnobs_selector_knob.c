#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/PushB.h>
#include <Xm/RowColumn.h>

#include "misc_macros.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_selector_knob.h"
// For TMP: v2-style button style support
#include "Xh_fallbacks.h"


typedef struct
{
    int     is_compact;
    int     no_label;
    int     alignment;

    int     ign_otherop;
    
    Widget *items;
    int    *bgs;
    int     numitems;

    Widget  csc;
    
    int     defcols_obtained;
    Pixel   csc_deffg;
    Pixel   csc_defbg;
    int     cur_bgidx;
} selector_privrec_t;

static psp_lkp_t alignment_lkp[] =
{
    {"left",    XmALIGNMENT_BEGINNING},
    {"right",   XmALIGNMENT_END},
    {"center",  XmALIGNMENT_CENTER},
    {NULL, 0}
};


static psp_paramdescr_t text2selectoropts[] =
{
    PSP_P_FLAG  ("compact",     selector_privrec_t, is_compact,  1, 0),
    PSP_P_FLAG  ("nolabel",     selector_privrec_t, no_label,    1, 0),

    PSP_P_LOOKUP("align",       selector_privrec_t, alignment,
                 XmALIGNMENT_BEGINNING, alignment_lkp),

    PSP_P_FLAG  ("ign_otherop", selector_privrec_t, ign_otherop, 1, 0),
    
    PSP_P_END()
};

static void SelectorSetValue_m(DataKnob k, double v);
static void SelectorColorize_m(DataKnob k, knobstate_t newstate);

static void HandleBgIdxChange(DataKnob k, int newidx)
{
  selector_privrec_t *me = (selector_privrec_t *)k->privptr;

  int  oldidx;
    
    oldidx = me->cur_bgidx;
    me->cur_bgidx = newidx;

    if (newidx != oldidx) SelectorColorize_m(k, k->curstate);
}

static void ActivateCB(Widget     w,
                       XtPointer  closure,
                       XtPointer  call_data __attribute__((unused)))
{
  DataKnob            k  = (DataKnob)closure;
  selector_privrec_t *me = (selector_privrec_t *)k->privptr;
  int                 y;
    
    /* Mmmm...  We have to walk through the whole list to find *which* one... */
    for (y = 0;  y < me->numitems; y++)
    {
        if (me->items[y] == w)
        {
            if (k->is_rw)
            {
                MotifKnobs_SetControlValue(k, y, 1);
                HandleBgIdxChange(k, me->bgs[y]);
            }
            else
            {
                /* Handle readonly selectors */
                if (y != (int)(k->u.k.curv))
                    SelectorSetValue_m(k, k->u.k.curv);
            }
            return;
        }
    }

    /* Hmmm...  Nothing found?!?!?! */
    fprintf(stderr, "%s::%s: no widget found!\n", __FILE__, __FUNCTION__);
}

static void Mouse1Handler(Widget     w       __attribute__((unused)),
                          XtPointer  closure __attribute__((unused)),
                          XEvent    *event,
                          Boolean   *continue_to_dispatch)
{
  XButtonPressedEvent *ev  = (XButtonPressedEvent *) event;

    if (ev->button == Button1) *continue_to_dispatch = False;
}


/* ((( TMP: v2-style button style support */
typedef struct
{
    int   color;
    int   bg;
    int   size;
    int   bold;
    char *icon;
} buttonopts_t;
static psp_paramdescr_t text2buttonopts[] =
{
    PSP_P_LOOKUP ("color",  buttonopts_t, color, -1,                MotifKnobs_colors_lkp),
    PSP_P_LOOKUP ("bg",     buttonopts_t, bg,    -1,                MotifKnobs_colors_lkp),
    PSP_P_LOOKUP ("size",   buttonopts_t, size,  -1,                MotifKnobs_sizes_lkp),
    PSP_P_FLAG   ("bold",   buttonopts_t, bold,  1,                 0),
    PSP_P_FLAG   ("medium", buttonopts_t, bold,  0,                 1),
    PSP_P_MSTRING("icon",   buttonopts_t, icon,  NULL,              100),
    PSP_P_END()
};
static int TuneSelButton(const char *style, CxWidget w, int flags, int *bg_idx_p,
                         DataKnob k, int y)
{
  buttonopts_t    opts;

  CxPixel         bg;
  Colormap        cmap;
  CxPixel         junk;
  CxPixel         ts;
  CxPixel         bs;
  CxPixel         am;

  char            fontspec[100];

  const char     *rqd_prefix = "V2STYLE:";

    if (style == NULL  ||
        memcmp(style, rqd_prefix, strlen(rqd_prefix)) != 0)
        return -1;

    bzero(&opts, sizeof(opts));
    if (psp_parse(style + strlen(rqd_prefix), NULL,
                  &opts,
                  Knobs_wdi_equals_c, Knobs_wdi_separators, Knobs_wdi_terminators,
                  text2buttonopts) != PSP_R_OK)
    {
        fprintf(stderr, "%s: [%s/\"%s\"].item[%d].style: %s \n",
                __FUNCTION__, k->ident, k->label, y, psp_error());
        psp_free(&opts, text2buttonopts);
        return +1;
    }

    if (opts.bg > 0)
    {
        bg = XhGetColor(opts.bg);
        XtVaGetValues(XtParent(w), XmNcolormap, &cmap, NULL);
        XmGetColors(XtScreen(XtParent(w)), cmap, bg, &junk, &ts, &bs, &am);

        XtVaSetValues(w,
                      XmNbackground,        bg,
                      XmNtopShadowColor,    ts,
                      XmNbottomShadowColor, bs,
                      XmNarmColor,          am,
                      NULL);
    }
    if (opts.color > 0)
    {
        XtVaSetValues(w, XmNforeground, XhGetColor(opts.color), NULL);
    }

    if (opts.size > 0  &&  (flags & TUNE_BUTTON_KNOB_F_NO_FONT) == 0)
    {
        check_snprintf(fontspec, sizeof(fontspec),
                       "-*-" XH_FONT_PROPORTIONAL_FMLY "-%s-r-*-*-%d-*-*-*-*-*-*-*",
                       opts.bold? XH_FONT_BOLD_WGHT : XH_FONT_MEDIUM_WGHT,
                       opts.size);
        XtVaSetValues(w,
                      XtVaTypedArg, XmNfontList, XmRString, fontspec, strlen(fontspec)+1,
                      NULL);
    }

//    if ((flags & TUNE_BUTTON_KNOB_F_NO_PIXMAP) == 0)
//        MaybeAssignPixmap(w, opts.icon);

    if (bg_idx_p != NULL) *bg_idx_p = opts.bg;

    psp_free(&opts, text2buttonopts);

    return 0;
}
/* ))) TMP: v2-style button style support */
typedef struct
{
    int  armidx;
} itemstyle_t;

static psp_paramdescr_t text2itemstyle[] =
{
    PSP_P_LOOKUP("lit",  itemstyle_t, armidx, -1, MotifKnobs_colors_lkp),
    PSP_P_END()
};

static int CreateSelectorKnob(DataKnob k, CxWidget parent)
{
  selector_privrec_t    *me = (selector_privrec_t *)k->privptr;

  Arg                    al[20];
  int                    ac;
  XmString               s;

  char                  *lp;
  
  Widget                 pulldown;
  Widget                 selector;
  Widget                 label_w;

  int                    y;
  datatree_items_prv_t   irec;
  char                   l_buf[200];
  char                  *l_buf_p;
  char                  *tip_p;
  char                  *style_p;
  int                    disabled;
  itemstyle_t            istyle;
  int                    r;

  Dimension              shd_thk;
  Dimension              mrg_hgt;
  
    k->behaviour |= DATAKNOB_B_IS_SELECTOR | DATAKNOB_B_STEP_FXD;
    if (me->ign_otherop) k->behaviour |= DATAKNOB_B_IGN_OTHEROP;

    /* Obtain number of items and allocate them */
    me->numitems = get_knob_items_count(k->u.k.items, &irec);
    if (me->numitems < 0)  return -1;
    me->items = (void *)(XtCalloc(me->numitems, sizeof(me->items[0])));
    me->bgs   = (void *)(XtCalloc(me->numitems, sizeof(me->bgs  [0])));
    if (me->items == NULL  ||  me->bgs == NULL)
    {
        XtFree(me->bgs);
        XtFree(me->items);
        return -1;
    }
    bzero(me->bgs, sizeof(me->bgs[0]) * me->numitems);
    
    /* Create a pulldown menu... */
    ac = 0;
    XtSetArg(al[ac], XmNentryAlignment, me->alignment); ac++;
    pulldown =
        XmCreatePulldownMenu(CNCRTZE(parent), "selectorPullDown", al, ac);

    /* ...and populate it */
    for (y = 0;  y < me->numitems;  y++)
    {
        get_knob_item_n(&irec, y, l_buf, sizeof(l_buf), &tip_p, &style_p);
        
        l_buf_p  = l_buf;
        disabled = 0;
        if (*l_buf_p == CX_KNOB_NOLABELTIP_PFX_C)
        {
            l_buf_p++;
            disabled = 1;
        }

        me->items[y] =
            XtVaCreateManagedWidget("selectorItem",
                                    xmPushButtonWidgetClass,
                                    pulldown,
                                    XmNlabelString, s = XmStringCreateLtoR(l_buf_p, xh_charset),
                                    NULL);
        XmStringFree(s);
        if (disabled) XtSetSensitive(me->items[y], False);
        XtAddCallback(me->items[y], XmNactivateCallback, ActivateCB, (XtPointer)k);

        if (tip_p != NULL  &&  *tip_p != '\0')
            XhSetBalloon(ABSTRZE(me->items[y]), tip_p);

        r = TuneSelButton(style_p, me->items[y], 0, &istyle.armidx, k, y);
        if      (r == 0)
        {
            me->bgs[y] = istyle.armidx;
            if (y == 0) me->cur_bgidx = istyle.armidx;

            if (istyle.armidx > 0)
                XtVaSetValues(me->items[y], XmNbackground, XhGetColor(istyle.armidx), NULL);
        }
        else if (r < 0)
        {
            r = psp_parse(style_p, NULL,
                          &istyle,
                          Knobs_wdi_equals_c, Knobs_wdi_separators, Knobs_wdi_terminators,
                          text2itemstyle);
            if (r != PSP_R_OK)
            {
                fprintf(stderr, "%s: [%s/\"%s\"].item[%d:\"%s\"].style: %s\n",
                        __FUNCTION__, k->ident, k->label, y, l_buf_p, psp_error());
                if (r == PSP_R_USRERR)
                    psp_parse("", NULL,
                              &istyle,
                              Knobs_wdi_equals_c, Knobs_wdi_separators, Knobs_wdi_terminators,
                              text2itemstyle);
                else
                    bzero(&istyle, sizeof(istyle));
            }
            me->bgs[y] = istyle.armidx;
            if (y == 0) me->cur_bgidx = istyle.armidx;

            if (istyle.armidx > 0)
                XtVaSetValues(me->items[y], XmNbackground, XhGetColor(istyle.armidx), NULL);
        }
    }

    /* Now, the selector widget itself... */
    ac = 0;
    XtSetArg(al[ac], XmNsubMenuId,   pulldown);     ac++;
    XtSetArg(al[ac], XmNtraversalOn, k->is_rw); ac++;
    
    s = NULL;
    if (!me->no_label) me->no_label = !get_knob_label(k, &lp);
    if (!me->no_label)
    {
        XtSetArg(al[ac], XmNlabelString, s = XmStringCreateLtoR(lp, xh_charset)); ac++;
    }
    if (me->is_compact)
    {
        XtSetArg(al[ac], XmNorientation, XmVERTICAL);   ac++;
        XtSetArg(al[ac], XmNspacing,     0);            ac++;
    }
    else
    {
        XtSetArg(al[ac], XmNorientation, XmHORIZONTAL); ac++;
    }

    selector = XmCreateOptionMenu(CNCRTZE(parent),
                                  k->is_rw? "selector_i" : "selector_o",
                                  al, ac);
    if (s != NULL) XmStringFree(s);

    /* ...and tailor it */
    label_w = XtNameToWidget(selector, "OptionLabel");
    if (label_w != NULL  &&  me->no_label)
        XtUnmanageChild(label_w);
    if (label_w != NULL)
        XtVaSetValues(label_w, XmNalignment, XmALIGNMENT_BEGINNING, NULL);

    me->csc = XtNameToWidget(selector, "OptionButton");
    if (me->csc != NULL)
        XtVaSetValues(me->csc, XmNalignment, me->alignment, NULL);

    XtManageChild(selector);

    /*!!!Note: we use the fact that only "selector" is a true widget
     with XWindow, while "label" and "cascade" are gadgets.  This has two
     consequences: 1) hooking to selector is enough; 2) attempt to hook to
     {label,cascade}_w leads to SIGSEGV. */
    MotifKnobs_HookButton3(k, selector);
    
    /* Block Button1 for readonly selectors */
    if (me->csc != NULL  &&  !k->is_rw)
    {
        XtVaGetValues(me->csc,
                      XmNshadowThickness, &shd_thk,
                      XmNmarginHeight,    &mrg_hgt,
                      NULL);
        XtVaSetValues(me->csc,
                      XmNshadowThickness, 0,
                      XmNmarginHeight,    mrg_hgt + shd_thk,
                      NULL);

        XtInsertEventHandler(selector, ButtonPressMask, False,
                             Mouse1Handler, (XtPointer)k, XtListHead);
    }
    
    k->w = ABSTRZE(selector);

    MotifKnobs_DecorateKnob(k);
    
    return 0;
}

static void DestroySelectorKnob(DataKnob k)
{
  selector_privrec_t *me = (selector_privrec_t *)k->privptr;

    MotifKnobs_CommonDestroy_m(k);
    XtFree((void *)(me->items));
    XtFree((void *)(me->bgs));
}

static void SelectorSetValue_m(DataKnob k, double v)
{
  selector_privrec_t *me   = (selector_privrec_t *)k->privptr;
  int                 item = (int)v;
  
    if (item < 0)             item = 0;
    if (item >= me->numitems) item = me->numitems - 1;
    
    XtVaSetValues(CNCRTZE(k->w),
                  XmNmenuHistory, me->items[item],
                  NULL);

    HandleBgIdxChange(k, me->bgs[item]);
}

static void SelectorColorize_m(DataKnob k, knobstate_t newstate)
{
  selector_privrec_t *me = (selector_privrec_t *)k->privptr;
  Pixel               fg;
  Pixel               bg;

    if (!me->defcols_obtained)
    {
        if (me->csc != NULL)
            XtVaGetValues(me->csc,
                          XmNforeground, &(me->csc_deffg),
                          XmNbackground, &(me->csc_defbg),
                          NULL);
        
        me->defcols_obtained = 1;
    }

    MotifKnobs_CommonColorize_m(k, newstate);

    if (me->csc != NULL)
    {
        MotifKnobs_ChooseColors(k->colz_style, newstate,
                                me->csc_deffg, me->csc_defbg, &fg, &bg);
        if (me->cur_bgidx > 0) bg = XhGetColor(me->cur_bgidx);

        XtVaSetValues(me->csc,
                      XmNforeground, fg,
                      XmNbackground, bg,
                      NULL);
    }
}

dataknob_knob_vmt_t motifknobs_selector_vmt =
{
    {DATAKNOB_KNOB, "selector",
        sizeof(selector_privrec_t), text2selectoropts,
        0,
        CreateSelectorKnob, DestroySelectorKnob,
        SelectorColorize_m, NULL},
    SelectorSetValue_m, NULL
};

