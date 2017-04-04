#ifndef _XmIncDecB_h
#define _XmIncDecB_h

#include <Xm/Xm.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XmIsIncDecButton
#define XmIsIncDecButton(w) XtIsSubclass (w, xmIncDecButtonWidgetClass)
#endif /* XmIsIncDecButton */

#define XmNclientWidget        "clientWidget"
#define XmCClientWidget        "ClientWidget"
    
externalref WidgetClass xmIncDecButtonWidgetClass;

typedef struct _XmIncDecButtonClassRec *XmIncDecButtonWidgetClass;
typedef struct _XmIncDecButtonRec      *XmIncDecButtonWidget;

/********    Public Function Declarations    ********/

extern Widget XmCreateIncDecButton(
                        Widget parent,
                        char *name,
                        ArgList arglist,
                        Cardinal argcount) ;

/********    End Public Function Declarations    ********/

#ifdef __cplusplus
}  /* Close scope of 'extern "C"' declaration which encloses file. */
#endif

#endif /* _XmIncDecB_h */
