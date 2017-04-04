#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/Form.h>
#include <Xm/Label.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>

#include "misc_macros.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "Xh_fallbacks.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_choicebs_knob.h"


typedef struct
{
    int     is_compact;
    int     no_label;
    int     alignment;

    Widget *items;
    int     numitems;
    int     cur_active;
    int     dlyd_clrz;

    int     defcols_obtained;
    Pixel   b_deffg;
    Pixel   b_defbg;
} choicebs_privrec_t;

static psp_lkp_t alignment_lkp[] =
{
    {"left",    XmALIGNMENT_BEGINNING},
    {"right",   XmALIGNMENT_END},
    {"center",  XmALIGNMENT_CENTER},
    {NULL, 0}
};


static psp_paramdescr_t text2choicebsopts[] =
{
    PSP_P_FLAG  ("compact",     choicebs_privrec_t, is_compact,  1, 0),
    PSP_P_FLAG  ("nolabel",     choicebs_privrec_t, no_label,    1, 0),

    PSP_P_LOOKUP("align",       choicebs_privrec_t, alignment,
                 XmALIGNMENT_BEGINNING, alignment_lkp),

    PSP_P_END()
};



static void SetActiveItem(DataKnob k, int v)
{
  choicebs_privrec_t *me   = (choicebs_privrec_t *)k->privptr;
  Pixel               fg;
  Pixel               bg;

    if (v < 0  ||  v >= me->numitems) v = -1;
    if (me->cur_active == v) return;
    if (me->cur_active >= 0)
    {
        XhInvertButton(ABSTRZE(me->items[me->cur_active]));
        if (me->dlyd_clrz)
        {
            me->dlyd_clrz = 0;
            MotifKnobs_ChooseColors(k->colz_style, k->curstate,
                                    me->b_deffg, me->b_defbg,
                                    &fg, &bg);
            XtVaSetValues(me->items[me->cur_active],
                          XmNforeground, fg,
                          XmNbackground, bg,
                          NULL);
        }
    }
    me->cur_active = v;
    if (me->cur_active >= 0) XhInvertButton(ABSTRZE(me->items[me->cur_active]));
}

static void ActivateCB(Widget     w,
                       XtPointer  closure,
                       XtPointer  call_data __attribute__((unused)))
{
  DataKnob            k  = (DataKnob)closure;
  choicebs_privrec_t *me = (choicebs_privrec_t *)(k->privptr);

  int         y;
    
    /* Mmmm...  We have to walk through the whole list to find *which* one... */
    for (y = 0;  y < me->numitems; y++)
    {
        if (me->items[y] == w)
        {
            if (k->is_rw)
                MotifKnobs_SetControlValue(k, y, 1);
            else
            {
                /* Handle readonly choicebs */
                if (y != (int)(k->u.k.curv))
                    SetActiveItem(k, k->u.k.curv);
            }
            return;
        }
    }

    /* Hmmm...  Nothing found?!?!?! */
    fprintf(stderr, "%s::%s: no widget found!\n", __FILE__, __FUNCTION__);
}


typedef struct
{
    int  fgcidx;
    int  armidx;
    int  size;
    int  bold;
} itemstyle_t;

static psp_paramdescr_t text2itemstyle[] =
{
    PSP_P_LOOKUP("color",  itemstyle_t, fgcidx, -1, MotifKnobs_colors_lkp),
    PSP_P_LOOKUP("lit",    itemstyle_t, armidx, -1, MotifKnobs_colors_lkp),
    PSP_P_LOOKUP("size",   itemstyle_t, size,   -1, MotifKnobs_sizes_lkp),
    PSP_P_FLAG  ("bold",   itemstyle_t, bold,    1, 0),
    PSP_P_FLAG  ("medium", itemstyle_t, bold,    0, 1),
    PSP_P_END()
};

