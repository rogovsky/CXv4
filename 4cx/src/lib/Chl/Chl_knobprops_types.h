#ifndef __CHL_KNOBPROPS_TYPES_H
#define __CHL_KNOBPROPS_TYPES_H


enum {NUM_PROP_FLAGS = 17}; /*!!! log2(CDA_FLAG_CALCERR) + 1 */
enum {MAX_DPY_PARAMS = 10};

enum
{
    KPROPS_TXT_SHD   = 0,
    KPROPS_TXT_INP   = 1,
    KPROPS_TXT_COUNT = 2,

    KPROPS_MNX_MIN   = 0,
    KPROPS_MNX_MAX   = 1,
    KPROPS_MNX_COUNT = 2,
};
typedef struct
{
    int       editable;
    Widget    form;
    Widget    txt[KPROPS_TXT_COUNT][KPROPS_MNX_COUNT];
    Widget    dash;
    // ??? modification flagging?
} kprops_range_t;

typedef struct
{
    int            initialized;
    DataSubsys     subsys; // Backreference, for Update1HZProc()

    XtIntervalId   upd_timer;

    Widget         box;
    Widget         pth_dpy;
    Widget         nam_lbl;
    Widget         lbl_lbl;
    Widget         com_lbl;
    Widget         uni_lbl;
    Widget         val_dpy;
    Widget         raw_dpy;
    Widget         rds_dpy;
    Widget         tsp_dpy;
    Widget         age_dpy;
    Widget         rflags_leds[NUM_PROP_FLAGS];
    Widget         col_dpy;
    Widget         src_dpy;
    Widget         wrt_dpy;
    Widget         clz_dpy;

    kprops_range_t r[4];
    int            is_closing;

    Widget         stp_inp;
    Widget         gcf_inp;

    Widget         prm_none;
    CxWidget       prm_grid;
    Widget         prm_idn[MAX_DPY_PARAMS];
    Widget         prm_lbl[MAX_DPY_PARAMS];
    Widget         prm_inp[MAX_DPY_PARAMS];

    double         mins[4];
    double         maxs[4];

    struct
    {
        int        ranges[4];
        int        stp;
        int        gcf;
        int        prm[MAX_DPY_PARAMS];
    }              chg;
    
    DataKnob       k;
    rflags_t       rflags_shown;
    knobstate_t    knobstate_shown;
    Pixel          col_deffg;
    Pixel          col_defbg;
    
} knobprops_rec_t;


#endif /* __CHL_KNOBPROPS_TYPES_H */
