#ifndef __DATATREE_H
#define __DATATREE_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <unistd.h>

#include "cx.h"
#include "misc_sepchars.h"


typedef struct _data_knob_t_struct   *DataKnob;
typedef struct _data_subsys_t_struct *DataSubsys;


enum
{
    KNOBS_USERINACTIVITYLIMIT = 60, // Seconds before text inputs turn back to autoupdate
    KNOBS_RELAX_PERIOD        = 10, // Seconds for green "relax" bg
    KNOBS_OTHEROP_PERIOD      = 5,  // Seconds for orange "other op" bg
};

enum
{
    HISTPLOT_REASON_UPDATE_PLOT_GRAPH = 0,
    HISTPLOT_REASON_UPDATE_PROT_PROPS = 1,
};


// Filename patterns
#define DATATREE_MODE_PATTERN_FMT "mode_%s_*.dat"
#define DATATREE_DATA_PATTERN_FMT "data_%s_*.dat"
#define DATATREE_PLOT_PATTERN_FMT "plot_%s_*.dat"
#define DATATREE_OPS_PATTERN_FMT  "events_%s.log"


// Standard commands
#define DATATREE_STDCMD_FREEZE "datatreeFreeze"


typedef void (*knob_editstate_hook_t)(DataKnob k, int state);
void set_knob_editstate_hook(knob_editstate_hook_t hook);

typedef void (*_m_histplot_update)(void *privptr, int reason);


/*
 *  Note: when adding/changing states, the
 *        lib/datatree/datatree.c::_datatree_knobstates_list[]
 *        must be updated coherently.
 */

typedef enum
{
    KNOBSTATE_UNINITIALIZED = 0,  // Uninitialized
    KNOBSTATE_JUSTCREATED,        // Just created
    KNOBSTATE_NONE,               // Normal
    KNOBSTATE_YELLOW,             // Value in yellow zone
    KNOBSTATE_RED,                // Value in red zone
    KNOBSTATE_WEIRD,              // Value is weird
    KNOBSTATE_DEFUNCT,            // Defunct channel
    KNOBSTATE_HWERR,              // Hardware error
    KNOBSTATE_SFERR,              // Software error
    KNOBSTATE_NOTFOUND,           // Channel not found
    KNOBSTATE_RELAX,              // Relaxing after alarm
    KNOBSTATE_OTHEROP,            // Other operator is active
    KNOBSTATE_PRGLYCHG,           // Programmatically changed

    KNOBSTATE__DUMMY_,            // For KNOBSTATE_LAST only
    
    KNOBSTATE_FIRST = KNOBSTATE_UNINITIALIZED,
    KNOBSTATE_LAST  = KNOBSTATE__DUMMY_ - 1
} knobstate_t;

rflags_t    choose_knob_rflags(DataKnob k, rflags_t rflags, double new_v);

knobstate_t choose_knobstate(DataKnob k, rflags_t rflags);
void        set_knobstate   (DataKnob k, knobstate_t newstate);
void        set_knob_relax  (DataKnob k, int onoff);
void        set_knob_otherop(DataKnob k, int onoff);

char *strknobstate_short(knobstate_t state);
char *strknobstate_long (knobstate_t state);

void *find_knobs_nearest_upmethod(DataKnob k, int vmtoffset);

int  set_knob_controlvalue(DataKnob k, double      v, int fromlocal);
int  set_knob_textvalue   (DataKnob k, const char *s, int fromlocal);
void show_knob_alarm      (DataKnob k, int onoff);
void ack_knob_alarm       (DataKnob k);
void show_knob_props      (DataKnob k);
void show_knob_bigval     (DataKnob k);
void show_knob_histplot   (DataKnob k);
void express_hist_interest(DataKnob k,
                           _m_histplot_update proc, void *privptr,
                           int set1_fgt0);
int  historize_knob       (DataKnob k);

void user_began_knob_editing(DataKnob k);
void cancel_knob_editing    (DataKnob k);

void store_knob_undo_value  (DataKnob k);
void perform_knob_undo      (DataKnob k);

int  get_knob_label(DataKnob k, char **label_p);
int  get_knob_tip  (DataKnob k, char **tip_p);

void get_knob_rowcol_label_and_tip(const char *ms, int n,
                                   DataKnob    model_k,
                                   char       *lbuf, size_t lbufsize,
                                   char       *tbuf, size_t tbufsize,
                                   char      **label_p,
                                   char      **tip_p);

typedef struct
{
    int         type;
    int         count;
    const char *data_p;
    double      offset;
    double      multiplier;
    char        format[200];
} datatree_items_prv_t;
int  get_knob_items_count(const char *items, datatree_items_prv_t *r);
int  get_knob_item_n     (datatree_items_prv_t *r,
                          int n, char *buf, size_t bufsize,
                          char **tip_p, char **style_p);

DataSubsys get_knob_subsys(DataKnob k);

DataKnob datatree_find_node(DataKnob current, const char *name, int rqd_type);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __DATATREE_H */
