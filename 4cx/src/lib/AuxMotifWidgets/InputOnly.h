#ifndef _XmInputOnly_h
#define _XmInputOnly_h


#include <Xm/Xm.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XmIsInputOnly
#define XmIsInputOnly(w) XtIsSubclass (w, xmInputOnlyWidgetClass)
#endif /* XmIsInputOnly */

externalref WidgetClass xmInputOnlyWidgetClass;

typedef struct _XmInputOnlyClassRec *XmInputOnlyWidgetClass;
typedef struct _XmInputOnlyRec      *XmInputOnlyWidget;

/********    Public Function Declarations    ********/

extern Widget XmCreateInputOnly(Widget parent,
                                char *name,
                                ArgList arglist,
                                Cardinal argcount);

/********    End Public Function Declarations    ********/

#ifdef __cplusplus
}  /* Close scope of 'extern "C"' declaration which encloses file. */
#endif


#endif /* _XmInputOnly_h */
