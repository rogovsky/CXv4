#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/Form.h>
#include <Xm/Label.h>

#include "misc_macros.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_grid_cont.h"


typedef struct
{
    MotifKnobs_containeropts_t  cont;

    int   nodecor;

    int   one;
    int   norowtitles;
    int   nocoltitles;
    int   transposed;
    int   hspc;
    int   vspc;
    int   colspc;
} grid_privrec_t;

static psp_paramdescr_t text2gridopts[] =
{
    PSP_P_INCLUDE("container",
                  text2_MotifKnobs_containeropts,
                  PSP_OFFSET_OF(grid_privrec_t, cont)),

    PSP_P_FLAG  ("nodecor",     grid_privrec_t, nodecor,     1, 0),

    PSP_P_FLAG  ("one",         grid_privrec_t, one,         1, 0),
    PSP_P_FLAG  ("norowtitles", grid_privrec_t, norowtitles, 1, 0),
    PSP_P_FLAG  ("nocoltitles", grid_privrec_t, nocoltitles, 1, 0),
    PSP_P_FLAG  ("transposed",  grid_privrec_t, transposed,  1, 0),
    PSP_P_INT   ("hspc",        grid_privrec_t, hspc,        -1, 0, 0),
    PSP_P_INT   ("vspc",        grid_privrec_t, vspc,        -1, 0, 0),
    PSP_P_INT   ("colspc",      grid_privrec_t, colspc,      -1, 0, 0),
    PSP_P_END()
};


//// Knobs' position management //////////////////////////////////////

enum
{
    HALIGN_LEFT = 10, HALIGN_RIGHT,  HALIGN_CENTER, HALIGN_FILL, DEF_HALIGN = HALIGN_LEFT,
    VALIGN_TOP  = 20, VALIGN_BOTTOM, VALIGN_CENTER, VALIGN_FILL, DEF_VALIGN = VALIGN_TOP,
};

typedef struct
{
    int  horz;
    int  vert;
} placeopts_t;

static psp_lkp_t horz_place_lkp[] =
{
    {"left",    HALIGN_LEFT},
    {"right",   HALIGN_RIGHT},
    {"center",  HALIGN_CENTER},
    {"fill",    HALIGN_FILL},
    {NULL, 0}
};

static psp_lkp_t vert_place_lkp[] =
{
    {"top",     VALIGN_TOP},
    {"bottom",  VALIGN_BOTTOM},
    {"center",  VALIGN_CENTER},
    {"fill",    VALIGN_FILL},
    {NULL, 0}
};

static psp_paramdescr_t text2placeopts[] =
{
    PSP_P_LOOKUP("horz", placeopts_t, horz, DEF_HALIGN, horz_place_lkp),
    PSP_P_LOOKUP("vert", placeopts_t, vert, DEF_VALIGN, vert_place_lkp),
    PSP_P_END()
};

static void PlaceKnob(DataKnob ck, int x, int y, int is_normal)
{
  const char  *spec;
  placeopts_t  placeopts;
  int          halign;
  int          valign;
  int          hfill;
  int          vfill;

    bzero(&placeopts, sizeof(placeopts));
    spec = is_normal? ck->layinfo : NULL;
    if (psp_parse(spec, NULL,
                  &placeopts,
                  Knobs_wdi_equals_c, Knobs_wdi_separators, Knobs_wdi_terminators,
                  text2placeopts) != PSP_R_OK)
    {
        fprintf(stderr, "Knob %s/\"%s\".layinfo: %s\n",
                ck->ident, ck->label, psp_error());
        bzero(&placeopts, sizeof(placeopts));
        placeopts.horz = DEF_HALIGN;
        placeopts.vert = DEF_VALIGN;
    }

    switch (placeopts.horz)
    {
        case HALIGN_LEFT:   halign = -1; hfill = 0; break;
        case HALIGN_RIGHT:  halign = +1; hfill = 0; break;
        case HALIGN_CENTER: halign =  0; hfill = 0; break;
        case HALIGN_FILL:   halign =  0; hfill = 1; break;
        default:            halign = -1; hfill = 0; break;
    }

    switch (placeopts.vert)
    {
        case VALIGN_TOP:    valign = -1; vfill = 0; break;
        case VALIGN_BOTTOM: valign = +1; vfill = 0; break;
        case VALIGN_CENTER: valign =  0; vfill = 0; break;
        case VALIGN_FILL:   valign =  0; vfill = 1; break;
        default:            valign = -1; vfill = 0; break;
    }

    XhGridSetChildPosition (ck->w, x,      y);
    XhGridSetChildAlignment(ck->w, halign, valign);
    XhGridSetChildFilling  (ck->w, hfill,  vfill);
}

