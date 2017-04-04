#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/ArrowB.h>
#include <Xm/PushB.h>

#include "misc_macros.h"
#include "Xh.h"
#include "Xh_globals.h"
#include "Xh_fallbacks.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_button_knob.h"


typedef struct
{
    int  cur_lit;
    int  lit_idx;
    int  off_idx;
    int  size;
} button_privrec_t;

static psp_paramdescr_t text2buttonopts[] =
{
    PSP_P_LOOKUP("color",  button_privrec_t, lit_idx, XH_COLOR_JUST_RED, MotifKnobs_colors_lkp),
    PSP_P_LOOKUP("offcol", button_privrec_t, off_idx, -1,                MotifKnobs_colors_lkp),
    PSP_P_LOOKUP("size",   button_privrec_t, size,    -1,                MotifKnobs_sizes_lkp),
    PSP_P_END()
};

static void ActivateCB(Widget     w         __attribute__((unused)),
                       XtPointer  closure,
                       XtPointer  call_data __attribute__((unused)))
{
  DataKnob  k = (DataKnob)closure;
  
    if (k->is_rw)
        MotifKnobs_SetControlValue(k, CX_VALUE_COMMAND, 1);
    else
        ack_knob_alarm            (k);
}

static void EquipButtonKnob(DataKnob k)
{
    k->behaviour |= DATAKNOB_B_IS_LIGHT | DATAKNOB_B_IS_BUTTON | DATAKNOB_B_IGN_OTHEROP | DATAKNOB_B_STEP_FXD;

    XtAddCallback(CNCRTZE(k->w), XmNactivateCallback,
                  ActivateCB, (XtPointer)k);
    MotifKnobs_HookButton3 (k, CNCRTZE(k->w));
    MotifKnobs_DecorateKnob(k);
}

static int CreateButtonKnob(DataKnob k, CxWidget parent)
{
  XmString      s;
  char         *ls;

    if (!get_knob_label(k, &ls)) ls = " ";

    k->w = ABSTRZE(XtVaCreateManagedWidget(k->is_rw? "push_i" : "push_o",
                                           xmPushButtonWidgetClass,
                                           CNCRTZE(parent),
                                           XmNtraversalOn, k->is_rw,
                                           XmNlabelString, s = XmStringCreateLtoR(ls, xh_charset),
                                           NULL));
    XmStringFree(s);

    MotifKnobs_TuneButtonKnob(k, k->w, 0);
    EquipButtonKnob(k);
    
    return 0;
}

static void SwapShadowsCB(Widget     w,
                          XtPointer  closure   __attribute__((unused)),
                          XtPointer  call_data __attribute__((unused)))
{
  Pixel  ts, bs;
  
    XtVaGetValues(w,
                  XmNtopShadowColor,    &ts,
                  XmNbottomShadowColor, &bs,
                  NULL);

    XtVaSetValues(w,
                  XmNtopShadowColor,    bs,
                  XmNbottomShadowColor, ts,
                  NULL);
}

static int CreateArrowKnob(DataKnob k, CxWidget parent, unsigned char dir)
{
  button_privrec_t *me     = (button_privrec_t *)k->privptr;

  int        has_shadow;

  XmString   s;
  Dimension  lw, lh;
  Dimension  asize;

  typedef struct
  {
      XmFontList  font;
      Dimension   margin_height;
      Dimension   margin_top;   
      Dimension   margin_bottom;
      Dimension   shadow_thickness;
      Dimension   highlight_thickness;
      Dimension   border_width;
  } labemu_t;
  labemu_t   lei;
  XtResource labemuRes[] = {
      {
          XmNfontList,           XmCFontList,           XmRFontList,
          sizeof(XmFontList), XtOffsetOf(labemu_t, font),
          NULL, 0
      },
      {
          XmNmarginHeight,       XmCMarginHeight,       XmRVerticalDimension,
          sizeof(Dimension),  XtOffsetOf(labemu_t, margin_height),
          XmRImmediate, (XtPointer)2
      },
      {
          XmNmarginTop,          XmCMarginTop,          XmRVerticalDimension,
          sizeof(Dimension),  XtOffsetOf(labemu_t, margin_top),
          XmRImmediate, (XtPointer)0
      },
      
      {
          XmNmarginBottom,       XmCMarginBottom,       XmRVerticalDimension,
          sizeof(Dimension),  XtOffsetOf(labemu_t, margin_bottom),
          XmRImmediate, (XtPointer)0
      },
      {
          XmNshadowThickness,    XmCShadowThickness,    XmRHorizontalDimension,
          sizeof (Dimension), XtOffsetOf(labemu_t, shadow_thickness),
          XmRImmediate, (XtPointer) 2
      },
      {
          XmNhighlightThickness, XmCHighlightThickness, XmRHorizontalDimension,
          sizeof (Dimension), XtOffsetOf(labemu_t, highlight_thickness),
          XmRImmediate, (XtPointer) 0
      },
      {
          XmNborderWidth,        XmCBorderWidth,        XmRHorizontalDimension,
          sizeof (Dimension), XtOffsetOf(labemu_t, border_width),
          XmRImmediate, (XtPointer) 0
      },
  };
  
    s = XmStringCreate("N", xh_charset);
    XtGetSubresources(CNCRTZE(parent), (XtPointer)&lei,
                      k->is_rw? "push_i" : "push_o", "XmPushButton",
                      labemuRes, XtNumber(labemuRes),
                      NULL, 0);
    XmStringExtent(lei.font, s, &lw, &lh);
    if (me->size > 0)
    {
      char         fontspec[100];
      XFontStruct *finfo;
      XmFontList   flist;
      
        check_snprintf(fontspec, sizeof(fontspec),
                       "-*-" XH_FONT_PROPORTIONAL_FMLY "-%s-r-*-*-%d-*-*-*-*-*-*-*",
                       XH_FONT_MEDIUM_WGHT,
                       me->size);
        finfo = XLoadQueryFont(XtDisplay(CNCRTZE(parent)), fontspec);
        if (finfo != NULL)
        {
            flist = XmFontListCreate(finfo, xh_charset);
            if (flist != NULL)
            {
                XmStringExtent(flist, s, &lw, &lh);
                XmFontListFree(flist);
            }
            XFreeFont(XtDisplay(CNCRTZE(parent)), finfo);
        }
    }
    XmStringFree(s);
    asize = (
             lei.border_width +
             lei.highlight_thickness +
             lei.shadow_thickness +
             lei.margin_height
            ) * 2 +
        lei.margin_top + lh + lei.margin_bottom;
    
    has_shadow = 1;
    if (!k->is_rw) has_shadow = 0;
    
    k->w = XtVaCreateManagedWidget(k->is_rw? "arrow_i" : "arrow_o",
                                   xmArrowButtonWidgetClass,
                                   CNCRTZE(parent),
                                   XmNtraversalOn,    k->is_rw,
                                   XmNarrowDirection, dir,
                                   XmNwidth,          asize,
                                   XmNheight,         asize,
                                   NULL);

    if (me->off_idx >= 0)
        XtVaSetValues(k->w, XmNforeground, XhGetColor(me->off_idx), NULL);

    if (has_shadow)
    {
        XtAddCallback(k->w, XmNarmCallback,    SwapShadowsCB, NULL);
        XtAddCallback(k->w, XmNdisarmCallback, SwapShadowsCB, NULL);
    }
    else
    {
        XtVaSetValues(k->w, XmNshadowThickness, 0, NULL);
    }
    
    EquipButtonKnob(k);
    
    return 0;
}

