#ifndef __MOTIF_KNOBSP_H
#define __MOTIF_KNOBSP_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <stdio.h>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>

#include "paramstr_parser.h"

#include "cx_common_types.h"
#include "datatree.h"


#define CNCRTZE(w) ((Widget)  w)
#define ABSTRZE(w) ((CxWidget)w)


extern psp_lkp_t        MotifKnobs_colors_lkp[];
extern psp_lkp_t        MotifKnobs_sizes_lkp [];
extern psp_paramdescr_t text2_MotifKnobs_containeropts[];


/* Standard/default knobs' methods */

void MotifKnobs_CommonDestroy_m (DataKnob k);
void MotifKnobs_CommonColorize_m(DataKnob k, knobstate_t newstate);
void MotifKnobs_NoColorize_m    (DataKnob k, knobstate_t newstate);

void MotifKnobs_CommonShowAlarm_m(DataKnob k, int onoff);
void MotifKnobs_CommonAckAlarm_m (DataKnob k);
void MotifKnobs_CommonAlarmNewData_m(DataKnob k, int synthetic); /*!!! hack -- because need to CycleAlarm somehow... */


/* Common layout parameters */

enum
{
    MOTIFKNOBS_INTERELEM_H_SPACING = 10,
    MOTIFKNOBS_INTERELEM_V_SPACING = 10,
    MOTIFKNOBS_INTERKNOB_H_SPACING = 2,
    MOTIFKNOBS_INTERKNOB_V_SPACING = 2
};


/* Style management */

enum
{
    MKS_MONO_FN = 1 << 0,  // Monospaced (fixed-width) font
    MKS_PROP_FN = 1 << 1,  // Proportional font
    MKS_NORM_FG = 1 << 2,  // Foreground for general widgets
    MKS_NORM_BG = 1 << 3,  // Background for ~
    MKS_INPT_FG = 1 << 4,  // Foreground for input (text, dial, etc.) widgets
    MKS_INPT_BG = 1 << 5,  // Background for ~
    MKS_CTL3_FG = 1 << 6,  // Foreground for CTL3D widgets
    MKS_CTL3_BG = 1 << 7,  // Background for ~
};

int  MotifKnobs_ParseStyle(DataKnob k,
                           Arg al[], int *ac,
                           int mask);

typedef struct
{
    int  fontsize;
    int  fg_idx;
    int  bg_idx;
    int  lit_idx;
    int  bg3d_idx;
    int  bgrc_idx;
} MotifKnobs_style_spec_t;
typedef struct
{
    MotifKnobs_style_spec_t inhrt;  // Inherited from parent
    MotifKnobs_style_spec_t curnt;  // Specified for current
    MotifKnobs_style_spec_t cntnt;  // Specified for content
} MotifKnobs_style_trpl_t;


/* Color management */

void MotifKnobs_ChooseColors(int colz_style, knobstate_t newstate,
                             CxPixel  deffg,  CxPixel  defbg,
                             Pixel   *newfg,  Pixel   *newbg);
void MotifKnobs_ChooseKnobCs(DataKnob k,  knobstate_t newstate,
                             Pixel   *newfg,  Pixel   *newbg);

void MotifKnobs_AllocStateGCs(Widget w, GC table, int norm_idx);


/* User interface */

void MotifKnobs_Mouse3Handler(Widget     w,
                              XtPointer  closure,
                              XEvent    *event,
                              Boolean   *continue_to_dispatch);
void MotifKnobs_HookButton3  (DataKnob  k, Widget w);

void MotifKnobs_DecorateKnob (DataKnob  k);

int  MotifKnobs_IsDoubleClick(Widget   w,
                              XEvent  *event,
                              Boolean *continue_to_dispatch);


/* SetControlValue interface */

void MotifKnobs_SetControlValue(DataKnob k, double v, int fromlocal);


/* Common-textfield interface */

enum
{
    MOD_FINEGRAIN_MASK = ControlMask,
    MOD_BIGSTEPS_MASK  = Mod1Mask,
    MOD_NOSEND_MASK    = ShiftMask
};

