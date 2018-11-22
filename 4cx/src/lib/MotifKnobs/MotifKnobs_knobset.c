#include "datatreeP.h"
#include "KnobsP.h"

#include "MotifKnobs_text_knob.h"
#include "MotifKnobs_selector_knob.h"
#include "MotifKnobs_choicebs_knob.h"
#include "MotifKnobs_alarmonoffled_knob.h"
#include "MotifKnobs_button_knob.h"
#include "MotifKnobs_text_text.h"
#include "MotifKnobs_text_vect.h"
#include "MotifKnobs_matrix_vect.h"
#include "MotifKnobs_label_noop.h"
#include "MotifKnobs_separator_noop.h"
#include "MotifKnobs_histplot_noop.h"
#include "MotifKnobs_grid_cont.h"
#include "MotifKnobs_subwin_cont.h"
#include "MotifKnobs_split_cont.h"
#include "MotifKnobs_scroll_cont.h"
#include "MotifKnobs_tabber_cont.h"
#include "MotifKnobs_canvas_cont.h"
#include "MotifKnobs_loggia_cont.h"
#include "MotifKnobs_invisible_cont.h"
#include "MotifKnobs_lrtb_grpg.h"


static knobs_knobset_t Motif_knobset =
{
    (dataknob_unif_vmt_t *[]){
        (dataknob_unif_vmt_t *)&motifknobs_def_knob_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_text_knob_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_selector_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_choicebs_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_alarm_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_onoff_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_led_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_radiobtn_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_button_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_arrowlt_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_arrowrt_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_arrowup_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_arrowdn_vmt,

        (dataknob_unif_vmt_t *)&motifknobs_def_text_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_text_text_vmt,

        (dataknob_unif_vmt_t *)&motifknobs_text_vect_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_def_vect_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_matrix_vect_vmt,

        (dataknob_unif_vmt_t *)&motifknobs_def_noop_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_hlabel_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_vlabel_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_hseparator_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_vseparator_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_histplot_vmt,

        (dataknob_unif_vmt_t *)&motifknobs_def_cont_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_grid_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_subwin_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_split_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_scroll_vmt,
#if XM_TABBER_AVAILABLE
        (dataknob_unif_vmt_t *)&motifknobs_tabber_vmt,
#endif
        (dataknob_unif_vmt_t *)&motifknobs_loggia_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_invisible_vmt,

        (dataknob_unif_vmt_t *)&motifknobs_rectangle_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_frectangle_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_ellipse_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_fellipse_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_polygon_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_fpolygon_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_polyline_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_pipe_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_string_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_canvas_vmt,

        (dataknob_unif_vmt_t *)&motifknobs_def_grpg_vmt,
        (dataknob_unif_vmt_t *)&motifknobs_lrtb_vmt,
        
        NULL
    },
    NULL
};

knobs_knobset_t *first_knobset_ptr = &Motif_knobset;