static void sethv(int *h, int *v, int x, int y, int transposed)
{
    if (!transposed)
    {
        *h = x;
        *v = y;
    }
    else
    {
        *h = y;
        *v = x;
    }
}


//// Labels management ///////////////////////////////////////////////

static void LabelClickHandler(Widget     w,
                              XtPointer  closure,
                              XEvent    *event,
                              Boolean   *continue_to_dispatch)
{
  DataKnob        k = (DataKnob) closure;
  grid_privrec_t *me = (grid_privrec_t *)k->privptr;
  XtPointer       user_data;
  int             col;

  int             my_x;
  int             my_y;

  WidgetList      children;
  Cardinal        numChildren;
  Cardinal        i;
  int             its_x;
  int             its_y;
  
  Widget          victims[200]; /* 200 seems to be a fair limit; in unlimited case we'll need to XtMalloc() */
  int             count;

  char          lbuf[1000];
  char          tbuf[1000];
  char         *ls;
  char         *tip;
  XmString      s;

    if (!MotifKnobs_IsDoubleClick(w, event, continue_to_dispatch)) return;

    /* Obtain parameters */
    XtVaGetValues(w, XmNuserData, &user_data, NULL);
    col = ptr2lint(user_data);
    
    XhGridGetChildPosition(ABSTRZE(w), &my_x, &my_y);
    XtVaGetValues(XtParent(w),
                  XmNchildren,    &children,
                  XmNnumChildren, &numChildren,
                  NULL);

    /* Collect a list of children to be switch-managed */
    for (i = 0,               count = 0;
         i < numChildren  &&  count < countof(victims);
         i++)
    {
        XhGridGetChildPosition(ABSTRZE(children[i]), &its_x, &its_y);
        if (children[i] != w  &&  its_x == my_x)
            victims[count++] = children[i];
    }

    if (count == 0) return; /*!?What can it be?!*/

    /* Switch managedness */
    if (XtIsManaged(victims[0]))
    {
        XtUnmanageChildren(victims, count);
        ls = "!";
    }
    else
    {
        get_knob_rowcol_label_and_tip(me->transposed? k->u.c.str2 : k->u.c.str1, col,
                                      k->u.c.content + k->u.c.param3 + col,
                                      lbuf, sizeof(lbuf), tbuf, sizeof(tbuf),
                                      &ls, &tip);
        XtManageChildren  (victims, count);
    }
    
    XtVaSetValues(w,
                  XmNlabelString, s = XmStringCreateLtoR(ls, xh_charset),
                  NULL);
    XmStringFree(s);
}

/*
 *  make_label
 *      Creates a column- or row-label,
 *      + appropriately assigning it a label-string and a tooltip
 *        (both either from the element description or from an
 *        appropriate source-knob);
 *      + positions and aligns the label as requested;
 *      + optionally adds some space to the label's left margin;
 *      + optionally sets label's style to be same as model_k's
 *      + optionally adds a double-click callback
 *
 *  Args:
 *      ctnr           the container knob
 *      name           resource name for the widget
 *      ms             multistring which (probably) contains the label-string
 *      n              index in the above multistring
 *      model_k        the 1st knob in row/col (source of label-string, tip, and style)
 *      alias_to_knob  whether to colorize the label as source-knob
 *      loffset        space to add to marginLeft
 *      x,y            position in parent_grid
 *      h_a,v_a        horizontal and vertical alignment (-1/0/+1)
 *      add_lch        add a label-click handler which folds column upon double-click
 */

