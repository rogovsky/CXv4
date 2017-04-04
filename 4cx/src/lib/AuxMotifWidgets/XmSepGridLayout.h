#ifndef _XmSepGridLayout_h
#define _XmSepGridLayout_h

#include <Xm/Xm.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Resource strings */
#define XmNresFlag	"resFlag"
#define XmCResFlag	"ResFlag"
#define XmNrows 	"rows"
#define XmCRows 	"Rows"
#define XmNcolumns 	"columns"
#define XmCColumns 	"Columns"
#define XmNmarginHeight	"marginHeight"
#define XmCMarginHeight "MarginHeight"
#define XmNmarginWidth	"marginWidth"
#define XmCMarginWidth  "MarginWidth"
#define XmNsepHType 	"sepHType"
#define XmCSepHType 	"SepHType"
#define XmNsepVType 	"sepVType"
#define XmCSepVType 	"SepVType"
#define XmNsepHeight	"sepHeight"
#define XmCSepHeight	"SepHeight"
#define XmNsepWidth	"sepWidth"
#define XmCSepWidth	"SepWidth"
#define XmNcellX    	"cellX"
#define XmCCellX	"CellX"
#define XmNcellY    	"cellY"
#define XmCCellY	"CellY"
#define XmNfillType	"fillType"
#define XmCFillType	"FillType"
#define XmNcellGravity  "gravity" 
#define XmCCellGravity  "Gravity" 
#define XmNprefHeight	"prHeight"
#define XmCPrefHeight	"PrHeight"
#define XmNprefWidth	"prWidth"
#define XmCPrefWidth	"PrWidth"

/* Class record constants */

externalref WidgetClass xmSepGridLayoutWidgetClass;

typedef struct _XmSepGridLayoutClassRec *XmSepGridLayoutWidgetClass;
typedef struct _XmSepGridLayoutRec *XmSepGridLayoutWidget;

/* Public Functions Declarations */

extern Widget XmCreateSepGridLayout(
		    Widget p,
		    String name,
		    ArgList args,
		    Cardinal n);
extern void SGLBlock(XmSepGridLayoutWidget w);
extern void SGLUnBlock(XmSepGridLayoutWidget w);
/* End Public Functions Declarations */

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* _XmSepGridLayout_h */
