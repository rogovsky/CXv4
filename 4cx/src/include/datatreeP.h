#ifndef __DATATREEP_H
#define __DATATREEP_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <time.h>

#include "cx_common_types.h"
#include "paramstr_parser.h"

#include "datatree.h"


/* Possible knob types */
enum
{
    DATAKNOB_GRPG = 1,
    DATAKNOB_CONT = 2,
    DATAKNOB_NOOP = 3,
    DATAKNOB_KNOB = 4,
    DATAKNOB_BIGC = 5,
    DATAKNOB_TEXT = 6,
    DATAKNOB_USER = 7,
    DATAKNOB_PZFR = 8,
};

/* Standard knobs' parameters */
enum
{
    DATAKNOB_PARAM_STEP     = 0,
    DATAKNOB_PARAM_SAFEVAL  = 1,
    DATAKNOB_PARAM_GRPCOEFF = 2,
    DATAKNOB_PARAM_RSRVD1   = 3,
    DATAKNOB_PARAM_ALWD_MIN = 4,
    DATAKNOB_PARAM_ALWD_MAX = 5,
    DATAKNOB_PARAM_NORM_MIN = 6,
    DATAKNOB_PARAM_NORM_MAX = 7,
    DATAKNOB_PARAM_YELW_MIN = 8,
    DATAKNOB_PARAM_YELW_MAX = 9,
    DATAKNOB_PARAM_DISP_MIN = 10,
    DATAKNOB_PARAM_DISP_MAX = 11,
    DATAKNOB_NUM_STD_PARAMS = 12
};

/* Knob behaviour flags */
enum
{
    DATAKNOB_B_IS_BUTTON      = 1 << 0,
    DATAKNOB_B_IGN_OTHEROP    = 1 << 1,
    DATAKNOB_B_IS_LIGHT       = 1 << 2,
    DATAKNOB_B_IS_ALARM       = 1 << 3,
    DATAKNOB_B_IS_SELECTOR    = 1 << 4,

    DATAKNOB_B_IS_GROUPABLE   = 1 << 31,
    
    DATAKNOB_B_NO_WASJUSTSET  = 1 << 30,
    DATAKNOB_B_STEP_FXD       = 1 << 29, /* Change of "STEP" is forbidden */
    DATAKNOB_B_RANGES_FXD     = 1 << 28, /* Change of ranges is forbidden */

    DATAKNOB_B_UNSAVABLE      = 1 << 27, /* Do NOT save in modes */

    DATAKNOB_B_ON_UPDATE      = 1 << 26, /* Update upon data arrival, not at-cycle */
};

/* Knob colorization styles */
enum
{
    DATAKNOB_COLZ_NORMAL    = 0,
    DATAKNOB_COLZ_HILITED   = 1,
    DATAKNOB_COLZ_IMPORTANT = 2,
    DATAKNOB_COLZ_VIC       = 3,
    DATAKNOB_COLZ_DIM       = 4,
    DATAKNOB_COLZ_HEADING   = 5,
};

/* Knob kinds */
enum
{
    DATAKNOB_KIND_READ   = 1000,
    DATAKNOB_KIND_DEVN   = 1001,
    DATAKNOB_KIND_MINMAX = 1002,

    DATAKNOB_KIND_WRITE  = 2000
};

enum
{
    DATAKNOB_STRSBHVR_IDENT   = 1 << 0,
    DATAKNOB_STRSBHVR_LABEL   = 1 << 1,
    DATAKNOB_STRSBHVR_TIP     = 1 << 2,
    DATAKNOB_STRSBHVR_COMMENT = 1 << 3,
    DATAKNOB_STRSBHVR_GEOINFO = 1 << 4,
    DATAKNOB_STRSBHVR_RSRVD6  = 1 << 5,
    DATAKNOB_STRSBHVR_UNITS   = 1 << 6,
    DATAKNOB_STRSBHVR_DPYFMT  = 1 << 7,
};

typedef struct
{
    /* Subelement-specific */
    const char  *refbase;
    DataSubsys   subsyslink;
    
    /* Content */
    struct _data_knob_t_struct
                *content;
    int          count;
    int          param1;
    int          param2;
    int          param3;
    const char  *str1;
    const char  *str2;
    const char  *str3;

    /* "Code" to execute upon initialization */
    char        *at_init_src;
    CxDataRef_t  at_init_ref;

    /* "Code" to execute upon-process ("ap" - At Process-time */
    char        *ap_src;
    CxDataRef_t  ap_ref;
    
    /* Decorations */
    CxWidget     caption;    // Caption/title
    CxWidget     compact;    // A compact form -- only visible when container is folded
    CxWidget     toolbar;    // Holder to the left of title
    CxWidget     attitle;    // Holder to the right of title
    CxWidget     hline;      // Separator between the caption and innage
    CxWidget     innage;     // The main "content"

    /* Alarm handling */
    int          alarm_on;
    int          alarm_acked;
    int          alarm_flipflop;
    cx_time_t    alarm_prev_time;
} dataknob_cont_data_t;

