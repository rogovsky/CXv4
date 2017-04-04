#ifndef _XmIncDecBP_h
#define _XmIncDecBP_h

#include "IncDecB.h"
#include <Xm/PrimitiveP.h>

#ifdef __cplusplus
extern "C" {
#endif

/* IncDecButton class structure */
typedef struct _XmIncDecButtonClassPart
{
    XtPointer  extension;
} XmIncDecButtonClassPart;

/* Full class record declaration for IncDecButton class */
typedef struct _XmIncDecButtonClassRec
{
    CoreClassPart            core_class;
    XmPrimitiveClassPart     primitive_class;
    XmIncDecButtonClassPart  incdecbutton_class;
} XmIncDecButtonClassRec;

externalref XmIncDecButtonClassRec xmIncDecButtonClassRec;

/* IncDecButton instance record */
typedef struct _XmIncDecButtonPart
{
    Widget        client_widget;
    Pixel         arm_color;
    int           initial_delay;
    int           repeat_delay;
    
    int           armed_parts;
    KeyCode       key_up;
    KeyCode       key_dn;
    GC            normal_gc;
    GC            insensitive_gc;
    GC            background_gc;
    GC            fill_gc;
    XtIntervalId  timer;
    unsigned int  click_state;
} XmIncDecButtonPart;

/* Full instance record declaration */
typedef struct _XmIncDecButtonRec
{
    CorePart            core;
    XmPrimitivePart     primitive;
    XmIncDecButtonPart  incdecbutton;
} XmIncDecButtonRec;

/********    Private Function Declarations    ********/


/********    End Private Function Declarations    ********/

#ifdef __cplusplus
}  /* Close scope of 'extern "C"' declaration which encloses file. */
#endif

#endif /* _XmIncDecBP_h */