Widget MotifKnobs_MakeTextWidget (Widget parent, Boolean rw, int columns);
Widget MotifKnobs_CreateTextValue(DataKnob k, Widget parent);
Widget MotifKnobs_CreateTextInput(DataKnob k, Widget parent);
void   MotifKnobs_SetTextString  (DataKnob k, Widget w, double v);

void   MotifKnobs_SetTextCursorCallback(Widget w);
int    MotifKnobs_ExtractDoubleValue   (Widget w, double *result);
int    MotifKnobs_ExtractIntValue      (Widget w, int    *result);


/* Common "container" interface */

enum
{
    MOTIFKNOBS_TITLE_SIDE_TOP    = 0,
    MOTIFKNOBS_TITLE_SIDE_BOTTOM = 1,
    MOTIFKNOBS_TITLE_SIDE_LEFT   = 2,
    MOTIFKNOBS_TITLE_SIDE_RIGHT  = 3,
};

enum
{
    MOTIFKNOBS_CF_NOSHADOW = 1 << 0,
    MOTIFKNOBS_CF_NOTITLE  = 1 << 1,
    MOTIFKNOBS_CF_NOHLINE  = 1 << 2,
    MOTIFKNOBS_CF_NOFOLD   = 1 << 3,
    MOTIFKNOBS_CF_FOLDED   = 1 << 4,
    MOTIFKNOBS_CF_FOLD_H   = 1 << 5,
    MOTIFKNOBS_CF_SEPFOLD  = 1 << 6,
    MOTIFKNOBS_CF_TOOLBAR  = 1 << 7,
    MOTIFKNOBS_CF_ATTITLE  = 1 << 8,
    MOTIFKNOBS_CF_TITLE_SIDE_shift = 9,
    MOTIFKNOBS_CF_TITLE_SIDE_MASK  = 3 << MOTIFKNOBS_CF_TITLE_SIDE_shift,
    MOTIFKNOBS_CF_TITLE_ATTOP      = MOTIFKNOBS_TITLE_SIDE_TOP    << MOTIFKNOBS_CF_TITLE_SIDE_shift,
    MOTIFKNOBS_CF_TITLE_ATBOTTOM   = MOTIFKNOBS_TITLE_SIDE_BOTTOM << MOTIFKNOBS_CF_TITLE_SIDE_shift,
    MOTIFKNOBS_CF_TITLE_ATLEFT     = MOTIFKNOBS_TITLE_SIDE_LEFT   << MOTIFKNOBS_CF_TITLE_SIDE_shift,
    MOTIFKNOBS_CF_TITLE_ATRIGHT    = MOTIFKNOBS_TITLE_SIDE_RIGHT  << MOTIFKNOBS_CF_TITLE_SIDE_shift,
};

typedef Widget (* motifknobs_cpopulator_t)(DataKnob k, CxWidget parent, void *privptr);

int    MotifKnobs_CreateContainer(DataKnob k, CxWidget parent,
                                  motifknobs_cpopulator_t populator, void *privptr,
                                  int flags);
int    MotifKnobs_PopulateContainerAttitle(DataKnob k, int nattl, int title_side);

typedef struct
{
    int   title_side;
    int   noshadow;  
    int   notitle;   
    int   nohline;   
    int   nofold;    
    int   folded;    
    int   fold_h;    
    int   sepfold;   
} MotifKnobs_containeropts_t;

/* ((( TMP: v2-style button style support */
enum
{
    TUNE_BUTTON_KNOB_F_NO_PIXMAP = 1 << 0,
    TUNE_BUTTON_KNOB_F_NO_FONT   = 1 << 1,
};
void MotifKnobs_TuneButtonKnob(DataKnob k, CxWidget w, int flags);

void MotifKnobs_TuneTextKnob  (DataKnob k, CxWidget w, int flags);

/* ))) TMP: v2-style button style support */


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __MOTIF_KNOBSP_H */