typedef struct
{
    dataknob_cont_data_t  c;
} dataknob_grpg_data_t;

typedef struct
{
} dataknob_noop_data_t;

typedef struct
{
    int            kind;            // DATAKNOB_KIND_{READ,DEVN,MINMAX,WRITE}

    char          *units;
    char           dpyfmt[16];
    char          *items;

    char          *rd_src;
    char          *wr_src;
    char          *cl_src;
    
    CxDataRef_t    rd_ref;
    CxDataRef_t    wr_ref;
    CxDataRef_t    cl_ref;

    CxKnobParam_t *params;
    int            num_params;

    /* Current state */
    double         curv;
    double         curcolv;
    double         q;
    double         userval;
    int            userval_valid;
    CxAnyVal_t     curv_raw;
    cxdtype_t      curv_raw_dtype;

    double         undo_val;
    int            undo_val_saved;

    int            is_ingroup;
    
    union
    {
        struct  // For DATAKNOB_KIND_DEVN;
        {
            int     notnew;
            double  avg;
            double  avg2;
        } devn;
        struct  // For DATAKNOB_KIND_MINMAX
        {
            double *buf;
            int     bufcap;
            int     bufused;
        } minmax;
    }              kind_var;

    /* Historization */
    DataKnob       hist_prev;
    DataKnob       hist_next;
    double        *histring;        // History ring buffer
    int            histring_len;    // Ring buffer length
    int            histring_start;  // The oldest point
    int            histring_used;   // Number of points in the buffer
} dataknob_knob_data_t;

typedef struct
{
    CxDataRef_t  bigch;
} dataknob_bigc_data_t;

typedef struct
{
    char          *src;

    CxDataRef_t    ref;
} dataknob_text_data_t;

typedef struct
{
    CxDataRef_t  dr;
    int          is_rw;
    double       curv;
    rflags_t     rflags;
} dataknob_user_data_rec_t;

typedef struct
{
    CxDataRef_t  br;
    rflags_t     rflags;
} dataknob_user_bigc_rec_t;

typedef struct
{
    dataknob_user_data_rec_t *datarecs;
    int                       num_datarecs;
    dataknob_user_bigc_rec_t *bigcrecs;
    int                       num_bigcrecs;
} dataknob_user_data_t;

typedef struct
{
    char          *tree_base;
    char          *src;
    CxDataRef_t    _devstate_ref;
} dataknob_pzfr_data_t;


typedef int  (*_k_create_f)    (DataKnob k, CxWidget parent);
typedef void (*_k_destroy_f)   (DataKnob k);
typedef void (*_k_colorize_f)  (DataKnob k, knobstate_t newstate);
typedef int  (*_k_handlecmd_f) (DataKnob k, const char *cmd, int info_int);

typedef void (*_k_sndphys_f)   (DataKnob k, double v);
typedef void (*_k_sndtext_f)   (DataKnob k, const char *s);
typedef void (*_k_showprops_f) (DataKnob k);
typedef void (*_k_showbigval_f)(DataKnob k);
typedef void (*_k_tohistplot_f)(DataKnob k);
typedef void (*_k_showalarm_f) (DataKnob k, int onoff);
typedef void (*_k_ackalarm_f)  (DataKnob k);
typedef void (*_k_newdata_f)   (DataKnob k, int synthetic);
typedef void (*_k_setvis_f)    (DataKnob k, int vis_state, DataKnob child);
typedef void (*_k_chldprpchg_f)(DataKnob k, int child_idx, DataKnob old_k);
typedef int  (*_k_histintrst_f)(DataKnob k,
                                _m_histplot_update proc, void *privptr,
                                int set1_fgt0);
typedef int  (*_k_historize_f) (DataKnob k);

typedef void (*_k_setvalue_f)  (DataKnob k, double v);
typedef void (*_k_propschg_f)  (DataKnob k, DataKnob old_k);

typedef void (*_k_settextv_f)  (DataKnob k, const char *s);

typedef struct
{
    int               type;
    const char       *name;

    size_t            privrec_size;
    psp_paramdescr_t *options_table;

    int               ref_count;
    
    _k_create_f       Create;
    _k_destroy_f      Destroy;
    _k_colorize_f     Colorize;
    _k_handlecmd_f    HandleCmd;
} dataknob_unif_vmt_t;

typedef struct
{
    dataknob_unif_vmt_t  unif;

    _k_sndphys_f         SndPhys;
    _k_sndtext_f         SndText;
    _k_showprops_f       ShowProps;
    _k_showbigval_f      ShowBigVal;
    _k_tohistplot_f      ToHistPlot;
    _k_showalarm_f       ShowAlarm;
    _k_ackalarm_f        AckAlarm;
    _k_newdata_f         NewData;
    _k_setvis_f          SetVis;
    _k_chldprpchg_f      ChildPropsChg;
    _k_histintrst_f      HistInterest;
    _k_historize_f       Historize;
} dataknob_cont_vmt_t;

