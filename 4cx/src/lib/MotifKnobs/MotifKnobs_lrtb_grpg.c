#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include <Xm/Form.h>

#include "Xh.h"
#include "Xh_globals.h"
#include "datatreeP.h"
#include "Knobs.h"
#include "KnobsP.h"
#include "MotifKnobsP.h"

#include "MotifKnobs_lrtb_grpg.h"


//////////////////////////////////////////////////////////////////////

typedef struct
{
    int  is_vertical;
    int  hspc;
    int  vspc;
} lrtb_privrec_t;

static psp_paramdescr_t text2lrtbopts[] =
{
    PSP_P_FLAG("vertical", lrtb_privrec_t, is_vertical, 1, 0),
    PSP_P_INT ("hspc",     lrtb_privrec_t, hspc,        -1, 0, 0),
    PSP_P_INT ("vspc",     lrtb_privrec_t, vspc,        -1, 0, 0),
    PSP_P_END()
};

//////////////////////////////////////////////////////////////////////

typedef struct
{
    int  newline;
    int  hfill;
    int  vfill;
} placeopts_t;

static psp_paramdescr_t text2placeopts[] =
{
    PSP_P_FLAG("newline", placeopts_t, newline, 1, 0),
    PSP_P_FLAG("hfill",   placeopts_t, hfill,   1, 0),
    PSP_P_FLAG("vfill",   placeopts_t, vfill,   1, 0),
    PSP_P_END()
};

//////////////////////////////////////////////////////////////////////

static int CreateLRTB(DataKnob k, CxWidget parent, int may_use_layinfo)
{
  lrtb_privrec_t *me = (lrtb_privrec_t *)k->privptr;
  int             n;
  DataKnob        ck;
  Widget          prev;
  Widget          line, prevline;
  Widget          ew;
  placeopts_t     placeopts;
  int             force_newline;
  DataKnob        warn_about;

    if (me->hspc < 0  ||  !may_use_layinfo) me->hspc = MOTIFKNOBS_INTERELEM_H_SPACING;
    if (me->vspc < 0  ||  !may_use_layinfo) me->vspc = MOTIFKNOBS_INTERELEM_V_SPACING;
  
    /* The topmost element-form */
    k->w =
      ABSTRZE(XtVaCreateManagedWidget("lrtbForm",
                                      xmFormWidgetClass,
                                      CNCRTZE(parent),
                                      XmNshadowThickness, 0,
                                      NULL));

    for (prev = line = prevline = NULL, force_newline = 0, warn_about = NULL,
         n = 0, ck = k->u.c.content;
         n < k->u.c.count;
         n++,   ck++)
    {
        bzero(&placeopts, sizeof(placeopts));
        if (psp_parse(may_use_layinfo? ck->layinfo : NULL, NULL,
                      &placeopts,
                      Knobs_wdi_equals_c, Knobs_wdi_separators, Knobs_wdi_terminators,
                      text2placeopts) != PSP_R_OK)
        {
            fprintf(stderr, "Knob %s/\"%s\".layinfo: %s\n",
                    ck->ident, ck->label, psp_error());
            bzero(&placeopts, sizeof(placeopts));
        }
        
        /* New-line? */
        if (placeopts.newline  ||  force_newline)
        {
            if (warn_about != NULL)
            {
                fprintf(stderr, "%s(): Knob %s/\"%s\" requests a newline, while %s/\"%s\" had previously employed a %s flag. Such behaviour is unsupported and isn't guaranteed to work.\n",
                        __FUNCTION__,
                        ck->ident,          ck->label,
                        warn_about->ident, warn_about->label,
                        me->is_vertical? "HFILL" : "VFILL");
                warn_about = NULL;
            }

            prevline = line;
            line = NULL;
        }

        /* Should we create a new row/column container? */
        if (line == NULL)
        {
            line = XtVaCreateManagedWidget("container",
                                           xmFormWidgetClass,
                                           CNCRTZE(k->w),
                                           XmNshadowThickness, 0,
                                           NULL);

            if (prevline == NULL)
                me->is_vertical? attachleft(line, NULL,     0)
                               : attachtop (line, NULL,     0);
            else
                me->is_vertical? attachleft(line, prevline, me->hspc)
                               : attachtop (line, prevline, me->vspc);

            me->is_vertical? attachtop (line, NULL, 0)
                           : attachleft(line, NULL, 0);
                               
            prev = NULL;
        }

        /* Create and attach the subknob */
        if (CreateKnob(ck, ABSTRZE(line)) < 0) return -1;

        ew = CNCRTZE(ck->w);
        if (prev == NULL)
            me->is_vertical? attachtop (ew, NULL, 0)
                           : attachleft(ew, NULL, 0);
        else
            me->is_vertical? attachtop (ew, prev, me->vspc)
                           : attachleft(ew, prev, me->hspc);

        me->is_vertical? attachleft(ew, NULL, 0)
                       : attachtop (ew, NULL, 0);

        /* Remember it for future reference... */
        prev = ew;

        /* Strech element+container, if required... */
        /*   a. Horizontally */
        if (placeopts.hfill)
        {
            attachright(ew,    NULL, 0);
            attachright(line,  NULL, 0);
        }
        /*   b. Vertically */
        if (placeopts.vfill)
        {
            attachbottom(ew,   NULL, 0);
            attachbottom(line, NULL, 0);
        }
        /* ...and perform additional "actions" if so */
        if (placeopts.hfill)
        {
            if (me->is_vertical)
                warn_about = ck;
            else
                force_newline = 1;
        }
        /*   b. Vertically */
        if (placeopts.vfill)
        {
            if (me->is_vertical)
                force_newline = 1;
            else
                warn_about = ck;
        }
    }
    
    return 0;
}


static int CreateNormalLRTB(DataKnob k, CxWidget parent)
{
    return CreateLRTB(k, parent, 1);
}

static int CreateDefGrpg   (DataKnob k, CxWidget parent)
{
    return CreateLRTB(k, parent, 0);
}

dataknob_cont_vmt_t motifknobs_lrtb_vmt =
{
    {DATAKNOB_CONT, "lrtb",
        sizeof(lrtb_privrec_t), text2lrtbopts,
        0,
        CreateNormalLRTB, MotifKnobs_CommonDestroy_m, NULL, NULL},
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

dataknob_grpg_vmt_t motifknobs_def_grpg_vmt =
{
    {
        {DATAKNOB_GRPG, DEFAULT_KNOB_LOOK_NAME,
            sizeof(lrtb_privrec_t), NULL,
            0,
            CreateDefGrpg, MotifKnobs_CommonDestroy_m, NULL, NULL},
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
    }
};