static int CreateArrowLT(DataKnob k, CxWidget parent)
{
    return CreateArrowKnob(k, parent, XmARROW_LEFT);
}

static int CreateArrowRT(DataKnob k, CxWidget parent)
{
    return CreateArrowKnob(k, parent, XmARROW_RIGHT);
}

static int CreateArrowUP(DataKnob k, CxWidget parent)
{
    return CreateArrowKnob(k, parent, XmARROW_UP);
}

static int CreateArrowDN(DataKnob k, CxWidget parent)
{
    return CreateArrowKnob(k, parent, XmARROW_DOWN);
}

static void DoButtonColorization(DataKnob k, knobstate_t newstate, double curv)
{
  button_privrec_t *me     = (button_privrec_t *)k->privptr;
  Pixel             fg;
  Pixel             bg;
  Boolean           is_lit = (((int)(curv)) & CX_VALUE_LIT_MASK) != 0;
  
    MotifKnobs_ChooseColors(k->colz_style, newstate,
                            k->deffg, k->defbg, &fg, &bg);
    if (is_lit) fg = XhGetColor(me->lit_idx);
    XtVaSetValues(CNCRTZE(k->w),
                  XmNforeground, fg,
                  XmNbackground, bg,
                  NULL);
}

static void ButtonSetValue_m(DataKnob k, double v)
{
  button_privrec_t *me         = (button_privrec_t *)k->privptr;
  Boolean           is_lit     = (((int)v) & CX_VALUE_LIT_MASK)      != 0;
  Boolean           is_enabled = (((int)v) & CX_VALUE_DISABLED_MASK) == 0;
  
    if (is_lit != me->cur_lit)
    {
        DoButtonColorization(k, k->curstate, v);
        me->cur_lit = is_lit;
    }

    if (XtIsSensitive (CNCRTZE(k->w)) != is_enabled)
        XtSetSensitive(CNCRTZE(k->w),    is_enabled);
}

static void ButtonColorize_m(DataKnob k, knobstate_t newstate)
{
    DoButtonColorization(k, newstate, k->u.k.curv);
}

dataknob_knob_vmt_t motifknobs_button_vmt  =
{
    {DATAKNOB_KNOB, "button",
        sizeof(button_privrec_t), text2buttonopts,
        0,
        CreateButtonKnob, MotifKnobs_CommonDestroy_m,
        ButtonColorize_m, NULL},
    ButtonSetValue_m, NULL
};

dataknob_knob_vmt_t motifknobs_arrowlt_vmt =
{
    {DATAKNOB_KNOB, "arrowlt",
        sizeof(button_privrec_t), text2buttonopts,
        0,
        CreateArrowLT, MotifKnobs_CommonDestroy_m,
        ButtonColorize_m, NULL},
    ButtonSetValue_m, NULL
};

dataknob_knob_vmt_t motifknobs_arrowrt_vmt =
{
    {DATAKNOB_KNOB, "arrowrt",
        sizeof(button_privrec_t), text2buttonopts,
        0,
        CreateArrowRT, MotifKnobs_CommonDestroy_m,
        ButtonColorize_m, NULL},
    ButtonSetValue_m, NULL
};

dataknob_knob_vmt_t motifknobs_arrowup_vmt =
{
    {DATAKNOB_KNOB, "arrowup",
        sizeof(button_privrec_t), text2buttonopts,
        0,
        CreateArrowUP, MotifKnobs_CommonDestroy_m,
        ButtonColorize_m, NULL},
    ButtonSetValue_m, NULL
};

dataknob_knob_vmt_t motifknobs_arrowdn_vmt =
{
    {DATAKNOB_KNOB, "arrowdn",
        sizeof(button_privrec_t), text2buttonopts,
        0,
        CreateArrowDN, MotifKnobs_CommonDestroy_m,
        ButtonColorize_m, NULL},
    ButtonSetValue_m, NULL
};