typedef struct
{
    dataknob_cont_vmt_t  c;
} dataknob_grpg_vmt_t;

typedef struct
{
    dataknob_unif_vmt_t  unif;
} dataknob_noop_vmt_t;

typedef struct
{
    dataknob_unif_vmt_t  unif;

    _k_setvalue_f        SetValue;
    _k_propschg_f        PropsChg;
} dataknob_knob_vmt_t;

typedef struct
{
    dataknob_unif_vmt_t  unif;
} dataknob_bigc_vmt_t;

typedef struct
{
    dataknob_unif_vmt_t  unif;

    _k_settextv_f        SetText;
} dataknob_text_vmt_t;

typedef struct
{
    dataknob_unif_vmt_t  unif;
} dataknob_user_vmt_t;

typedef struct
{
    dataknob_unif_vmt_t  unif;
} dataknob_pzfr_vmt_t;

typedef struct _data_knob_t_struct
{
    int         type;       // DATAKNOB_{GRPG,ELEM,NOOP,KNOB,BIGC,USER}
    int         is_rw;
    int         behaviour;
    int         colz_style;
    const char *look;
    const char *options;    // Knob-dependent parameters

    /* Description */
    const char *ident;      // Identifier -- knob's "name", "handle"
    const char *label;      // Label to be displayed
    const char *tip;        // Text to be displayed in a tooltip
    const char *comment;    // Comment -- freeform for humans
    const char *style;      // For CSS-like style management
    const char *layinfo;    // Layout info -- for parent container
    const char *geoinfo;    // Geographical location -- probably GIS info
    int         strsbhvr;   // Strings behaviour (bitmask): "1" means "unspecified, should be found elsewhere")

    /* Type-specific */
    union
    {
        dataknob_grpg_data_t   g;
        dataknob_cont_data_t   c;
        dataknob_noop_data_t   n;
        dataknob_knob_data_t   k;
        dataknob_bigc_data_t   b;
        dataknob_text_data_t   t;
        dataknob_user_data_t   u;
        dataknob_pzfr_data_t   z;
    } u;

    /* Vital activity */
    rflags_t    currflags;
    cx_time_t   timestamp;
    knobstate_t curstate;

    time_t      attn_endtime;    // time() when the relaxation or other "attention" should end, or 0
    knobstate_t attn_state;      // Knob state which started at attn_time

    time_t      usertime;        // time() of the last user activity
    int         wasjustset;      // Was just set by user, so that one update should be skipped
    int         being_modified;

    /* Existence */
    dataknob_unif_vmt_t
               *vmtlink;    // Virtual Methods Table
    DataKnob    uplink;     // Link to holder
    CxWidget    w;          // Widget
    CxPixel     deffg;      // Default (normal state) foreground
    CxPixel     defbg;      // Default (normal state) background
    void       *privptr;    // Specific-knob private
    void       *upprivptr1; // CB for simple-knobs
    void       *upprivptr2; // privptr for simple-knobs
    
    uint8      *conns_u;    // Array of boolean flags -- connections used by this node
} data_knob_t;


#define DSTN_SYSNAME    "sysname"
#define DSTN_GROUPING   "grouping"
#define DSTN_DEFSERVER  "defserver"
#define DSTN_TREEPLACE  "treeplace"
#define DSTN_WINTITLE   "win_title"
#define DSTN_WINNAME    "win_name"
#define DSTN_WINCLASS   "win_class"
#define DSTN_WINOPTIONS "win_options"
#define DSTN_ARGVWINOPT "argv_win_opt"

typedef struct _data_section_t_struct *DataSection;
typedef void (* _ds_destructor_t)(DataSection section);

typedef struct _data_section_t_struct
{
    const char       *type;  // 'type' field should NOT be freed, since it always points to static memory
    char             *name;
    void             *data;
    _ds_destructor_t  destroy;
} data_section_t;

typedef struct _data_subsys_t_struct
{
    data_section_t *sections;
    int             num_sections;

    // Note: following pointers are just quick-aliases to actual data,
    //       so they should NOT be freed
    const char     *sysname;
    data_knob_t    *main_grouping;
    const char     *defserver;
    const char     *treeplace;

    //
    int             cid; /*!!! cda_context_t */
    int             is_freezed;
    int             readonly;
    rflags_t        currflags; /* added 29.11.2015 */

    //
    void           *leds_ptr;

    //
    dataknob_cont_vmt_t
                   *sys_vmtlink;
    void           *gui_privptr;
    // History management
    int             cyclesize_us;
    int             def_histring_len;
    DataKnob        hist_first;
    DataKnob        hist_last;
} data_subsys_t;


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __DATATREEP_H */
