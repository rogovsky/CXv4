#ifndef __KNOBSP_H
#define __KNOBSP_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <stdarg.h>

#include "datatreeP.h"


#define DEFAULT_KNOB_LOOK_NAME "-"


extern const char  Knobs_wdi_equals_c; 
extern const char *Knobs_wdi_separators;
extern const char *Knobs_wdi_terminators;


typedef struct _knobs_knobset_t_struct
{
    dataknob_unif_vmt_t            **list;
    struct _knobs_knobset_t_struct  *next;
} knobs_knobset_t;

extern knobs_knobset_t *first_knobset_ptr;

void RegisterKnobset(knobs_knobset_t *knobset);


dataknob_unif_vmt_t *GetKnobLookVMT(DataKnob k);


void KnobsClearErr(void);
void KnobsSetErr  (const char *format, ...)
    __attribute__((format (printf, 1, 2)));
void KnobsVSetErr (const char *format, va_list ap);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __KNOBSP_H */
