#ifndef _XmInputOnlyP_h
#define _XmInputOnlyP_h


#include "InputOnly.h"
#include <Xm/PrimitiveP.h>

#ifdef __cplusplus
extern "C" {
#endif

/* InputOnly class structure */
typedef struct _XmInputOnlyClassPart
{
    XtPointer  extension;
} XmInputOnlyClassPart;

/* Full class record declaration for InputOnly class */
typedef struct _XmInputOnlyClassRec
{
    CoreClassPart            core_class;
    XmPrimitiveClassPart     primitive_class;
    XmInputOnlyClassPart     inputonly_class;
} XmInputOnlyClassRec;

externalref XmInputOnlyClassRec xmInputOnlyClassRec;

/* InputOnly instance record */
typedef struct _XmInputOnlyPart
{
    void *dummy;
} XmInputOnlyPart;

/* Full instance record declaration */
typedef struct _XmInputOnlyRec
{
    CorePart            core;
    XmPrimitivePart     primitive;
    XmInputOnlyPart     inputonly;
} XmInputOnlyRec;

/********    Private Function Declarations    ********/


/********    End Private Function Declarations    ********/

#ifdef __cplusplus
}  /* Close scope of 'extern "C"' declaration which encloses file. */
#endif


#endif /* _XmInputOnlyP_h */
