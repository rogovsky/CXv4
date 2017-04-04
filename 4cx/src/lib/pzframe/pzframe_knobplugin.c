
#include "misclib.h"
#include "paramstr_parser.h"

#include "Xh.h"
#include "Knobs.h"
#include "KnobsP.h"      // For Knobs_wdi_*
#include "datatreeP.h"
#include "MotifKnobsP.h" // For ABSTRZE()/CNCRTZE()

#include "pzframe_data.h"
#include "pzframe_gui.h"
#include "pzframe_knobplugin.h"


int      PzframeKnobpluginDoCreate(DataKnob k, CxWidget parent,
                                   pzframe_knobplugin_realize_t  kp_realize,
                                   pzframe_gui_t *gui, pzframe_gui_dscr_t *gkd,
                                   psp_paramdescr_t *table2, void *rec2,
                                   psp_paramdescr_t *table4, void *rec4,
                                   psp_paramdescr_t *table7, void *rec7)
{
  pzframe_knobplugin_t *kpn = k->privptr;

  cda_context_t         present_cid;
  DataSubsys            sys = get_knob_subsys(k);
  char                  curbase[CDA_PATH_MAX];
  char                 *curbptr;

  XhWindow              win;

  void             *recs  [] =
  {
//      &(gui->pfr->cfg.param_iv),
      rec2,
      &(gui->pfr->cfg),
      rec4,
      &(gui->look),
      // Here was v2's opts -- #6
      rec7,
  };
  psp_paramdescr_t *tables[] =
  {
//      gkd->ftd->specific_params,
      table2,
      pzframe_data_text2cfg,
      table4,
      pzframe_gui_text2look,
      // Here was v2's opts -- #6
      table7,
  };

    /* Note: gui should be already inited by caller */

    bzero(kpn, sizeof(*kpn));
    kpn->g = gui;

    /* Parse parameters */
    if (psp_parse_v(k->options, NULL,
                    recs, tables, countof(tables),
                    Knobs_wdi_equals_c, Knobs_wdi_separators, Knobs_wdi_terminators,
                    0) != PSP_R_OK)
    {
        fprintf(stderr, "%s %s[%s/\"%s\"].options: %s\n",
                strcurtime(), __FUNCTION__, k->ident, k->label, psp_error());
        goto FAILURE;
    }

    /* Obey readonlyness */
    gui->pfr->cfg.readonly = (sys != NULL  &&  sys->readonly);

    present_cid = (sys != NULL)? sys->cid : CDA_CONTEXT_ERROR;
    curbptr = cda_combine_base_and_spec(present_cid,
                                        k->u.z.tree_base, k->u.z.src,
                                        curbase, sizeof(curbase));

    if ((win = XhWindowOf(parent)) != NULL  &&
        XhGetWindowAlarmleds(win) != NULL)
        gui->look.noleds = 1;

    /*!!! No "constr" -- rely on realize()! */
    if (kp_realize != NULL  &&
        kp_realize(kpn, k) < 0) ;

    /* Create widgets */
    if (gui->d->vmt.realize != NULL  &&
        gui->d->vmt.realize(gui,
                            CNCRTZE(parent),
                            present_cid, curbptr) < 0) goto FAILURE;
    k->w = gui->top;

    return 0;

 FAILURE:
    k->w = PzframeKnobpluginRedRectWidget(parent);
    return -1;
}


CxWidget PzframeKnobpluginRedRectWidget(CxWidget parent)
{
    return ABSTRZE(XtVaCreateManagedWidget("", widgetClass, CNCRTZE(parent),
                                           XmNwidth,      20,
                                           XmNheight,     10,
                                           XmNbackground, XhGetColor(XH_COLOR_JUST_RED),
                                           NULL));
}

int      PzframeKnobpluginHandleCmd_m(DataKnob k, const char *cmd, int info_int)
{
  pzframe_knobplugin_t *kpn = k->privptr;

    if      (strcmp(cmd, DATATREE_STDCMD_FREEZE) == 0)
    {
        PzframeDataSetRunMode(kpn->g->pfr, !info_int, -1);
    }

    return 0;
}

//////////////////////////////////////////////////////////////////////

enum {ALLOC_INC = 1};

// GetCblistSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Cblist, pzframe_knobplugin_cb_info_t,
                                 cb, cb, NULL, (void*)1,
                                 0, ALLOC_INC, 0,
                                 kpn->, kpn,
                                 pzframe_knobplugin_t *kpn, pzframe_knobplugin_t *kpn);

static void PzframeKnobpluginEventProc(pzframe_gui_t *gui,
                                       int            reason,
                                       int            info_changed,
                                       void          *privptr)
{
  pzframe_knobplugin_t *kpn = privptr;
  int                         i;

//if (reason == 1000) fprintf(stderr, "%s() REPER_CHANGE\n", __FUNCTION__);
    for (i = 0;  i < kpn->cb_list_allocd;  i++)
        if (kpn->cb_list[i].cb != NULL)
            kpn->cb_list[i].cb(kpn->g->pfr, reason,
                               info_changed, kpn->cb_list[i].privptr);
}


/*
    Note:
        Here we suppose a proper "inheritance", so that
        k->widget_private will point to pzframe_knobplugin_t object
        (which is, usually, at the beginning of an offspring-type object).
 */
pzframe_data_t
    *PzframeKnobpluginRegisterCB(DataKnob                     k,
                                 const char                  *rqd_type_name,
                                 pzframe_knobplugin_evproc_t  cb,
                                 void                        *privptr,
                                 const char                 **name_p)
{
  pzframe_knobplugin_t         *kpn;
  pzframe_data_t               *pfr;
  int                           id;
  pzframe_knobplugin_cb_info_t *slot;

    if (k == NULL) return NULL;

    /* Access knobplugin... */
    kpn = k->privptr;
    if (kpn == NULL) return NULL;

    /* ...and pzframe */
    pfr = kpn->g->pfr;

    /* Check type */
    if (strcasecmp(pfr->ftd->type_name, rqd_type_name) != 0)
        return NULL;

    /* "cb==NULL" means "just return pointer to pzframe" */
    if (cb == NULL) return pfr;

    /* Perform registration */

    /* First check if we already imposed a callback to data */
    if (kpn->low_level_registered == 0)
    {
        if (PzframeGuiAddEvproc(kpn->g, PzframeKnobpluginEventProc, kpn) < 0)
            return NULL;
        kpn->low_level_registered = 1;
    }

    id = GetCblistSlot(kpn);
    if (id < 0) return NULL;
    slot = AccessCblistSlot(id, kpn);

    slot->cb      = cb;
    slot->privptr = privptr;

    if (name_p != NULL) *name_p = k->label;

    return pfr;
}

pzframe_gui_t
    *PzframeKnobpluginGetGui    (DataKnob                     k,
                                 const char                  *rqd_type_name)
{
  pzframe_knobplugin_t         *kpn;
  pzframe_data_t               *pfr;

    if (k == NULL) return NULL;

    /* Access knobplugin... */
    kpn = k->privptr;
    if (kpn == NULL) return NULL;

    /* ...and pzframe */
    pfr = kpn->g->pfr;

    /* Check type */
    if (strcasecmp(pfr->ftd->type_name, rqd_type_name) != 0)
        return NULL;

    return kpn->g;
}