static void make_label(DataKnob    ctnr,
                       const char *name,
                       const char *ms,      int      n,
                       DataKnob    model_k, int      alias_to_knob,
                       int         loffset,
                       int         x,       int      y,
                       int         h_a,     int      v_a,
                       int         add_lch)
{
  Widget        l;
  XmString      s;
  char          lbuf[1000];
  char          tbuf[1000];
  char         *ls;
  char         *tip;
  Dimension     lmargin;
  
    /* Obtain strings first... */
    get_knob_rowcol_label_and_tip(ms, n, model_k,
                                  lbuf, sizeof(lbuf), tbuf, sizeof(tbuf),
                                  &ls, &tip);

    /* Create a widget */
    l = XtVaCreateManagedWidget(name,
                                xmLabelWidgetClass,
                                CNCRTZE(ctnr->u.c.innage),
                                XmNlabelString, s = XmStringCreateLtoR(ls, xh_charset),
                                NULL);
    XmStringFree(s);

    /* Assign a tip, if any */
    if (tip != NULL  &&  *tip != '\0')
        XhSetBalloon(ABSTRZE(l), tip);

    /* Position the thingie */
    if (loffset > 0)
    {
        XtVaGetValues(l, XmNmarginLeft, &lmargin, NULL);
        lmargin += loffset;
        XtVaSetValues(l, XmNmarginLeft, lmargin,  NULL);
    }
    
    XhGridSetChildPosition (ABSTRZE(l), x,   y);
    XhGridSetChildFilling  (ABSTRZE(l), 0,   0);
    XhGridSetChildAlignment(ABSTRZE(l), h_a, v_a);

    /*  */
    if (alias_to_knob)
    {
        /*!!! should perform "style" trickery here  */

        /* Redirect mouse-click on label to knob */
        MotifKnobs_HookButton3(model_k, l)/*, fprintf(stderr, "%s: %s->%s\n", __FUNCTION__, ls, model_k->ident)*/;
    }

    /*  */
    if (add_lch)
    {
        XtVaSetValues    (l, XmNuserData, lint2ptr(n), NULL);
        XtAddEventHandler(l, ButtonPressMask, False,
                          LabelClickHandler, (XtPointer)ctnr);
    }
}


//// Main -- creator /////////////////////////////////////////////////

static Widget GridMaker(DataKnob k, CxWidget parent, void *privptr)
{
  grid_privrec_t *me = (grid_privrec_t *)k->privptr;
  CxWidget        grid;

    grid = XhCreateGrid(parent, "grid");
    XhGridSetGrid   (grid, -1*0,     -1*0);
    XhGridSetSpacing(grid, me->hspc, me->vspc);
    XhGridSetPadding(grid,  1*0,      1*0);
  
    return CNCRTZE(grid);
}

static Widget FormMaker(DataKnob k, CxWidget parent, void *privptr)
{
    return XtVaCreateManagedWidget("form",
                                   xmFormWidgetClass,
                                   CNCRTZE(parent),
                                   XmNshadowThickness, 0,
                                   NULL);
}