static int CreateChoicebsKnob(DataKnob k, CxWidget parent)
{
  choicebs_privrec_t *me = (choicebs_privrec_t *)k->privptr;

  XmString               s;

  char                  *lp;

  Widget                 form;
  Widget                 prev;
  Widget                 label_w;
  Widget                 b;

  int                    y;
  datatree_items_prv_t   irec;
  char                   l_buf[200];
  char                  *l_buf_p;
  char                  *tip_p;
  char                  *style_p;
  int                    disabled;
  itemstyle_t            istyle;
  int                    r;
  char                   fontspec[100];

  Dimension              shd_thk;
  Dimension              mrg_hgt;

  Dimension              hth;
  Dimension              mrw;
  Dimension              mrh;
  
    k->behaviour |= DATAKNOB_B_STEP_FXD;

    me->cur_active = -1;

    /* Obtain number of items and allocate them */
    me->numitems = get_knob_items_count(k->u.k.items, &irec);
    if (me->numitems < 0)  return -1;
    me->items = (void *)(XtCalloc(me->numitems, sizeof(Widget)));
    if (me->items == NULL) return -1;
    
    /* Create a form... */
    form = XtVaCreateManagedWidget("choiceForm", xmFormWidgetClass, CNCRTZE(parent),
                                   XmNshadowThickness, 0,
                                   XmNtraversalOn, k->is_rw,
                                   NULL);

    /* ...and populate it */
    prev = NULL;

    /* a. label */
    if (!me->no_label) me->no_label = !get_knob_label(k, &lp);
    if (!me->no_label)
    {
        label_w = XtVaCreateManagedWidget("rowlabel", xmLabelWidgetClass, form,
                                          XmNlabelString, s = XmStringCreateLtoR(lp, xh_charset),
                                          NULL);
        XmStringFree(s);
        //MaybeAssignPixmap(label_w, lp, 0);
        attachleft(label_w, NULL, 0);
        attachtop (label_w, NULL, 0);
        MotifKnobs_HookButton3(k, label_w);
        
        prev = label_w;
    }

    /* b. items */
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

        me->items[y] = b =
            XtVaCreateManagedWidget(k->is_rw? "push_i" : "push_o",
                                    xmPushButtonWidgetClass,
                                    form,
                                    XmNlabelString, s = XmStringCreateLtoR(l_buf_p, xh_charset),
                                    XmNalignment,   me->alignment,
                                    XmNtraversalOn, False,
                                    XmNarmColor,    XhGetColor(XH_COLOR_BG_TOOL_ARMED),
                                    NULL);
        XmStringFree(s);
        //MaybeAssignPixmap(me->items[y], l_buf_p, 0);
        /*!!!STYLE!!! (((*/
        if (istyle.fgcidx > 0)
            XtVaSetValues(b, XmNforeground, XhGetColor(istyle.fgcidx), NULL)/*, fprintf(stderr, "fgcidx=%d\n", istyle.fgcidx)*/;
        if (istyle.armidx > 0)
            XtVaSetValues(b, XmNarmColor,   XhGetColor(istyle.armidx), NULL);
        if (istyle.size   > 0)
        {
            check_snprintf(fontspec, sizeof(fontspec),
                           "-*-" XH_FONT_PROPORTIONAL_FMLY "-%s-r-*-*-%d-*-*-*-*-*-*-*",
                           istyle.bold? XH_FONT_BOLD_WGHT : XH_FONT_MEDIUM_WGHT,
                           istyle.size);
            XtVaSetValues(b,
                          XtVaTypedArg, XmNfontList, XmRString, fontspec, strlen(fontspec)+1,
                          NULL);
        }
        /*!!!STYLE!!! )))*/
        if (disabled) XtSetSensitive(b, False);
        if (k->is_rw)
            XtAddCallback(b, XmNactivateCallback, ActivateCB, (XtPointer)k);
        else
        {
            XtVaGetValues(b,
                          XmNshadowThickness, &shd_thk,
                          XmNmarginHeight,    &mrg_hgt,
                          NULL);
            XtVaSetValues(b,
                          XmNshadowThickness, 0,
                          XmNmarginHeight,    mrg_hgt + shd_thk,
                          NULL);
        }

        if (tip_p != NULL  &&  *tip_p != '\0')
            XhSetBalloon(ABSTRZE(b), tip_p);

        XtVaGetValues(b,
                      XmNhighlightThickness, &hth,
                      XmNmarginWidth,        &mrw,
                      XmNmarginHeight,       &mrh,
                      NULL);
        XtVaSetValues(b,
                      XmNhighlightThickness, 0,
                      XmNmarginWidth,        mrw + hth,
                      XmNmarginHeight,       mrh + hth,
                      NULL);
        
        if (!me->is_compact)
        {
            attachtop     (b, NULL, 0);
            attachbottom  (b, NULL, 0);
            if (prev != NULL)
                attachleft(b, prev, 0);
        }
        else
        {
            attachleft    (b, NULL, 0);
            attachright   (b, NULL, 0);
            if (prev != NULL)
                attachtop (b, prev, 0);
        }

        MotifKnobs_HookButton3(k, b);

        prev = b;
    }

    k->w = ABSTRZE(form);

    MotifKnobs_DecorateKnob(k);
    
    return 0;
}

static void DestroyChoicebsKnob(DataKnob k)
{
  choicebs_privrec_t *me = (choicebs_privrec_t *)k->privptr;

    MotifKnobs_CommonDestroy_m(k);
    XtFree((void *)(me->items));
}

static void ChoicebsSetValue_m(DataKnob k, double v)
{
    SetActiveItem(k, (int)v);
}

static void ChoicebsColorize_m(DataKnob k, knobstate_t newstate)
{
  choicebs_privrec_t *me = (choicebs_privrec_t *)k->privptr;
  Pixel               fg;
  Pixel               bg;
  int                 n;

    if (!me->defcols_obtained)
    {
        XtVaGetValues(me->numitems > 0? me->items[0] : k->w,
                      XmNforeground, &(me->b_deffg),
                      XmNbackground, &(me->b_defbg),
                      NULL);
        
        me->defcols_obtained = 1;
    }

    MotifKnobs_CommonColorize_m(k, newstate);

    MotifKnobs_ChooseColors(k->colz_style, newstate,
                            me->b_deffg, me->b_defbg, &fg, &bg);
    for (n = 0;  n < me->numitems;  n++)
        if (n != me->cur_active)
            XtVaSetValues(me->items[n],
                          XmNforeground, fg,
                          XmNbackground, bg,
                          NULL);
        else
            me->dlyd_clrz = 1;
}

dataknob_knob_vmt_t motifknobs_choicebs_vmt =
{
    {DATAKNOB_KNOB, "choicebs",
        sizeof(choicebs_privrec_t), text2choicebsopts,
        0,
        CreateChoicebsKnob, DestroyChoicebsKnob,
        ChoicebsColorize_m, NULL},
    ChoicebsSetValue_m, NULL
};
