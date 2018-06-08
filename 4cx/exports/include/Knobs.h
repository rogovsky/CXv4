#ifndef __KNOBS_H
#define __KNOBS_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "cx_common_types.h"
#include "datatree.h"


enum
{
    SIMPLEKNOB_OPT_READONLY = 1 << 1,
};


/* Main interface */

int       CreateKnob   (DataKnob k, CxWidget parent);
void      DestroyKnob  (DataKnob k);


/* Simple-knobs API */

typedef void (*simpleknob_cb_t)(DataKnob k, double v, void *privptr);

DataKnob  CreateSimpleKnob(const char      *spec,     // Knob specification
                           int              options,
                           CxWidget         parent,   // Parent widget
                           simpleknob_cb_t  cb,       // Callback proc
                           void            *privptr); // Pointer to pass to cb

void      SetSimpleKnobValue(DataKnob k, double v);
void      SetSimpleKnobState(DataKnob k, knobstate_t newstate);

CxWidget  GetKnobWidget(DataKnob k);


/* Error descriptions */
char *KnobsLastErr(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __KNOBS_H */