static int CreateGridCont(DataKnob k, CxWidget parent, int is_normal)
{
  grid_privrec_t *me = (grid_privrec_t *)k->privptr;

  DataKnob        kl;
  int             ncols;    // # of columns
  int             nflrs;    // # of floors
  int             nattl;    // # at title
  int             ngrid;    // # in grid body (=count-nattl)
  int             nrows;    // Total # of floors (in all col-sections/"porches")
  int             nlanes;   // # of lanes/col-sections/"porches"

  DataKnob        ck;
  
  int             x;
  int             y;
  
  int             x0;
  int             y0;

  int             gx;
  int             gy;
  
  int             eh;  // Effective Horz.
  int             ev;  // Effective Vert.
  const char     *collabel_name;
  int             collabel_halign;
  int             collabel_valign;
  const char     *rowlabel_name;
  int             rowlabel_halign;
  int             rowlabel_valign;
  int             rowlabel_ofs;

    if (me->nodecor) me->cont.noshadow = me->cont.notitle = me->cont.nohline =
        me->nocoltitles = me->norowtitles = 1;

    /* Set default/safe values... */
    ncols    = 0;
    nflrs    = 0;
    nattl    = 0;

    /* ...and override them from specification, if allowed */
    if (is_normal)
    {
        ncols    = k->u.c.param1;
        nflrs    = k->u.c.param2;
        nattl    = k->u.c.param3;
    }

    /* Sanitize parameters... */
    if (ncols <= 0)           ncols = 1;
    if (me->cont.notitle)     nattl = 0;
    if (nattl < 0)            nattl = 0;
    if (nattl > k->u.c.count) nattl = k->u.c.count;
    ngrid = k->u.c.count   - nattl;
    kl    = k->u.c.content + nattl;
    /* and options */
    if (!is_normal)   me->norowtitles = me->nocoltitles = 1;
    if (me->hspc   < 0  ||  !is_normal) me->hspc   = MOTIFKNOBS_INTERKNOB_H_SPACING;
    if (me->vspc   < 0  ||  !is_normal) me->vspc   = MOTIFKNOBS_INTERKNOB_V_SPACING;
    if (me->colspc < 0  ||  !is_normal) me->colspc = MOTIFKNOBS_INTERELEM_H_SPACING;
    if (ngrid != 1)                     me->one    = 0;
    /* Store for use by LabelClickHandler() */
    k->u.c.param3 = nattl;

    /* Calculate grid geometry */
    nrows = (ngrid + ncols - 1) / ncols;
    if (nflrs <= 0  ||  nflrs > nrows) nflrs = nrows;
    if (nflrs == 0) nflrs = 1;
    nlanes = nrows / nflrs;

    /* Create a container... */
    if (MotifKnobs_CreateContainer(k, parent,
                                   me->one? FormMaker : GridMaker,
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

    MotifKnobs_PopulateContainerAttitle(k, nattl, me->cont.title_side);

    /* ...and populate it */
    x0 = me->norowtitles? 0 : 1;
    y0 = me->nocoltitles? 0 : 1;
    
    sethv(&eh, &ev, nlanes * (x0 + ncols), nflrs + y0, me->transposed);
    XhGridSetSize(k->u.c.innage, eh, ev);

    sethv(&collabel_halign, &rowlabel_halign,  0, -1, me->transposed);
    sethv(&collabel_valign, &rowlabel_valign, +1, -1, me->transposed);
    if (!me->transposed)
    {
        collabel_name   = "collabel";
        rowlabel_name   = "rowlabel";
        rowlabel_ofs    = me->colspc;
    }
    else
    {
        collabel_name   = "rowlabel";
        rowlabel_name   = "collabel";
        rowlabel_ofs    = 0;
    }

    if (me->one)
    {
        if (CreateKnob(kl, k->u.c.innage) < 0) return -1;
        attachleft  (kl->w, NULL, 0);
        attachright (kl->w, NULL, 0);
        attachtop   (kl->w, NULL, 0);
        attachbottom(kl->w, NULL, 0);

        return 0;
    }
    
    for (y = 0, ck = kl;
         y < nrows;
         y++)
        for (x = 0;
             x < ncols  &&  y*ncols+x < ngrid;
             x++, ck++)
        {
            gx = x0 + x + (y / nflrs) * (ncols + x0);
            gy = y0 + (y % nflrs);

            /* Rowlabel? */
            sethv(&eh, &ev, gx - 1, gy, me->transposed);
            if (me->norowtitles == 0  &&  x == 0)
                make_label(k,
                           rowlabel_name,
                           k->u.c.str2, y,
                           ck, 1,
                           y < nflrs? 0 : rowlabel_ofs,
                           eh, ev,
                           rowlabel_halign, rowlabel_valign,
                           me->transposed);

            /* Collabel? */
            sethv(&eh, &ev, gx, gy - 1, me->transposed);
            if (me->nocoltitles == 0  &&  gy == y0)
                make_label(k,
                           collabel_name,
                           k->u.c.str1, x,
                           ck, 1,
                           0,
                           eh, ev,
                           collabel_halign, collabel_valign,
                           !me->transposed);

            /* Knob tself */
            if (CreateKnob(ck, k->u.c.innage) < 0) return -1;
            sethv(&eh, &ev, gx, gy, me->transposed);
            PlaceKnob(ck, eh, ev, is_normal);
        }
    
    return 0;
}

static int CreateNormalGrid(DataKnob k, CxWidget parent)
{
    return CreateGridCont(k, parent, 1);
}

static int CreateDefCont   (DataKnob k, CxWidget parent)
{
    return CreateGridCont(k, parent, 0);
}


dataknob_cont_vmt_t motifknobs_grid_vmt =
{
    {DATAKNOB_CONT, "grid",
        sizeof(grid_privrec_t), text2gridopts,
        0,
        CreateNormalGrid, MotifKnobs_CommonDestroy_m, NULL, NULL},
    NULL, NULL, NULL, NULL, NULL,
    MotifKnobs_CommonShowAlarm_m, MotifKnobs_CommonAckAlarm_m, 
    MotifKnobs_CommonAlarmNewData_m, NULL, NULL, NULL
};

dataknob_cont_vmt_t motifknobs_def_cont_vmt =
{
    {DATAKNOB_CONT, DEFAULT_KNOB_LOOK_NAME,
        sizeof(grid_privrec_t), text2gridopts,
        0,
        CreateDefCont, MotifKnobs_CommonDestroy_m, NULL, NULL},
    NULL, NULL, NULL, NULL, NULL,
    MotifKnobs_CommonShowAlarm_m, MotifKnobs_CommonAckAlarm_m,
    MotifKnobs_CommonAlarmNewData_m, NULL, NULL, NULL
};
