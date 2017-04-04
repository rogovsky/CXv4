#include "XmSepGridLayoutP.h"
#include <X11/IntrinsicP.h>
#include <Xm/DrawP.h>
#include <Xm/RepType.h>
#include <Xm/XmStrDefs.h>/*includes Xm.h*/
#include <stdlib.h>
#include <X11/Xmu/Converters.h>

#define SGL_HORIZONTAL 	1
#define SGL_VERTICAL 	0
#define MAX_NUM 	1000
#define MIN_SEP_H	0
#define MIN_SEP_W	0
#define ROWS_DEFAULT 	1
#define COLUMNS_DEFAULT 1
#define MARGINW_DEFAULT 8
#define MARGINH_DEFAULT 8
#define MIN_ADD_WIDTH 	0
#define MIN_ADD_HEIGHT 	0

#define RECOUNT_STAT	0
#define CONST_INIT	0
#define GEOM_MAN	0
#define CH_MAN		0
#define REDISP 		0
#define RESIZE_STAT	0
#define DEBUG_MODE	0
#define SNR		0
#define CON_SetValues	0
#define Recalc_Size_Stat	0
#define RECOUNT_GRID	0
#define ADDW		0
#define ADDH		0
#define SGL_H_STAT	0

//#define MAX_COLUMNS_DEFAULT 
#define SEP_V_TYPE_DEFAULT XmSINGLE_LINE
#define SEP_H_TYPE_DEFAULT XmSINGLE_LINE

/* Useless macros */

#define GetSGLConstraint(w) \
	(&((XmSepGridLayoutConstraintPtr) (w)->core.constraints)->sep_grid_layout)

/********* Static Function Declarations  **********/

static Boolean SetNewResources(Widget cw, Widget nw, Boolean init);

static void SepGridLayoutClassInit(void);

static void Initialize(
			Widget rw, 
			Widget nw, 
			ArgList args, 
			Cardinal *num_args);

static XtGeometryResult GeometryManager(
			Widget instigator,
			XtWidgetGeometry *request,
			XtWidgetGeometry *reply);

static void ChangeManaged(Widget wid);

static void Resize(Widget wid);

static void Redisplay(Widget w, XEvent *event, Region region);

static Boolean SetValues(
			Widget cw,
			Widget rw,
			Widget nw,
			ArgList args,
			Cardinal *num_args);

static void ConstraintInitialize(
			Widget req,
			Widget nw,
			ArgList args,
			Cardinal *num_args);	
static Boolean	ConstraintSetValues(
			register Widget old,
			register Widget ref,
			register Widget new_w,
			ArgList args,
			Cardinal *num_args);
static void GetSeparatorGC(XmSepGridLayoutWidget w);

static short GetSeparatorSize(XmSepGridLayoutWidget w, char orient);

#if 0
static void FixWidget(XmSepGridLayoutWidget m/*, Widget w*/);
#endif

static void InsertChild(Widget w);

static void RecountCoordinates(XmSepGridLayoutWidget par);

static void RecountGrid(XmSepGridLayoutWidget w, int numx, int numy);

static void RecalcSize(XmSepGridLayoutWidget par, Widget w, 
		Dimension* width, Dimension* height);

static  XtGeometryResult
                changeGeometry(XmSepGridLayoutWidget, int, int, int, XtWidgetGeometry *) ;
//static void DeleteChild(Widget child);

/***** End Static Function Declarations  **********/

/* static array */
static int shiftX[] = 
{ -1,0,1,-1,0,1,-1,0,1
};

static int shiftY[] = 
{ -1,-1,-1,0,0,0,1,1,1
};

/* default translation table and action list */

/* Resource definitions for SepGridLayout */

static XtResource resources[] =
{	
    {
	XmNresFlag,
	XmCResFlag,
	XmRBoolean,
	sizeof(Boolean),
	XtOffsetOf(struct _XmSepGridLayoutRec, sep_grid_layout.resize_flag),
	XmRImmediate,
	(XtPointer)True
    },
    {	XmNcolumns,
	XmCColumns,
	XmRShort,
	sizeof (short),
	XtOffsetOf(struct _XmSepGridLayoutRec, sep_grid_layout.num_columns),
	XmRImmediate,
	(XtPointer)1
    },
    {	XmNrows,
	XmCRows,
	XmRShort,
	sizeof (short),
	XtOffsetOf(struct _XmSepGridLayoutRec, sep_grid_layout.num_rows),
	XmRImmediate,
	(XtPointer)1
    },
    {
	XmNmarginHeight,
	XmCMarginHeight,
	XmRShort,
	sizeof(short),
	XtOffsetOf( struct _XmSepGridLayoutRec, sep_grid_layout.margin_height),
	XmRImmediate,
	(XtPointer) 1
    },
    {
	XmNmarginWidth,
	XmCMarginWidth,
	XmRShort,
	sizeof(short),
	XtOffsetOf( struct _XmSepGridLayoutRec, sep_grid_layout.margin_width),
	XmRImmediate,
	(XtPointer) 1
    },
    {
	XmNsepHeight,
	XmCSepHeight,
	XmRShort,
	sizeof(short),
	XtOffsetOf( struct _XmSepGridLayoutRec, sep_grid_layout.sep_height),
	XmRImmediate,
	(XtPointer) 2
    },
    {
	XmNsepWidth,
	XmCSepWidth,
	XmRShort,
	sizeof(short),
	XtOffsetOf( struct _XmSepGridLayoutRec, sep_grid_layout.sep_width),
	XmRImmediate,
	(XtPointer) 2
    },
    {
	XmNsepHType,
	XmCSepHType,
	XmRSeparatorType,
	sizeof (unsigned char),
	XtOffsetOf( struct _XmSepGridLayoutRec, sep_grid_layout.separator_type_h),
	XmRImmediate,
	(XtPointer) XmSHADOW_ETCHED_IN
    },
    {
	XmNsepVType,
	XmCSepVType,
	XmRSeparatorType,
	sizeof (unsigned char),
	XtOffsetOf( struct _XmSepGridLayoutRec, sep_grid_layout.separator_type_v),
	XmRImmediate,
	(XtPointer) XmSHADOW_ETCHED_IN
    },
};

static XtResource constraints[] =
{
    {
	XmNcellX,
	XmCCellX,
	XmRShort,
	sizeof(short),
	XtOffsetOf( struct _XmSepGridLayoutConstraintRec, sep_grid_layout.num_cell_x),
	XmRImmediate,
	(XtPointer)-1
    },
    {
	XmNcellY,
	XmCCellY,
	XmRShort,
	sizeof(short),
	XtOffsetOf( struct _XmSepGridLayoutConstraintRec, sep_grid_layout.num_cell_y),
	XmRImmediate,
	(XtPointer)-1
    },

#if 0
    {
	XmNcellAlignment,
	XmCCellAlignment,
	XmRAlignment,
	sizeof(unsigned char),
	XtOffsetOf( struct _XmSepGridLayoutConstraintRec, sep_grid_layout.alignment),
	XmRImmediate,
	(XtPointer)XmALIGNMENT_CENTER
    },
#endif

    {
	XmNcellGravity, 
	XmCCellGravity, 
	XtRGravity, 
	sizeof(XtGravity),
        XtOffsetOf( struct _XmSepGridLayoutConstraintRec, sep_grid_layout.gravity),
	XmRImmediate, 
	(XtPointer)CenterGravity
    },
    {
	XmNprefWidth, 
	XmCPrefWidth, 
	XtRDimension, 
	sizeof(Dimension),
        XtOffsetOf( struct _XmSepGridLayoutConstraintRec, sep_grid_layout.prefWidth),
	XmRImmediate, 
	(XtPointer)0
    },
    {
	XmNprefHeight, 
	XmCPrefHeight, 
	XtRDimension, 
	sizeof(Dimension),
        XtOffsetOf( struct _XmSepGridLayoutConstraintRec, sep_grid_layout.prefHeight),
	XmRImmediate, 
	(XtPointer)0
    },
    {
	XmNfillType, 
	XmCFillType, 
	XtRInt, 
	sizeof(int),
        XtOffsetOf( struct _XmSepGridLayoutConstraintRec, sep_grid_layout.fillType),
	XmRImmediate, 
	(XtPointer)0
    },
    
};

/* SepGridLayout class record */

externaldef( xmsepgridlayoutclassrec) XmSepGridLayoutClassRec
					xmSepGridLayoutClassRec =
{
    {		/* core class fields */
	(WidgetClass) &xmManagerClassRec,	/* superclass		*/
	"XmSepGridLayout",			/* class_name		*/
	sizeof(XmSepGridLayoutRec),		/* widget_size		*/
	SepGridLayoutClassInit,			/* class_initialize	*/
	NULL,					/* class_part_init	*/
	FALSE,					/* class_inited		*/
	Initialize,				/* initialize		*/
	NULL,					/* initialize_hook	*/
	XtInheritRealize,			/* realize		*/
	NULL,					/* actions		*/
	0,					/* num_actions		*/
	resources,				/* resources		*/
	XtNumber(resources),			/* num_resources	*/
	NULLQUARK,				/* xrm_class		*/
	TRUE,					/* compress_motion	*/
	XtExposeCompressMaximal,		/* compress_exposure	*/
	TRUE,					/* compress_enterlv	*/
	FALSE,					/* vivible_interest	*/
	(XtWidgetProc)NULL,			/* destroy		*/
	Resize,					/* resize		*/
	Redisplay,				/* expose		*/
	SetValues,				/* set_values		*/
	NULL,					/* set_values_hook	*/
	XtInheritSetValuesAlmost,		/* set_values_almost	*/
	NULL,					/* get_values_hook	*/
	NULL,					/* accept_focus		*/
	XtVersion,				/* version		*/
	NULL,					/* callback_private	*/
	XtInheritTranslations,			/* tm_table		*/
	XtInheritQueryGeometry,			/* query_geometry	*/
	NULL,					/* display_accelerator	*/
	NULL					/* extension		*/
    },
    {		/* composite_class fields */	
	GeometryManager,			/* geometry_manager	*/
	ChangeManaged,				/* change_managed	*/
	InsertChild,				/* insert child		*/
	XtInheritDeleteChild,			/* delete child		*/
	NULL,					/* extension		*/
    },
    {		/* constraint_class fields*/
	constraints,				/* constraint resource list*/
	XtNumber(constraints),			/* num resources	*/
	sizeof(XmSepGridLayoutConstraintRec),	/* constraint size	*/
	ConstraintInitialize,			/* init proc		*/
	(XtWidgetProc)NULL,			/* destroy proc		*/
	ConstraintSetValues,			/* set values proc	*/
	NULL,					/* extension		*/
    },
    {		/* manager_class_fields */
	XtInheritTranslations,			/* translations		*/
	NULL,					/* syn_resources	*/
	0,					/* num_get_resources	*/
	NULL,					/* syn_cont_resources	*/
	0,					/* num_get_cont_resources*/
	XmInheritParentProcess,			/* parent_process	*/
	NULL,					/* extension		*/
    },
    {		/* bulletin board class fields */
	FALSE,					/* always_install_accelerators */
	(XmGeoCreateProc)NULL,			/* geo_matrix_create	*/
	XmInheritFocusMovedProc,		/* focus moved proc	*/
	NULL,					/* extension		*/
    },
    {		/* sep_grid_layout_class fields */
	(XtPointer)NULL,			/* extension		*/
    },
};

externaldef (xmsepgridlayoutwidgetclass) WidgetClass xmSepGridLayoutWidgetClass
					    = (WidgetClass) &xmSepGridLayoutClassRec;					


static void SepGridLayoutClassInit(void)
{
    XtAddConverter( XtRString, XtRGravity, XmuCvtStringToGravity, NULL, 0) ;
}

/* Blocks recount flag, returns old flag value */
void SGLBlock(XmSepGridLayoutWidget w)
{
    w->sep_grid_layout.recount_flag++;
}

/* Unblocks recount flag, returns old flag value*/
void SGLUnBlock(XmSepGridLayoutWidget w)
{
    //int i;
/*    XtWidgetGeometry request, reply_return;
    XtGeometryResult res;*/
    
    if (w->sep_grid_layout.recount_flag == 0)
    {
	//printf("Warning: flag already = 0\n");
	return;
    }
    if (!(-- w->sep_grid_layout.recount_flag))
    {
	//it's time to recount coordinates
#if RECOUNT_STAT
	printf("SGLUnblock - RecountCoordinates");
#endif	
	RecountCoordinates(w);
	
	/*for (i = 0; i < SGL_NumRows(w)*SGL_NumColumns(w); i++)
	    if (SGL_Children(w)[i])
	    {
		printf("Child[%d].x = %d, y = %d\n",i,SGL_Children(w)[i]->core.x,
			SGL_Children(w)[i]->core.y);
	    }*/
	//printf("He!\n");
    }
}


/* Initialize widget */
static void Initialize(Widget rw, 
		    Widget nw, 
		    __attribute__((unused)) ArgList args, //unused
		    __attribute__((unused)) Cardinal *num_args)//unused
{
    short row, col;
    int i,j;
    
    XmSepGridLayoutWidget m = (XmSepGridLayoutWidget) nw;
//    XmSepGridLayoutWidget req = (XmSepGridLayoutWidget) rw;

#if DEBUG_MODE
    printf("Initialize started...\n");
#endif    

    SetNewResources(rw,nw, True);

    row = 0;
    col = 0;
    
    row = SGL_NumRows(m);
    col = SGL_NumColumns(m);
    
    SGL_Children(m) = (Widget*) malloc(sizeof(Widget) * row * col);
    SGL_MaxRows(m) = row;
    SGL_MaxColumns(m) = col;
    SGL_CoordinatesX(m) = (Dimension*) malloc(sizeof(Dimension) * col);
    SGL_CoordinatesY(m) = (Dimension*) malloc(sizeof(Dimension) * row);
    
    for (i = 0; i < col; i++)
	for (j = 0; j < row; j++)
	{    
	    //printf("\naha!\n");
	    SGL_Children(m)[i + j*col] = NULL;
	}
    
    bzero(SGL_CoordinatesX(m), sizeof(Dimension)*col);
    bzero(SGL_CoordinatesY(m), sizeof(Dimension)*row);
    
    SGL_Width(m) = 0;
    SGL_Height(m) = 0;
    
    GetSeparatorGC(m);

//  don't need next 2 strings    
//    SGL_SepW(m) = GetSeparatorSize(m,SGL_VERTICAL);
//    SGL_SepH(m) = GetSeparatorSize(m,SGL_HORIZONTAL);
    
    //printf("Startup data: margin w = %d, h = %d\n",SGL_MarginW(m),SGL_MarginH(m));
#if 0
    for (i = 0; i < col; i++)
    {
	if (i != 1)
	{
	    SGL_CoordinatesX(m)[i] = (2 * SGL_MarginW(m) + SGL_SepW(m) ) * i /*+ SGL_MarginW(m)*/;	
	}    
	else
	{
	     SGL_CoordinatesX(m)[i] = 2 * SGL_MarginW(m); 
	}
	//printf("Init CoordinatesX[%d] = %d\n",i,SGL_CoordinatesX(m)[i]);
    }
    
    for (i = 0; i < row; i++)
    {
	if (i != 1)
	{
	    SGL_CoordinatesY(m)[i] = (2 * SGL_MarginH(m) + SGL_SepH(m) ) * i /*+ SGL_MarginH(m)*/;	
	}
	else
	{
	    SGL_CoordinatesY(m)[i] = 2 * SGL_MarginH(m);
	}    
	//printf("Init CoordinatesY[%d] = %d\n",i,SGL_CoordinatesY(m)[i]);
    }
#endif
#if 1
//    printf("BTW SGL_MarginW(m)=%d SGL_MarginH(m)=%d SGL_SepW(m)=%d SGL_SepH(m)=%d\n",
//	SGL_MarginW(m),SGL_MarginH(m),SGL_SepW(m),SGL_SepH(m));
    for (i = 1; i < col; i++)
    {
	if (i != 1)
	{
	    SGL_CoordinatesX(m)[i] = 2 * SGL_MarginW(m)*i + SGL_SepW(m) *(i-1) /*+ SGL_MarginW(m)*/;	
//	    printf("Set %d coordinatesX to %d\n",i,SGL_CoordinatesX(m)[i]);
	}    
	else
	{
	     SGL_CoordinatesX(m)[i] = 2 * SGL_MarginW(m); 
//	     printf("Set %d coordinatesX to %d\n",i,SGL_CoordinatesX(m)[i]);
	}
	//printf("Init CoordinatesX[%d] = %d\n",i,SGL_CoordinatesX(m)[i]);
    }
    
    for (i = 1; i < row; i++)
    {
	if (i != 1)
	{
	    SGL_CoordinatesY(m)[i] = 2*SGL_MarginH(m)*i + SGL_SepH(m) *(i-1) /*+ SGL_MarginH(m)*/;	
//	    printf("Set %d coordinatesY to %d\n",i,SGL_CoordinatesY(m)[i]);
	}
	else
	{
	    SGL_CoordinatesY(m)[i] = 2 * SGL_MarginH(m);
//	    printf("Set %d coordinatesY to %d\n",i,SGL_CoordinatesY(m)[i]);
	}    
	//printf("Init CoordinatesY[%d] = %d\n",i,SGL_CoordinatesY(m)[i]);
    }
#endif    
    /*set window size*/
    /* perhaps it does not work!*/
    //m->core.width = SGL_CoordinatesX(m)[i-1] + 2 * SGL_MarginW(m) + SGL_SepW(m);
    //m->core.height = SGL_CoordinatesY(m)[i-1] + 2 * SGL_MarginH(m) + SGL_SepH(m);
    /*XtResizeWidget((Widget)m, SGL_CoordinatesX(m)[i-1] + 2 * SGL_MarginW(m) + SGL_SepW(m),
		    SGL_CoordinatesY(m)[i-1] + 2 * SGL_MarginH(m) + SGL_SepH(m),
		    1);*/

   /* printf("Initial window size: Width = %d Height=%d Rows = %d Columns = %d\n",
	    m->core.width, m->core.height, row, col);*/
    
    SGL_LastCellX(m) = 0;
    SGL_LastCellY(m) = 0;
    
    m->sep_grid_layout.add_flag = True;
    m->sep_grid_layout.processing_constraints = False;
    m->sep_grid_layout.recount_flag = 0;
    SGL_NumChildren(m) = 0;    
    RecountCoordinates(m);

#if DEBUG_MODE    
    printf("Initialize done.\n");
#endif
    
}

static  XtGeometryResult
changeGeometry(sglw, req_width,req_height, queryOnly, reply)
XmSepGridLayoutWidget       sglw ;
int                 req_width, req_height ;
int                 queryOnly ;
XtWidgetGeometry    *reply ;
{
    XtGeometryResult    result ;
    Dimension   old_width = sglw->core.width, old_height = sglw->core.height;
	
    if( req_width != sglw->core.width  ||  req_height != sglw->core.height )
    {
        XtWidgetGeometry myrequest ;
			
        myrequest.width = req_width ;
        myrequest.height = req_height ;
        myrequest.request_mode = CWWidth | CWHeight ;
        if( queryOnly )
            myrequest.request_mode |= XtCWQueryOnly ;
							    
        result = XtMakeGeometryRequest((Widget)sglw, &myrequest, reply) ;
        /* BUG.  The Athena box widget (and probably others) will change  
        * our dimensions even if this is only a query.  To work around that  
        * bug, we restore our dimensions after such a query.  
        */
        if( queryOnly ) {
	    sglw->core.width = old_width ;
	    sglw->core.height = old_height ;
	}
    }
    else
        result = XtGeometryNo ;
	    
    if( reply != NULL )
    switch( result ) {
        case XtGeometryYes:
        case XtGeometryDone:
            reply->width = req_width ;
            reply->height = req_height ;
            break ;
        case XtGeometryNo:
            reply->width = old_width ;
            reply->height = old_height ;
            break ;
        case XtGeometryAlmost:
            break ;
    }
																								        
    return result ;
}


/* SetNewResources checks new resources and set them if they are correct.
   		*/

static Boolean SetNewResources(Widget cw, Widget nw, Boolean init)
{

    XmSepGridLayoutWidget old_w = (XmSepGridLayoutWidget) cw;
    XmSepGridLayoutWidget new_w = (XmSepGridLayoutWidget) nw;
    
    //XmRepTypeId shortID 	= XmRepTypeGetId(XmRShort);
    //XmRepTypeId horDimensionID 	= XmRepTypeGetId(XmRHorizontalDimension);
    //XmRepTypeId vertDimensionID = XmRepTypeGetId(XmRVerticalDimension);
    XmRepTypeId sepTypeID 	= XmRepTypeGetId(XmRSeparatorType);
    
    int x, y, i, j, delta, shiftArray;
    int deltax, deltay;
    int numRows;
    short sepH, sepW; 
    Widget* temp;
    register XmSepGridLayoutConstraint con;
    
#if DEBUG_MODE    
    printf("\nStarted function SetNewResources\n");    
#endif    
//    printf("marginH = %d\n",SGL_MarginH(new_w));
    if (init)
    /* it means they have the same values*/
    {
	//printf("Called from Initialize\n");
	if (SGL_MarginH(new_w) > MAX_NUM) SGL_MarginH(new_w) = MARGINH_DEFAULT;
	if (SGL_MarginW(new_w) > MAX_NUM) SGL_MarginW(new_w) = MARGINW_DEFAULT;
	if (new_w->sep_grid_layout.num_rows > MAX_NUM) 
	    SGL_NumRows(new_w) = ROWS_DEFAULT;
	if (new_w->sep_grid_layout.num_columns > MAX_NUM)
	    SGL_NumColumns(new_w) = COLUMNS_DEFAULT;
	
	if (!XmRepTypeValidValue( sepTypeID, 
	    new_w->sep_grid_layout.separator_type_h,
	    (Widget) new_w) )
	    SGL_SepHType(new_w) = SEP_H_TYPE_DEFAULT;
	
	sepH = GetSeparatorSize(new_w,SGL_HORIZONTAL);
	if (SGL_SepH(new_w) < sepH) SGL_SepH(new_w) = sepH;
	
	//inspite of SGL_SepH(new_w) = GetSeparatorSize(new_w,SGL_HORIZONTAL);
	
	if (!XmRepTypeValidValue( sepTypeID, 
	    new_w->sep_grid_layout.separator_type_v,
	    (Widget) new_w) )
	    SGL_SepVType(new_w) = SEP_V_TYPE_DEFAULT;
	    
	sepW = GetSeparatorSize(new_w,SGL_VERTICAL);
	if (SGL_SepW(new_w) < sepW) SGL_SepW(new_w) = sepW;
	//inspite of SGL_SepW(new_w) = GetSeparatorSize(new_w, SGL_VERTICAL);
	
	//why???
	SGL_AddW(new_w) = 0;
	SGL_AddH(new_w) = 0;
	
//	SGL_AddW(new_w) = SGL_SepW(new_w)/2;
//	SGL_AddH(new_w) = SGL_SepH(new_w)/2;
	
	return True;
    }

#if 1
    if (new_w->core.width != old_w->core.width)
    {
#if SNR
	printf ("SNR: core.width\n");
#endif	
        deltax =  (new_w->core.width - old_w->core.width)/(2*SGL_NumColumns(old_w));
        /* count new margin*/	    
	SGL_AddW(new_w) += deltax;
#if ADDW
	printf("SNR ADDW: new value = %d\n",SGL_AddW(new_w));
#endif	

	//printf("SNR: new AddW = %d\n",SGL_AddW(new_w));
    }
    
    if (new_w->core.height != old_w->core.height)
    {
#if SNR
	printf ("SNR: core.height\n");
#endif    
        deltay =  (new_w->core.height - old_w->core.height)/(2*SGL_NumRows(old_w));
        /* count new margin*/	    
	SGL_AddH(new_w) += deltay;
#if ADDH
	printf("SNR ADDH: new value = %d\n",SGL_AddH(new_w));
#endif	
	//printf("SNR: new AddH = %d\n",SGL_AddH(new_w));
    }
#endif
    if (SGL_MarginH(new_w) != SGL_MarginH(old_w))
    {
#if SNR
	printf ("SNR: margin.height\n");
#endif    
        if (SGL_MarginH(new_w) > MAX_NUM)
        {
	    //new value is bad:(
	    //so we would better use an old one
            new_w->sep_grid_layout.margin_height = old_w->sep_grid_layout.margin_height;
	}
	else
	{
	    deltay =  (SGL_MarginH(new_w) - SGL_MarginH(old_w))/(2*SGL_NumRows(old_w));
            /* count new margin*/	    
    	    SGL_AddH(new_w) += deltay;
#if ADDH
	printf("SNR ADDH: new value = %d\n",SGL_AddH(new_w));
#endif	
	    
	}
    }
    
    
    if (SGL_MarginW(new_w) != SGL_MarginW(old_w))
    {
#if SNR
	printf ("SNR: margin.width\n");
#endif    
#if SNR
	printf("SNR: MarginW changed from %d to %d\n", SGL_MarginW(old_w), SGL_MarginW(new_w));    
#endif
        if (SGL_MarginW(new_w) > MAX_NUM)
        {
	    new_w->sep_grid_layout.margin_width =old_w->sep_grid_layout.margin_width;
	}
	else
	{
	    deltax =  (SGL_MarginW(new_w) - SGL_MarginW(old_w))/(2*SGL_NumColumns(old_w));
            /* count new margin*/	    
    	    SGL_AddW(new_w) += deltax;	
#if ADDW
	printf("SNR ADDW: new value = %d\n",SGL_AddW(new_w));
#endif	    
	}	
    }
    
    /* Check sep_width/height */
     if (SGL_SepH(new_w) != SGL_SepH(old_w))
    {
#if SNR
	printf ("SNR: separator.height\n");
#endif    

	sepH = GetSeparatorSize(new_w,SGL_HORIZONTAL);
	if (SGL_SepH(new_w) < sepH) SGL_SepH(new_w) = sepH;
	
	deltay =  (SGL_SepH(new_w) - SGL_SepH(old_w))/2;
        /* count new margin*/	    
    	//SGL_AddH(new_w) += deltay;
	
/*        if (SGL_SepH(new_w) < MIN_SEP_H)
        {
            new_w->sep_grid_layout.sep_height = old_w->sep_grid_layout.sep_height;
	}
	else
	{
	    //realy need???
	    deltay =  (SGL_SepH(new_w) - SGL_SepH(old_w))/2;
            // count new margin
    	//    SGL_AddH(new_w) += deltay;
//	}
*/
    }
    
    
    if (SGL_SepW(new_w) != SGL_SepW(old_w))
    {
#if SNR
	printf ("SNR: margin.width\n");
#endif    
#if SNR
	printf("SNR: MarginW changed from %d to %d\n", SGL_MarginW(old_w), SGL_MarginW(new_w));    
#endif

	sepW = GetSeparatorSize(new_w,SGL_VERTICAL);
	if (SGL_SepW(new_w) < sepW) SGL_SepW(new_w) = sepW;
	
	deltax =  (SGL_SepW(new_w) - SGL_SepW(old_w))/2;
            /* count new margin*/	    
        //SGL_AddW(new_w) += deltax;	
	
        /*if (SGL_SepW(new_w) < MIN_SEP_W)
        {
	    new_w->sep_grid_layout.sep_width =old_w->sep_grid_layout.sep_width;
	}
	else
	{
	    deltax =  (SGL_SepW(new_w) - SGL_SepW(old_w))/2;*/
            /* count new margin*/	    
/*    	    SGL_AddW(new_w) += deltax;	
	}	
	*/
    }
    
     if (new_w->sep_grid_layout.separator_type_h
	!= old_w->sep_grid_layout.separator_type_h)
    {
#if SNR
	printf ("SNR: sep_type_h\n");
#endif    
        if (!XmRepTypeValidValue( sepTypeID, 
	    new_w->sep_grid_layout.separator_type_h,
	    (Widget) new_w) )
	{
	    new_w->sep_grid_layout.separator_type_h =
	        old_w->sep_grid_layout.separator_type_h;
	}
	
	sepH = GetSeparatorSize(new_w,SGL_HORIZONTAL);
	if (SGL_SepH(new_w) < sepH) SGL_SepH(new_w) = sepH;
	//inspite of SGL_SepH(new_w) = GetSeparatorSize(new_w,SGL_HORIZONTAL);
	
	GetSeparatorGC(new_w);
    }

    if (new_w->sep_grid_layout.separator_type_v
	!= old_w->sep_grid_layout.separator_type_v)
    {
#if SNR
	printf ("SNR: sep_type_w\n");
#endif    
//	printf("Old sep type =%c, new sep type=%c\n",SGL_SepVType(old_w),
//	    SGL_SepVType(new_w));
//	printf("Check v sep type for validity\n");
	    
	if (!XmRepTypeValidValue( sepTypeID, 
	    new_w->sep_grid_layout.separator_type_v,
	    (Widget) new_w) )
	{
//	    printf("It's bad valid type\n");	
	    new_w->sep_grid_layout.separator_type_v =
		old_w->sep_grid_layout.separator_type_v;
	}	
	
	sepW = GetSeparatorSize(new_w,SGL_VERTICAL);
	if (SGL_SepW(new_w) < sepW) SGL_SepW(new_w) = sepW;
	//inspite of SGL_SepW(new_w) = GetSeparatorSize(new_w, SGL_VERTICAL);
	GetSeparatorGC(new_w);
    }
    
    /*there could be a bug - what if we have increased rows number and 
	decreased columns number?*/
    
/*    printf("Compare old and new values\n");
    printf("New row = %d,old row = %d\n",SGL_NumRows(new_w),SGL_NumRows(old_w));*/
    numRows = old_w->sep_grid_layout.num_rows;
    if (new_w->sep_grid_layout.num_rows
	!= old_w->sep_grid_layout.num_rows)
    {
#if SNR
	printf ("SNR: num rows\n");
#endif    
        //printf("Check valid type for row resource\n");
        if (new_w->sep_grid_layout.num_rows > MAX_NUM)
        {
//	    printf("Bad row resource. Set %d rows\n", old_w->sep_grid_layout.num_rows);
	    new_w->sep_grid_layout.num_rows = old_w->sep_grid_layout.num_rows;
	}
	else
	{
	    if (new_w->sep_grid_layout.num_rows <= 0)
		new_w->sep_grid_layout.num_rows = 1;
	    /* new resource is ok*/
	    numRows = new_w->sep_grid_layout.num_rows;
#if SNR
	    for (i = 0; i < SGL_NumRows(old_w)*SGL_NumColumns(old_w);i++)
		if (SGL_Children(old_w)[i])
		    printf("Old (rows) i  = %d busy x = %d y =%d\n",i,
			    SGL_Children(old_w)[i]->core.x,
			    SGL_Children(old_w)[i]->core.y);
#endif	    
	    /* realloc coords array if we need*/
	    if (numRows > old_w->sep_grid_layout.num_rows)
	    {
	        /* increase */
#if SNR
		printf("increase rows...\n");
		printf("numRows = %d, MaxRows = %d\n",numRows, SGL_MaxRows(new_w));
#endif
		if (numRows > SGL_MaxRows(new_w))
		{
		    SGL_CoordinatesY(new_w) = (Dimension*) realloc(SGL_CoordinatesY(new_w),
			sizeof(Dimension) * numRows);
		    SGL_Children(new_w) = (Widget*) realloc(SGL_Children(new_w),
			sizeof(Widget)* numRows * SGL_NumColumns(old_w));
		    SGL_MaxRows(new_w) = numRows;
		}
		/* here is the place for entering new Y coor values */
		delta = 2*SGL_MarginH(new_w) + SGL_SepH(new_w);
		
		for (y = SGL_NumRows(old_w); y < numRows; y++)
		{
		    if (y==0) SGL_CoordinatesY(new_w)[y]=0;
		    else if (y > 1)
		    {
/*			SGL_CoordinatesY(new_w)[y] = 
			    SGL_CoordinatesY(new_w)[SGL_NumRows(old_w) - 1] + 
			    delta * (y + 1 - SGL_NumRows(old_w));*/
			SGL_CoordinatesY(new_w)[y] = SGL_CoordinatesY(new_w)[y-1] + delta;
		    }
		    else
		    {
	/*		SGL_CoordinatesY(new_w)[y] = 
			    SGL_CoordinatesY(new_w)[SGL_NumRows(old_w) - 1] + 
			    (delta - SGL_SepH(new_w))* (y + 1 - SGL_NumRows(old_w));*/
			SGL_CoordinatesY(new_w)[y] = delta - SGL_SepH(new_w);
		    }
		}

		for (i = SGL_NumRows(old_w)*SGL_NumColumns(old_w);
			i < numRows * SGL_NumColumns(old_w);i++)
		    SGL_Children(new_w)[i] = NULL;/*see notes.txt - 1*/

	    }
	    else
	    {
	        /* decrease */
	        for (y = SGL_NumRows(old_w)-1; y > SGL_NumRows(new_w)-1; y--)
		    for (x = 0; x < SGL_NumColumns(old_w); x++)
		    {
			i = x + y*SGL_NumColumns(old_w);
		        if (SGL_Children(new_w)[i])
		        {
#if SNR
			    printf("Hi!!!!!!!!!!!!!!!!!!!!!!!!:)\n");
#endif			    
			    XmeConfigureObject(SGL_Children(new_w)[i], 0, 0, 
			    SGL_Children(new_w)[i]->core.width,SGL_Children(new_w)[i]->core.height,
			    SGL_Children(new_w)[i]->core.border_width);
			    //we need to overwrite constraint recources here!!!
			    con = GetSGLConstraint(SGL_Children(new_w)[i]);
			    con->num_cell_x = -1;
			    con->num_cell_y = -1;
			    //the main thing!!!!!!!!!!!!
			    SGL_Children(new_w)[i] = NULL;
			}
		    }
		
		if (XtIsRealized(new_w))
                {
#if SNR
                    printf("SNR call RecountCoordinates\n");
#endif
                    RecountCoordinates(new_w);
                }
	    }
		
	}	
    }

/* there could be a bug - see previous comment*/
#if SNR
    printf("SetNewResources: numCols old = %d, new = %d\n",
	    SGL_NumColumns(old_w), SGL_NumColumns(new_w));
#endif

    if (new_w->sep_grid_layout.num_columns
	!= old_w->sep_grid_layout.num_columns)
    {
#if SNR
	printf ("SNR: num columns\n");
#endif    
#if SNR    
	printf("SetNewResources: shift Columns...\n");
#endif	
        if (new_w->sep_grid_layout.num_columns > MAX_NUM)
        {	
//	    printf("Bad column resource\n");
	    SGL_NumColumns(new_w) = SGL_NumColumns(old_w);
	}
	else
	{
	    if (new_w->sep_grid_layout.num_columns <= 0)
		new_w->sep_grid_layout.num_columns = 1;
	    /* columns resource is ok */
	    /* realloc coords array if we need*/
	    /* delta vs shiftArray fixed 22.03.03*/
	    if ((shiftArray = SGL_NumColumns(new_w) - SGL_NumColumns(old_w)) > 0)
	    {
#if 0	    
		printf("SetNewResources: shift Columns from %d to %d\n",
			SGL_NumColumns(old_w),SGL_NumColumns(new_w));
#endif	
#if 0
		printf("numRows = %d\n",numRows);
		for (i = 0; i < SGL_NumColumns(old_w)*SGL_NumRows(old_w);i++)
		{
		    printf("i = %d\n",i);
		    if (SGL_Children(old_w)[i]) 
			printf("i = %d busy x = %d\n",i,SGL_Children(old_w)[i]->core.x);
		    if (SGL_Children(new_w)[i]) 
			printf("i = %d busy x = %d\n",i,SGL_Children(new_w)[i]->core.x);
		    
			
		}
		printf("printf complete....\n");
#endif		
		//printf("numRows = %d, numColumns = %d\n",numRows,SGL_NumColumns(old_w));
		
		//malloc temp array and fill it
		temp = (Widget*) malloc(sizeof(Widget) * SGL_NumColumns(old_w) *
					numRows);
		
		//printf("malloc done.\n");
		for (i = 0; i < SGL_NumColumns(old_w) * numRows; i++)
		    //if (SGL_Children(new_w)[i]) //bug #4 - fixed 4.09.03
			temp[i] = SGL_Children(new_w)[i];
		    
		    /*if (SGL_Children(old_w)[i])
		    {
			temp[i] = SGL_Children(old_w)[i];
			printf("temp[%d] = %d\n",i,*temp[i]);
		    }*/

//		printf("temp filled\n");

	        /* increase */
		if (SGL_NumColumns(new_w) > SGL_MaxColumns(new_w))
		{
	    	    SGL_CoordinatesX(new_w) = (Dimension*) realloc(SGL_CoordinatesX(new_w),
			sizeof(Dimension)*SGL_NumColumns(new_w));
//		    printf("realloc children array...\n");
		    SGL_Children(new_w) = (Widget*) realloc(SGL_Children(new_w),
			sizeof(Widget)*SGL_NumColumns(new_w)*
			numRows);
//		    printf("complete\n");
		    SGL_MaxColumns(new_w) = SGL_NumColumns(new_w);
		}
		
//		printf(":)\n");

		/* here is the place for initializing new X coor fields*/
		delta = 2*SGL_MarginW(new_w) + SGL_SepW(new_w);
		
		for (x = SGL_NumColumns(old_w); x < SGL_NumColumns(new_w); x++)
		{
#if SNR
		    puts("");
		    printf("SNR: x=%d\n",x);
		    puts("");
#endif		    
		    if (x==0) SGL_CoordinatesX(new_w)[x]=0;
		    else if (x>1) 
		    {
/*			SGL_CoordinatesX(new_w)[x] = 
		    	    SGL_CoordinatesX(new_w)[SGL_NumColumns(old_w) - 1] + 
		    	    delta * (x + 1 - SGL_NumColumns(old_w));*/
			SGL_CoordinatesX(new_w)[x] = SGL_CoordinatesX(new_w)[x-1] + delta;
		    }
		    else
		    {
/*			SGL_CoordinatesX(new_w)[x] = 
		    	    SGL_CoordinatesX(new_w)[SGL_NumColumns(old_w) - 1] + 
		    	    (delta - SGL_SepW(new_w)) * (x + 1 - SGL_NumColumns(old_w));*/
			SGL_CoordinatesX(new_w)[x] = delta - SGL_SepW(new_w);
		    }
		}
#if SNR
		printf("SNR: Let's fill new array with zeros\n");
#endif

#if 0		
		for (i = numRows * SGL_NumColumns(old_w);
			i < numRows * SGL_NumColumns(new_w);i++)
		    SGL_Children(new_w)[i] = NULL;/*see notes.txt - 1*/
#endif		    
		/* shift Children array */

#if 0
		/* Old version */		    
		
		i = SGL_NumColumns(old_w) * numRows - 1;
		//last child in old widget
		    
		j = SGL_NumColumns(new_w) * numRows - 1 - shiftArray;
		//j - place for last child in new widget
		    
		for (y = numRows - 1; y > 1; --y)
		{
		    for (x = SGL_NumColumns(old_w)-1; x > 0; --x)
		    {
		        SGL_Children(new_w)[j] = SGL_Children(old_w)[i];
		        i--;
		        j--;
		    }
		    j -= shiftArray;
/*		    printf("x = %d, y = %d, i = %d,j = %d shiftArray = %d\n", 
			    x, y,i,j,shiftArray);
*/			    
		}
		/* End of old version */
#endif		

#if SNR
		printf("SNR:Time to fill new array...\n");
#endif

		/* new version 1.04.03 */
		i = 0;
		//first child in old widget
		    
		j = 0;
		//j - place for first child in new widget
		    
		for (y = 0; y < numRows; y++)
		{
		    for (x = 0; x < SGL_NumColumns(old_w); x++)
		    {
			SGL_Children(new_w)[j] = temp[i];
#if SNR			
			if (temp[i])
			{			
			    printf("Old i = %d Busy\n",i);
			    printf(" x = %d, y = %d\n",
				temp[i]->core.x, temp[i]->core.y);
			}
#endif			
			i++;
			j++;
		    }
		    for (x = 0; x < shiftArray; x++)
		    {
			SGL_Children(new_w)[j] = NULL;
			j++;
		    }
//		    printf("x = %d, y = %d, i = %d,j = %d shiftArray = %d\n", x, y,i,j,shiftArray);
		}
		free(temp);
/*		
		for (i = 0; i < numRows* SGL_NumColumns(new_w);i++)
		{
		    if (SGL_Children(new_w)[i])
		      printf (" i = %d Busy\n",i);
		    
		}
*/		
	    }
	    else
	    {
		/* decrease */
		/* remove outborder Children,e.g. set their core x,y = 0 */
		    
		for (y = 0; y < numRows; y++)
		    for (x = SGL_NumColumns(new_w); x < SGL_NumColumns(old_w); x++)
		    {
			i = x + y*SGL_NumColumns(old_w);
			if (SGL_Children(new_w)[i])
			{
#if SNR			
			    printf("Hi!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!11\n");
			    printf("The i = %d\n",i);
#endif			    
			    XmeConfigureObject(SGL_Children(new_w)[i], 0, 0, 
			    SGL_Children(new_w)[i]->core.width,SGL_Children(new_w)[i]->core.height,
			    SGL_Children(new_w)[i]->core.border_width);
			    //we have to set constraint resources here!!!
			    con = GetSGLConstraint(SGL_Children(new_w)[i]);
			    con->num_cell_x = -1;
			    con->num_cell_y = -1;
			    //the main thing!!!!!!!!!!!!1
			    SGL_Children(new_w)[i] = NULL;//!!!!!!!!!!!!!
			}
		    }	    
		/* shift Children array */
		for (x = 0;x < SGL_NumColumns(old_w); x++)
		    for (y = 1; y < numRows; y++)
		    {
			i = x+y*SGL_NumColumns(old_w);
			if (y < numRows - 1 || x < SGL_NumColumns(old_w) + shiftArray)
			    SGL_Children(new_w)[i + shiftArray * y] = 
				SGL_Children(new_w)[i];
		    }

		if (XtIsRealized(new_w))
                {
#if SNR
                    printf("SNR call RecountCoordinates\n");
#endif
                    RecountCoordinates(new_w);
                }

	    }
	}
    }

//    printf("Final marginH value=%d\n",SGL_MarginH(new_w));
#if SNR
    printf("SNR: Call RecountCordinates\n");
#endif    
    //RecountCoordinates(new_w);//fixed 25.03.03
#if SNR
    for (i = 0; i < numRows*SGL_NumColumns(new_w);i++)
	if (SGL_Children(new_w)[i])
	    printf("i = %d busy x = %d y = %d\n",
		    i, SGL_Children(new_w)[i]->core.x,SGL_Children(new_w)[i]->core.y);
#endif    
    return True;
}

/* check to change layout configuration */
static void ChangeManaged(Widget w)
{
#if 0
    XmSepGridLayoutWidget sglw = (XmSepGridLayoutWidget)w;
    XtWidgetGeometry reply;
    Dimension width, height;

    printf("core width=%d height=%d\n", sglw->core.width, sglw->core.height);
    printf("Other check: core width=%d height=%d\n", SGL_Width(sglw), SGL_Height(sglw));
    printf("columns=%d rows=%d\n", SGL_NumColumns(sglw), SGL_NumRows(sglw));

    RecalcSize(sglw, NULL, &width, &height);

    /* ask to change geometry to accomodate; accept any compromise offered */
    if( changeGeometry(sglw, width, height, False, &reply) == XtGeometryAlmost )
        (void) changeGeometry(sglw, reply.width, reply.height, False, &reply) ;
    
    /* always re-execute layout*/
    RecountCoordinates(sglw);
    XtClass(w)->core_class.resize((Widget)sglw) ;    
    
    printf("After\n");
     printf("core width=%d height=%d\n", sglw->core.width, sglw->core.height);
    printf("Other check: core width=%d height=%d\n", SGL_Width(sglw), SGL_Height(sglw));
    printf("columns=%d rows=%d\n", SGL_NumColumns(sglw), SGL_NumRows(sglw));

#endif

    XmSepGridLayoutWidget sglw = (XmSepGridLayoutWidget)w;
    XtWidgetGeometry reply;
    Dimension width, height;
    int i;

#if DEBUG_MODE
    printf("ChangeManaged started!\n");
#endif    

#if CH_MAN
    printf("Step1\n");
    printf("core width=%d height=%d\n", sglw->core.width, sglw->core.height);
    printf("Other check: core width=%d height=%d\n", SGL_Width(sglw), SGL_Height(sglw));
    printf("columns=%d rows=%d\n", SGL_NumColumns(sglw), SGL_NumRows(sglw));
    for (i = 0; i<SGL_NumChildren(sglw);i++)
    {
	if (SGL_Children(sglw)[i]!=NULL){
	     printf("Child %d width=%d height=%d\n",i,SGL_Children(sglw)[i]->core.width,
	         SGL_Children(sglw)[i]->core.height);
	}
	else printf("Child %d is null\n",i);
    }
#endif
    
    RecountCoordinates((XmSepGridLayoutWidget)w);
#if CH_MAN
    printf("Step2\n");
    printf("core width=%d height=%d\n", sglw->core.width, sglw->core.height);
    printf("Other check: core width=%d height=%d\n", SGL_Width(sglw), SGL_Height(sglw));
    printf("columns=%d rows=%d\n", SGL_NumColumns(sglw), SGL_NumRows(sglw));
    for (i = 0; i<SGL_NumChildren(sglw);i++)
    {
	if (SGL_Children(sglw)[i]!=NULL){
	     printf("Child %d width=%d height=%d\n",i,SGL_Children(sglw)[i]->core.width,
	         SGL_Children(sglw)[i]->core.height);
	}
	else printf("Child %d is null\n",i);
    }
#endif    
    width = XtWidth(w);
    height = XtHeight(w);
#if CH_MAN    
    printf("Step3\n");
    printf("core width=%d height=%d\n", sglw->core.width, sglw->core.height);
    printf("Other check: core width=%d height=%d\n", SGL_Width(sglw), SGL_Height(sglw));
    printf("columns=%d rows=%d\n", SGL_NumColumns(sglw), SGL_NumRows(sglw));
    for (i = 0; i<SGL_NumChildren(sglw);i++)
    {
	if (SGL_Children(sglw)[i]!=NULL){
	     printf("Child %d width=%d height=%d\n",i,SGL_Children(sglw)[i]->core.width,
	         SGL_Children(sglw)[i]->core.height);
	}
	else printf("Child %d is null\n",i);
    }
#endif    
    while(XtMakeResizeRequest(w,width, height,&width,&height)==
	    XtGeometryAlmost)
    {
	RecountCoordinates((XmSepGridLayoutWidget)w);	
	width = XtWidth(w);
	height = XtHeight(w);
    }
#if CH_MAN    
    printf("Step4\n");
    printf("core width=%d height=%d\n", sglw->core.width, sglw->core.height);
    printf("Other check: core width=%d height=%d\n", SGL_Width(sglw), SGL_Height(sglw));
    printf("columns=%d rows=%d\n", SGL_NumColumns(sglw), SGL_NumRows(sglw));
    for (i = 0; i<SGL_NumChildren(sglw);i++)
    {
	if (SGL_Children(sglw)[i]!=NULL){
	     printf("Child %d width=%d height=%d\n",i,SGL_Children(sglw)[i]->core.width,
	         SGL_Children(sglw)[i]->core.height);
	}
	else printf("Child %d is null\n",i);
    }
#endif    
    XmeNavigChangeManaged(w);
#if CH_MAN    
    printf("Step5\n");
    printf("core width=%d height=%d\n", sglw->core.width, sglw->core.height);
    printf("Other check: core width=%d height=%d\n", SGL_Width(sglw), SGL_Height(sglw));
    printf("columns=%d rows=%d\n", SGL_NumColumns(sglw), SGL_NumRows(sglw));
    for (i = 0; i<SGL_NumChildren(sglw);i++)
    {
	if (SGL_Children(sglw)[i]!=NULL){
	     printf("Child %d width=%d height=%d\n",i,SGL_Children(sglw)[i]->core.width,
	         SGL_Children(sglw)[i]->core.height);
	}
	else printf("Child %d is null\n",i);
    }
#endif    
}

/* Get GC to draw separator */
static short GetSeparatorSize(XmSepGridLayoutWidget w, char orient)
{
    unsigned char type;
    short size = 0;
    
    if (orient == SGL_HORIZONTAL)
    {
	type = SGL_SepHType(w);
    }
    else
    {
	type = SGL_SepVType(w);
    }
    
    if (type == XmNO_LINE)
	size = 0;
    if (type == XmSINGLE_LINE || type == XmSINGLE_DASHED_LINE)
	size = 1;
    if (type == XmDOUBLE_LINE || type == XmDOUBLE_DASHED_LINE)
	size = 3;
    if (type == XmSHADOW_ETCHED_IN || type == XmSHADOW_ETCHED_OUT 
	|| type == XmSHADOW_ETCHED_IN_DASH || type == XmSHADOW_ETCHED_OUT_DASH)
	size = 2;

/*
    if (orient == SGL_HORIZONTAL)
    {
	SGL_SepH(w) = size;
    }
    else
    {
	SGL_SepW(w) = size;
    }
    */
    //those block was changed 8.10.04 to this
    return size;
    //end
    
    
/*    printf("--------------------------------aaaaaaaaaaaaaaaaaa!!!!!!!!!1-------------------------\n");
    printf("GetSeparator size: Sep size w = %d, h = %d\n",SGL_SepW(w),SGL_SepH(w));
    */
}

/* Get GC to draw separator */
static void GetSeparatorGC(XmSepGridLayoutWidget w)
{
    XGCValues 	values_h, values_v;
    XtGCMask 	valueMask_h, valueMask_v;    
    
    valueMask_h = GCForeground | GCBackground;
    valueMask_v = GCForeground | GCBackground;
    
    values_h.foreground = w -> manager.foreground;
    values_h.background = w -> core.background_pixel;
    values_v.foreground = w -> manager.foreground;
    values_v.background = w -> core.background_pixel;
    
    if (w -> sep_grid_layout.separator_type_h == XmSINGLE_DASHED_LINE ||
	w -> sep_grid_layout.separator_type_h == XmDOUBLE_DASHED_LINE)
    {
	valueMask_h = valueMask_h | GCLineStyle;
        values_h.line_style = LineDoubleDash;
    }
    if (w -> sep_grid_layout.separator_type_v == XmSINGLE_DASHED_LINE ||
	w -> sep_grid_layout.separator_type_v == XmDOUBLE_DASHED_LINE)
    {
	valueMask_v = valueMask_v | GCLineStyle;
        values_v.line_style = LineDoubleDash;
    }

    w -> sep_grid_layout.separator_GC_h = XtGetGC ( (Widget) w, valueMask_h, &(values_h) );
    w -> sep_grid_layout.separator_GC_v = XtGetGC ( (Widget) w, valueMask_v, &(values_v) );
}

/* Redisplay proc */

static void Redisplay(Widget w,XEvent *event, Region region)
{
    XmSepGridLayoutWidget m = (XmSepGridLayoutWidget) w;
    XEvent tempEvent;
    short numX = SGL_NumColumns(m);
    short numY = SGL_NumRows(m);
    short i;
    
    if (event == NULL) /* fast exposure */
    {
//	printf("Fast exposure\n");
	event = &tempEvent;
	event->xexpose.x = 0;
	event->xexpose.y = 0;
	event->xexpose.width = m->core.width;
	event->xexpose.height = m->core.height;
    }
    
    /* clear area*/
    XClearArea(XtDisplay(w), XtWindow(w), 0, 0, w->core.width, w->core.height,
		False);
#if 0    
    printf("Redisplay... \n");
    /* Draw all separators*/
    printf("Columns = %d, Rows = %d\n", numX, numY);
    printf("Core width = %d, height = %d\n",m->core.width,m->core.height);
#endif    
    
//    printf("Sep v type = %c\n", SGL_SepVType(m));
    for (i = 1; i < numX; i++)
    {
//	printf ("Redisplay: x = %d\n",SGL_CoordinatesX(m)[i]);
	XmeDrawSeparator(XtDisplay(m), XtWindow(m),
		    m->manager.top_shadow_GC,
		    m->manager.bottom_shadow_GC,
		    m->sep_grid_layout.separator_GC_v,
		    SGL_CoordinatesX(m)[i],0,
		    SGL_SepW(m), m->core.height,
		    m->manager.shadow_thickness,
		    0,
		    XmVERTICAL,
		    SGL_SepVType(m));
    }
    
//    printf("There are %d hor separators\n",numY);
    
    for (i = 1; i < numY; i++)
    {
/*	printf ("y = %d\n",SGL_CoordinatesY(m)[i]);*/
	XmeDrawSeparator(XtDisplay(m), XtWindow(m),
		    m->manager.top_shadow_GC,
		    m->manager.bottom_shadow_GC,
		    m->sep_grid_layout.separator_GC_h,
		    0, SGL_CoordinatesY(m)[i],
		    m->core.width,  SGL_SepH(m),
		    m->manager.shadow_thickness,
		    0,
		    XmHORIZONTAL,
		    SGL_SepHType(m));
    }
    //printf("Redisplay gadgets...\n");
    
    //printf("row = %d, col = %d\n",SGL_NumRows(m),SGL_NumColumns(m));
#if REDISP    
   for (i=0;i<SGL_NumRows(m)*SGL_NumColumns(m);i++)
	if (SGL_Children(m)[i])
	{
	    printf("i=%d\n",i);
	    printf("Redisplay Child[%d].x = %d, y=%d width=%d\n",i,SGL_Children(m)[i]->core.x,
			SGL_Children(m)[i]->core.y,SGL_Children(m)[i]->core.width);
	}
#endif	
    XmeRedisplayGadgets ( (Widget) m, event, region);

    //???????
  //  XmeDrawShadows(XtDisplay (m), XtWindow (m),
		    /* pop-out not pop-in*/
/*		    m->manager.top_shadow_GC,
		    m->manager.bottom_shadow_GC,
		    0, 0, m->core.width, m->core.height,
		    m->manager.shadow_thickness,
		    XmSHADOW_OUT);*/
}

/* Invoke the application resize callbacks */
static void Resize(Widget wid)
{
    int deltax=0, deltay=0;
    XmSepGridLayoutWidget sgl = (XmSepGridLayoutWidget) wid;
#if DEBUG_MODE
    printf("Resize started!\n");
#endif
#if RESIZE_STAT
    puts("");
    printf("Resize %x: Widget old (SGL_Width) width = %d, height = %d\n", sgl, SGL_Width(sgl),
	    SGL_Height(sgl));
    printf("Resize %x: Widget new (core) width = %d, height = %d\n", sgl, sgl->core.width,
	    sgl->core.height);
#endif
    if (SGL_NumColumns(sgl)!=0)    
	deltax =  (int)(sgl->core.width - SGL_Width(sgl))/(2*SGL_NumColumns(sgl));
    if (SGL_NumRows(sgl)!=0)
	deltay =  (int)(sgl->core.height - SGL_Height(sgl))/(2*SGL_NumRows(sgl));
/*    printf("Widget old margin width = %d, height = %d\n", SGL_MarginW(sgl),
	    SGL_MarginH(sgl));*/
    /* count new margin*/	    
    SGL_AddW(sgl) += deltax;//???
    //SGL_AddW(sgl) = deltax;
#if ADDW
	printf("Resize ADDW: new value = %d\n",SGL_AddW(sgl));
#endif    
    SGL_AddH(sgl) += deltay;
#if ADDH
	printf("RESIZE ADDH: new value = %d\n",SGL_AddH(sgl));
#endif	
    
#if RESIZE_STAT
    puts("");
    printf("core.width = %d, core.height = %d\n", sgl->core.width, sgl->core.height);
    printf("SGL_Width = %d, SGL_Height = %d\n", SGL_Width(sgl), SGL_Height(sgl));
    printf("deltax = %d, deltay = %d\n", deltax, deltay);
#endif
    if (SGL_AddW(sgl) < MIN_ADD_WIDTH)
	SGL_AddW(sgl) = MIN_ADD_WIDTH;
    if (SGL_AddH(sgl) < MIN_ADD_HEIGHT)
	SGL_AddH(sgl) = MIN_ADD_HEIGHT;
/*    printf("Widget new margin width = %d, height = %d\n", SGL_MarginW(sgl),
	    SGL_MarginH(sgl));*/
#if RESIZE_STAT
    puts("");
    printf("Widget new addW = %d, addH = %d\n", SGL_AddW(sgl), SGL_AddH(sgl));
#endif
#if RESIZE_STAT    
    printf("Resize - RecountCoordinates\n");
#endif  	    
    RecountCoordinates(sgl);
}

static Boolean SetValues(
			Widget cw,
			__attribute__((unused)) Widget rw,//unused
			Widget nw,
			__attribute__((unused)) ArgList args,//unused
			__attribute__((unused)) Cardinal *num_args/*unused*/)
{
    if (SetNewResources(cw, nw, False)) return True;
    return False;
}

/* This function creates and returns a SepGridLayout widget */
Widget XmCreateSepGridLayout(Widget p, String name, ArgList args, Cardinal n)
{
    return ( XtCreateWidget(name, xmSepGridLayoutWidgetClass, p, args, n));
}

/* RecalcSize - recalculates possible grid size for GeometryManager 
    and control the size of children (_especially_ the children with fill
    parameter not equal to none )*/

static void RecalcSize(XmSepGridLayoutWidget par, 
			Widget w, /*child that has been changed*/
			Dimension* width,/* new grid size */
			Dimension* height)
{
    int MaxWidth, MaxHeight; 
    Widget wid;
    register XmSepGridLayoutConstraint con;
    int cellX, cellY;
    int x,y,i;

#if DEBUG_MODE
    puts("");
    puts("Recalc Size started!");
#endif    

    /* here was a bug, because a wrote width=0; */    
    *width = 0;
    *height = 0;
    
    //if (w == NULL) return;
    
    //con = GetSGLConstraint(w);
    if (w)
    {
	con = GetSGLConstraint(w);
	cellX = con->num_cell_x;
	cellY = con->num_cell_y;
    }
    else
    {
	cellX = cellY = -1;
    }
    
    
    /* get the space between columns*/
    for (x = 0;x < SGL_NumColumns(par); x++)
    {
	MaxWidth = 0;//margin*2 will be added later
	for (y = 0; y < SGL_NumRows(par); y++)
	{
	    i = x + y * SGL_NumColumns(par);
	    if (SGL_Children(par)[i] && SGL_Children(par)[i]->core.managed) //if there is a child
	    {
		if (x == cellX && y == cellY)
		    wid = w;
		else
		    wid = SGL_Children(par)[i];
		con = GetSGLConstraint(wid);
		if (MaxWidth < con->prefWidth)
		    MaxWidth = con->prefWidth;
#if Recalc_Size_Stat
    printf("RecalcSize: MaxWidth[%d, %d] = %d\n", x, y, MaxWidth);
#endif
	    		    
	    }
	}


        MaxWidth += (SGL_MarginW(par) + SGL_AddW(par))*2;
	
	*width += MaxWidth;
	//if (x < SGL_NumColumns(par)-1) *width += SGL_SepW(par);
	if (x!=0) *width += SGL_SepW(par);
#if Recalc_Size_Stat
	printf("RecalcSize: x = %d, total width = %d\n", x, *width);
#endif	
    }
    
     /* get the space between rows*/
    for (y = 0; y < SGL_NumRows(par); y++)
    {
	MaxHeight = 0;//margin*2 will be added later
	for (x = 0; x < SGL_NumColumns(par); x++)
	{
	    i = x + y * SGL_NumColumns(par);
	    if (SGL_Children(par)[i] && SGL_Children(par)[i]->core.managed)/*if there is a child*/
	    {
		//con = GetSGLConstraint(SGL_Children(par)[i]);
		if (x == cellX && y == cellY)
		    wid = w;
		else
		    wid = SGL_Children(par)[i];
		con = GetSGLConstraint(wid);
		if (MaxHeight < con->prefHeight)
		    MaxHeight = con->prefHeight;
	    }
	}
        MaxHeight += (SGL_MarginH(par)+SGL_AddH(par))*2;
	
	*height += MaxHeight;
//	if (y < SGL_NumRows(par)-1) *height += SGL_SepH(par);
	if (y != 0 ) *height += SGL_SepH(par);
#if Recalc_Size_Stat
	printf("RecalSize y=%d MaxHeight=%d height=%d par=%d\n",y,MaxHeight,*height,par);
#endif	
    }

    //added 140106
    SGL_Width(par) = *width;
    SGL_Height(par) = *height;
#if Recalc_Size_Stat
    printf("Recalc_Size SGL_Width=%d SGL_Height=%d %x\n",SGL_Width(par), SGL_Height(par),par);
    printf("Core.width = %d, core.height = %d\n",par->core.width, par->core.height);
#endif    
    
#if DEBUG_MODE
    printf("Recalc size finished.\n");
#endif    
    
}

static XtGeometryResult GeometryManager(
			Widget w,/*instigator*/
			XtWidgetGeometry *request,
			__attribute__((unused)) XtWidgetGeometry *reply/*unused*/)
{
    XmSepGridLayoutWidget sglw = (XmSepGridLayoutWidget) XtParent(w);
    XtWidgetGeometry 	parentRequest;
    XtGeometryResult	result;
    Dimension		curWidth, curHeight, curBW/*, curX, curY*/;
    Dimension		*width_return, *height_return;
    register 		XmSepGridLayoutConstraint con;
    Dimension 		width, height;

    width_return = (Dimension*) malloc(sizeof(Dimension));
    height_return = (Dimension*) malloc(sizeof(Dimension));

#if DEBUG_MODE
    puts("");
    puts("Geometry manager started");
#endif
    
#if GEOM_MAN    
    printf("\nGM: Old widget size: width = %d, height = %d\n",w->core.width,w->core.height);
#endif
    
    /* get constraints */
    //here was a bug (see next string)
    //con = GetSGLConstraint(sglw);// why should we get it from SGL??? Not from label?!
    //fixed 15.03.04
    
    con = GetSGLConstraint(w);
#if GEOM_MAN     
    printf("GM: Pref widget size: width = %d, height = %d\n",con->prefWidth, con->prefHeight);
    
    printf("Step 1\n");
    
    printf("Parent width = %d, height = %d\n",SGL_Width(sglw), SGL_Height(sglw));
#endif    

    /* If the request was caused by the ConstraintSetValues reset the flag*/
    if (sglw->sep_grid_layout.processing_constraints)
    {
	sglw->sep_grid_layout.processing_constraints = False;
	request->border_width--;
    }

#if GEOM_MAN    
printf("Step 2\n");

    /* Save the original child resources*/
printf("pw = %d\n", con->prefWidth);    
#endif
    
    curWidth = con->prefWidth;
    curHeight = con->prefHeight;
    curBW = w->core.border_width;

//printf("Step 3\n");

    /* Deny any requests for a new position */    
//removed at 15.09.05
/*    if (request->request_mode & CWX || request->request_mode & CWY)
	return XtGeometryNo;*/
	    
#if GEOM_MAN 
    printf("Step 4\n");
    printf("Request mode = %d\n",request->request_mode);
    printf("CWX = %d, CWY = %d\n", CWX, CWY);
    printf("CWWidth = %d, CWHeight = %d, CWBorderWidth= %d\n", CWWidth, CWHeight, CWBorderWidth);
    printf("Pref width = %d, height = %d\n", con->prefWidth, con->prefHeight);
    printf("Request width = %d, height = %d\n", request->width, request->height);
#endif

    /* very important place!!!*/		
    
    if (request->request_mode & CWWidth)
    {
#if GEOM_MAN	
	printf("GM: prefWidth change from %d to %d\n",con->prefWidth, request->width);
#endif	
	con->prefWidth = request->width;
    }
    
    if (request->request_mode & CWHeight)
    {
	/*w->core.height = request->height;*/
#if GEOM_MAN	
	printf("GM: prefHeight change from %d to %d\n",con->prefHeight, request->height);
#endif	
	con->prefHeight = request->height;
    }
	
    if (request->request_mode & CWBorderWidth)
	w->core.border_width = request->border_width;

#if GEOM_MAN	
printf("Step 5\n");	
#endif

    /* Calculate a new ideal size based on these requests. */
    RecalcSize(sglw, w, &parentRequest.width, &parentRequest.height);
/*  Old version 19.05.04
    if (parentRequest.width < sglw->core.width)
    {
	parentRequest.width = sglw->core.width;
    }
    if (parentRequest.height < sglw->core.height)
    {
	parentRequest.height = sglw->core.height;
    }
*/


#if GEOM_MAN
    printf("GM: something interesting - after recalcsize height=%d\n",parentRequest.height);
#endif
    //New version 19.05.04
    //minor changes
/*    if (parentRequest.width < sglw->core.width &&
	 (sglw->core.width - parentRequest.width < SGL_NumColumns(sglw)))
    {
	parentRequest.width = sglw->core.width;
    }
    if (parentRequest.height < sglw->core.height &&
	 (sglw->core.width - parentRequest.width < SGL_NumColumns(sglw)))
    {
	parentRequest.height = sglw->core.height;
    }*/


#if GEOM_MAN
    printf("Before RecalcSize: parentWidth=%d, parentHeight=%d\n",sglw->core.width, sglw->core.height);
    printf("After RecalcSize (parentRequest): parentWidth=%d parentHeight=%d\n", parentRequest.width, parentRequest.height);
#endif

    /* Ask the parent if the new calculated size is acceptable. */
/*    parentRequest.request_mode = CWWidth | CWHeight;

    if (request->request_mode & XtCWQueryOnly)
    {
	printf ("Where am I?\n");
	parentRequest.request_mode |= XtCWQueryOnly;
    }*/
/*	
    result = XtMakeGeometryRequest((Widget)sglw, &parentRequest, NULL);
*/
/*
    if (!(request->request_mode & CWWidth))
    {
	parentRequest.width = con->prefWidth;
#if GEOM_MAN	
	printf("parentRequest.width changed to %d\n",parentRequest.width);
#endif	
    }
    
    if (!(request->request_mode & CWHeight))
    {
	parentRequest.height = con->prefHeight;
#if GEOM_MAN	
	printf("parentRequest.height changed to %d\n",parentRequest.height);
#endif	
    }
*/
/*
    if (parentRequest.width == sglw->core.width && parentRequest.height==sglw->core.height)
    {
	result = XtGeometryYes;
    }
    else
    {*/

#if GEOM_MAN
	printf("GM: make resize request for grid. New width=%d, height=%d\n",
		parentRequest.width, parentRequest.height);
#endif	

	result = XtMakeResizeRequest((Widget)sglw, parentRequest.width, parentRequest.height, width_return, height_return);
	
//    }
    /* Turn XtGeometryAlmost in XtGeometryNo. */
    if (result == XtGeometryAlmost)
    {
#if GEOM_MAN
	printf("GM: Result Almost...\n");
#endif	
    	result = XtGeometryNo;
    }

    if (result == XtGeometryNo)
    {
#if GEOM_MAN    
	printf("No...\n");
#endif	
    	result = XtGeometryNo;
    }
	
    if (result == XtGeometryNo ||
	request->request_mode & XtCWQueryOnly)
    {
	/* Restore original geometry. */
#if GEOM_MAN
	printf("GM: prefWidth restored\n");
#endif	
	con->prefWidth = curWidth;
	con->prefHeight = curHeight;
	w->core.border_width = curBW;
    }
//    else /* set new pref sizes */
/*    {
	if ((con->fillType & 1) == 0)
	    w->core.width = con->prefWidth ;
	if ((con->fillType & 2) == 0)
	    w->core.height = con->prefHeight;
    }*/
    
    /* 2fix: GM must not change child size, but I'll fix it later*/
    if (result == XtGeometryYes)
    {
/*	height = con->prefHeight;
	width = con->prefWidth;
	
	if (!(request->request_mode & CWWidth))
        {
	    width = w->core.width;
	}
    
	if (!(request->request_mode & CWHeight))
        {
	    height = w->core.height;
	}
	XtResizeWidget(w, width, height, w->core.border_width);*/
#if GEOM_MAN	
	printf("GM: Result YES - Call RecountCoordinates\n");
#endif	
	RecountCoordinates(sglw);
	//changed 23.01.06
//	XtResizeWidget((Widget)sglw, *width_return, *height_return, sglw->core.border_width);
    }
    
#if DEBUG_MODE
    printf("Geometry manager finished.\n");
#endif    
    return result;
}

#if 0
static void FixWidget(XmSepGridLayoutWidget m/*,
			Widget w*/)
{
    //printf("FixWidget - RecountCoordinates\n");
    RecountCoordinates(m);
}
#endif

static void InsertChild(Widget w)
{
#if 0
    XmSepGridLayoutWidget m = (XmSepGridLayoutWidget) XtParent(w);
#endif    
    
    /* use composite class insert proc to do all the dirty work */
    {

	XtWidgetProc insert_child;
	
	#if DEBUG_MODE    
	    printf("InsertChild started!\n");
	#endif
	
//	_XmProcessLock();
	insert_child = ((XmManagerWidgetClass)xmManagerWidgetClass)->
				composite_class.insert_child;
//	_XmProcessUnlock();
	(*insert_child)(w);
    }
    
    /* now change the subwidget */
#if 0    
    FixWidget(m/*, w*/);
#endif
  
    return;
}

static void ConstraintInitialize(
			__attribute__((unused)) Widget req,//unused
			Widget nw,
			__attribute__((unused)) ArgList args,//unused
			__attribute__((unused)) Cardinal *num_args)//unused
{
    XmSepGridLayoutConstraint nc;
    XmSepGridLayoutWidget par;
    //int delta;
    //int x,y;
    int i, /*j,*/ numx, numy;
    //Dimension width, height;
    
#if DEBUG_MODE
    puts("");
    puts("Constraint initialize started.");
    printf("Num children = %d\n",SGL_NumChildren(par));
#endif    

    nc = GetSGLConstraint(nw);
    par = (XmSepGridLayoutWidget)XtParent(nw);

#if CONST_INIT
    printf("Const_init: SGL_NumRows = %d\n",SGL_NumRows(par));
#endif
    
    /*if one of parametres is default (-1) the other must be default too*/
    if (nc->num_cell_x == -1) nc->num_cell_y = -1;
    if (nc->num_cell_y == -1) nc->num_cell_x = -1;

    //find a position for child if needed
    if (nc->num_cell_x == -1||nc->num_cell_y == -1)
    {	

#if CONST_INIT
	    printf("SGL_NumChildren = %d, SGL_NumColumns = %d, SGL_NumRows = %d\n",SGL_NumChildren(par),SGL_NumColumns(par),SGL_NumRows(par));
#endif    
    
	//if where is empty space
	if (SGL_NumChildren(par) < SGL_NumColumns(par)*SGL_NumRows(par))
	{
#if CONST_INIT
	    printf("Searching a free cell...\n");
#endif	    
	    for (i = 0; i < SGL_NumRows(par)*SGL_NumColumns(par);i++)
	    {
	        if (SGL_Children(par)[i] == NULL)
	        {
		    /* put widget into the cell*/
		    nc->num_cell_x =  i % SGL_NumColumns(par);
		    nc->num_cell_y =  i / SGL_NumColumns(par);//old == i / NumRows ??? fixed 20.08.03(bug#3)
		    break;//!!!!!!!!!!!!!!! fixed 12.03.03
		}
	    }
	}
	//if where is no empty space
	else
	{
	    nc->num_cell_x = 0;
	    nc->num_cell_y = SGL_NumRows(par);//first cell in next(new) row
	}
    }
    
    //check if the cell is not empty, place the widget away
    i = nc->num_cell_y * SGL_NumColumns(par) + nc->num_cell_x;
    if (i >= 0 && i < SGL_NumChildren(par) && SGL_Children(par)[i]!=NULL)
    {
	nc->num_cell_x = -1;
	nc->num_cell_y = -1;
    }

#if CONST_INIT
    printf("SGL_NumColumns = %d, num_x=%d, SGL_NumRows = %d, num_y=%d\n",
	    SGL_NumColumns(par), nc->num_cell_x, SGL_NumRows(par), nc->num_cell_y);
#endif 

    //check if we have to recount the grid
    if (nc->num_cell_x>=SGL_NumColumns(par) || nc->num_cell_y>=SGL_NumRows(par))//have to enlarge the grid
    {
#if CONST_INIT
	    printf("Have to enlarge the grid\n");
#endif	    
	if (SGL_ResizeFlag(par))//able to resize
	{
	    if (nc->num_cell_x>=SGL_NumColumns(par)) //we have to add column(s)
		numx = nc->num_cell_x+1;
	    else
		numx = SGL_NumColumns(par);
	    
	    if (nc->num_cell_y>=SGL_NumRows(par)) //we have to add column(s)
		numy = nc->num_cell_y+1;
	    else
		numy = SGL_NumRows(par);
	    	    
	    RecountGrid(par, numx, numy);
	}
	else
	{
	    //place the widget away
	    nc->num_cell_x = -1;
	    nc->num_cell_y = -1;
	}
    }
    
    //put the widget into the cell
    if (nc->num_cell_x == -1 || nc->num_cell_y == -1)//away
    {
	XtMoveWidget(nw, -nw->core.width, -nw->core.height);
    }
    else
    {
	i = nc->num_cell_y * SGL_NumColumns(par) + nc->num_cell_x;
	if (!SGL_Children(par)[i])    
	{
	    //add a child
	    SGL_Children(par)[i] = nw;
	    
	    SGL_NumChildren(par)++;
#if CONST_INIT  	    
	    printf("NumChildren Inc! new value=%d\n",SGL_NumChildren(par));
#endif
	}
    }


#if CONST_INIT  
    printf("ConstraintInitialize - RecountCoordinates\n");
#endif      
    
    /* set constraint resources: prefSize */
#if DEBUG_MODE    
    printf("ConstInit: prefWidth changed form %d to %d\n",nc->prefWidth,nw->core.width);
#endif    
    nc->prefWidth = nw->core.width;
    nc->prefHeight = nw->core.height;
    
    //decrease margin's if they exist
    if (nc->num_cell_x != -1 && nc->num_cell_y != -1)//child in a grid
    {
	if (SGL_AddH(par)<=nc->prefHeight/2) SGL_AddH(par)=0;
	else SGL_AddH(par) -= nc->prefHeight/2;
	
	if (SGL_AddW(par)<=nc->prefWidth/2) SGL_AddW(par)=0;
	else SGL_AddW(par) -= nc->prefWidth/2;
#if ADDW
	printf("Const init ADDW: new value = %d\n",SGL_AddW(par));
#endif
#if ADDH
	printf("Const_init ADDH: new value = %d\n",SGL_AddH(par));
#endif	

    }

#if CONST_INIT  
    printf("SGL_AddW = %d\n",SGL_AddW(par));  
    printf("Prew w set to %d\n", nc->prefWidth);
    printf("pref: width = %d height = %d\n", nc->prefWidth, nc->prefHeight);
#endif


    /* recount Coordinates of all children in the grid with new child */
    RecountCoordinates(par);

#if CONST_INIT	    
    printf("Constraint initialize done.\n");
#endif    
#if DEBUG_MODE
    printf("After constraint initilaize columns = %d, rows = %d Num chuildren=%d\n",
	    SGL_NumColumns(par),SGL_NumRows(par),SGL_NumChildren(par));
#endif
}

static void RecountGrid(XmSepGridLayoutWidget w, int numx, int numy)
{
    int oldy, oldx, x, y;
    int delta, shiftArray, i, j;
    Widget* temp;

    oldx = SGL_NumColumns(w);
    oldy = SGL_NumRows(w);

#if DEBUG_MODE
    printf("\nRecount grid started\n");
    printf("\n");
#endif

#if RECOUNT_GRID
    printf("Recount grid: initial size x = %d, y = %d\n",oldx, oldy);
    printf("Recount grid: new size will be x=%d y=%d\n", numx, numy);
#endif    

#if RECOUNT_GRID  
    for (i = 0; i < SGL_NumRows(w)*SGL_NumColumns(w); i++)
    {	
	if (SGL_Children(w)[i]!=NULL)
	{
	    printf("Recount grid: Child[%d].x = %d, y = %d\n ",i, SGL_Children(w)[i]->core.x,
		    SGL_Children(w)[i]->core.y);
	}
	if (SGL_Children(w)[i]) printf("Recount grid: i = %d OK!\n",i); 
	else printf("Recount grid: i = %d Empty!\n",i);
    }
#endif  

    if (numx > SGL_NumColumns(w))
    {
	shiftArray = numx - oldx;
        /* increase width (add cells)*/
#if RECOUNT_GRID
	printf("Recount grid: increase numColumns from %d to %d\n", SGL_NumColumns(w), numx);
#endif	
	SGL_NumColumns(w) = numx;
//	printf("SGL_MaxColumns = %d\n",SGL_MaxColumns(w));
	if (numx > SGL_MaxColumns(w))
	{
	    SGL_CoordinatesX(w) = (Dimension*) realloc(SGL_CoordinatesX(w),
        	sizeof(Dimension) * numx);
	    SGL_Children(w) = (Widget*) realloc(SGL_Children(w),
		    sizeof(Widget) * numx * oldy);
/*	    printf("SGL_Children realloced to %d size\n",numx*oldy);
	    for (i = 0; i < numx*oldy; i++)
		if (SGL_Children(w)[i]!=NULL) printf("SGL_Children[%d] = %d\n",
			i, SGL_Children(w)[i]);
*/
	    SGL_MaxColumns(w) = numx;
	}
		
        /* here is the place for entering new X coor values */
        delta = 2 * SGL_MarginW(w) + SGL_SepW(w);
        for (x = oldx; x < numx; x++)
	{
#if RECOUNT_GRID	
	    printf("\n");
	    printf("SGL_CoordinatesX[%d]=%d\n",x,SGL_CoordinatesX(w)[x]);
	    printf("\n");
#endif
	    if (x==0)
	    {
		SGL_CoordinatesX(w)[x]=0;
	    }
	    else if (x>1)
	    {
/*		SGL_CoordinatesX(w)[x] =
    	    	    SGL_CoordinatesX(w)[oldx - 1] +
	    	    delta * (x + 1 - oldx);*/
		SGL_CoordinatesX(w)[x] = SGL_CoordinatesX(w)[x-1] + delta;
	    }
	    else
	    {
/*		SGL_CoordinatesX(w)[x] =
    	    	    SGL_CoordinatesX(w)[oldx - 1] +
	    	    (delta - SGL_SepW(w)) * (x + 1 - oldx);*/
		SGL_CoordinatesX(w)[x] = delta - SGL_SepW(w);
	    }
	}
	
	/*rearrange children*/
	temp = (Widget*) malloc(sizeof(Widget) * oldx * oldy);
#if RECOUNT_GRID
	printf("-----------------------------------------\n");
#endif	
	for (i = 0; i < oldx*oldy; i++)
	{
	    temp[i] = SGL_Children(w)[i];
	    //if (temp[i]) printf("temp i = %d busy\n",i);
	    //if (temp[i] == NULL) printf("temp i = %d empty\n",i);
	    //if (SGL_Children(w)[i] != NULL) printf("sgl child i = %d busy\n",i);
	}
	
#if 0
	/* Old version */
	
	for (i = oldy * oldx; i < oldy * numx; i++)
	    SGL_Children(w)[i] = NULL;/*see notes.txt - 1*/
	/* shift Children array */
    
	i = oldx * oldy - 1;
	//last child in old widget
		    
	j = numx * oldy - 1 - shiftArray;
	//j - place for last child in new widget
		    
	for (y = oldy - 1; y > 1; y--)
	{
    	    for (x = oldx-1; x > 0; x--)
    	    {
        	SGL_Children(w)[j] = SGL_Children(w)[i];
        	i--;
        	j--;
    	    }
    	    j -= shiftArray;
	}
	/* End of old version*/
#endif
	
	/* shift Children array */
#if RECOUNT_GRID
	printf("shift Children array...\n");
#endif
	i = 0;
	j = 0;
		    
	for (y = 0; y < oldy; y++)
	{
    	    for (x = 0; x < oldx; x++)
    	    {
//		printf("x = %d, y = %d\n",x,y);
//		if (temp[i] == NULL) printf("temp i = %d empty j = %d\n",i,j);
        	SGL_Children(w)[j] = temp[i];
        	i++;
        	j++;
    	    }
	    for (x = 0; x < shiftArray; x++)
    	    {
//		printf("*x = %d, y = %d\n",x,y);
        	SGL_Children(w)[j] = NULL;
        	j++;
    	    }
	}
	free(temp);

#if RECOUNT_GRID
	printf("Children array shifted\n");
#endif 
    }
    else numx = oldx;
    
    if (numy > SGL_NumRows(w))
    {
        /* increase */
#if RECOUNT_GRID
	printf("increase numRows from %d to %d\n",SGL_NumRows(w),numy);
#endif
	SGL_NumRows(w) = numy;
	if (numy > SGL_MaxRows(w))
	{
	    SGL_CoordinatesY(w) = (Dimension*) realloc(SGL_CoordinatesY(w),
        	sizeof(Dimension) * numy);
	    SGL_Children(w) = (Widget*) realloc(SGL_Children(w),
			        sizeof(Widget) * numx * numy);
	    SGL_MaxRows(w) = numy;//old - maxColumns??? fixed 20.08.03 = bug#3
	}
        /* here is the place for entering new Y coor values */
        delta = 2 * SGL_MarginH(w) + SGL_SepH(w);
	
        for (y = oldy; y < numy; y++)
	{
	    if (y == 0) SGL_CoordinatesY(w)[y]=0;
    	    else if (y > 1)
	    {
/*		SGL_CoordinatesY(w)[y] =
        	    SGL_CoordinatesY(w)[oldy - 1] +
        	    delta * (y + 1 - oldy);*/
		SGL_CoordinatesY(w)[y] = SGL_CoordinatesY(w)[y-1] + delta;
	    }
	    else
	    {
/*		SGL_CoordinatesY(w)[y] =
        	    SGL_CoordinatesY(w)[oldy - 1] +
        	    (delta - SGL_SepH(w)) * (y + 1 - oldy);*/
		SGL_CoordinatesY(w)[y] = delta - SGL_SepH(w);
	    }
	}
	for (i = oldy * numx; i < numx * numy; i++)
            SGL_Children(w)[i] = NULL;/*see notes.txt - 1*/
    }
#if RECOUNT_GRID
    printf("Recount grid: final size x = %d, y = %d\n",numx, numy);

    for (x = 0; x < SGL_NumColumns(w); x++)
    {
	if (SGL_CoordinatesX(w)) printf("x coor[%d] = %d\n",
	    x,SGL_CoordinatesX(w)[x]);
    }

    for (y = 0; y < SGL_NumRows(w); y++)
    {
	if (SGL_CoordinatesY(w)) printf("y coor[%d] = %d\n",
	    y,SGL_CoordinatesY(w)[y]);
    }
#endif    

#if DEBUG_MODE
    printf("\nRecount grid finished\n");
    printf("\n");
#endif
}

static void RecountCoordinates(XmSepGridLayoutWidget par)
{
    int i, x, y, delta;
    int *MaxWidth, *MaxHeight;
    int curWidth, curHeight, w, h;
    register XmSepGridLayoutConstraint con;
    
    /*for XtMakeGeometryRequest*/
//    XtWidgetGeometry request/*, reply_return*/;
    XtGeometryResult res;
    
    Dimension width, height, *width_return, *height_return;
    
    struct _Coordinates
    {
	Dimension x;
	Dimension y;
    }; 
    typedef struct _Coordinates WidgetCoordinates;
    
    WidgetCoordinates* wc;
#if DEBUG_MODE
    puts("");
    puts("Recount coordinates started");
    printf("Num children = %d\n",SGL_NumChildren(par));
#endif
    
    /* allocate memory for buffer coordinates arrays*/
    if (!(wc = (WidgetCoordinates*) malloc(sizeof(WidgetCoordinates)*
			SGL_NumRows(par)*SGL_NumColumns(par))))
    {
	perror("Malloc error: WidgetCoordinates==NULL\n");
	return;
    }
    
    /* allocate memory for space arrays*/
    if (!(MaxWidth = (int*) malloc(sizeof(int)*SGL_NumColumns(par))))
    {
    	perror("Malloc error: MaxWidth==NULL\n");
	free(wc);
	return;//probably exit(1)
    }
    
    if (!(MaxHeight = (int*) malloc(sizeof(int)*SGL_NumRows(par))))
    {
	perror("Malloc error: MaxHeight==NULL\n");
	free(MaxWidth);
	free(wc);
	return;
    }
#if RECOUNT_STAT
    printf(" RC: old grid - %d columns, %d rows, width = %d, height = %d corew=%d coreh=%d\n", 
	    SGL_NumColumns(par), SGL_NumRows(par),
	    SGL_Width(par), SGL_Height(par),
	    par->core.width, par->core.height);
#endif    

    /* get the space between columns*/
    for (x = 0;x < SGL_NumColumns(par); x++)
    {
	MaxWidth[x] = 0;//margin*2 will be added later
	for (y = 0; y < SGL_NumRows(par); y++)
	{
	    i = x + y * SGL_NumColumns(par);
	    if (SGL_Children(par)[i] && SGL_Children(par)[i]->core.managed)/*if there is a child*/
	    {
		con = GetSGLConstraint(SGL_Children(par)[i]);
		
		/* next if was added to control the form's size changing,
		that comes from the ChangeManaged method 
		TO DO: make it more elegant*/
		//commented 16.02.06
		if (!XtIsRealized(par) && SGL_Children(par)[i]->core.width > con->prefWidth &&
		    (con->fillType&1)==0)
		    con->prefWidth = SGL_Children(par)[i]->core.width;
		    
		/* interesting place - what if prefWidth > core.width?:)*/
		/* added 5 June 2004 */
		//commented 26.12.05
	//	if (SGL_Children(par)[i]->core.width < con->prefWidth)
	//	    con->prefWidth = SGL_Children(par)[i]->core.width;
		/* ---- end of addition 5 June 2004 -----*/
		
		if (MaxWidth[x] < con->prefWidth)
		    MaxWidth[x] = con->prefWidth;
#if RECOUNT_STA		    
		printf("MaxWidth[%d]=%d, prefWidth = %d",x,MaxWidth[x],con->prefWidth);		    
#endif		
	    }
	    
	}
#if RECOUNT_STAT	
	printf("Max child [%d] width = %d\n",x, MaxWidth[x]);
#endif	
        MaxWidth[x]+=(SGL_MarginW(par) + SGL_AddW(par))*2;
#if RECOUNT_STAT	
	printf("After changes: Max child [%d] width = %d\n",x, MaxWidth[x]);
#endif
    }

    /* get the space between rows*/
    for (y = 0;y < SGL_NumRows(par); y++)
    {
	MaxHeight[y] = 0;//margin*2 will be added later
	for (x = 0; x < SGL_NumColumns(par); x++)
	{
	    i = x + y * SGL_NumColumns(par);
	    if (SGL_Children(par)[i] && SGL_Children(par)[i]->core.managed)/*if there is a child*/
	    {
		con = GetSGLConstraint(SGL_Children(par)[i]);
		
		/* next if was added to control the form's size changing,
		that comes from the ChangeManaged method 
		TO DO: make it more elegant*/
		
//		printf("Child [%d,%d], height = %d, pref h=%d\n",x,y,
//		    SGL_Children(par)[i]->core.height, con->prefHeight);
	
		//commented 16.02.06	
		if (!XtIsRealized(par) && SGL_Children(par)[i]->core.height > con->prefHeight &&
		    (con->fillType&2)==0)
		    con->prefHeight = SGL_Children(par)[i]->core.height;
		    
		/* interesting place - what if prefHeight > core.height?:)*/
		/* added 5 June 2004 */
		//commented 26.12.05
	//	if (SGL_Children(par)[i]->core.height < con->prefHeight)
	//	    con->prefHeight = SGL_Children(par)[i]->core.height;
		/* ---- end of addition 5 June 2004 -----*/
		
		if (MaxHeight[y] < con->prefHeight)
		    MaxHeight[y] = con->prefHeight;
#if RECOUNT_STAT
		printf("RecountCoordiantes: Max height[%d,%d]=%d\n",x,y,MaxHeight[y]);
#endif
	    }
	}
        MaxHeight[y]+=(SGL_MarginH(par)+SGL_AddH(par))*2;
    }
    
    /* recount separators coordinates*/
    
    for (x = 1; x < SGL_NumColumns(par); x++)
    {
	if (x != 1)
	{
	    SGL_CoordinatesX(par)[x] = SGL_CoordinatesX(par)[x-1] + MaxWidth[x-1] 
				    + SGL_SepW(par);
	}
	else
	{
	    SGL_CoordinatesX(par)[x] = SGL_CoordinatesX(par)[x-1] + MaxWidth[x-1];
	}
    }
    /*
    for (x = 0; x < SGL_NumColumns(par); x++)
    {
	if (x != 0)
	{
	    SGL_CoordinatesX(par)[x] = SGL_CoordinatesX(par)[x-1] + MaxWidth[x-1] 
				    + SGL_SepW(par);
	}
	else
	{
	    SGL_CoordinatesX(par)[x] = SGL_CoordinatesX(par)[x-1] + MaxWidth[x-1];
	}
    }*/
    for (y = 1; y < SGL_NumRows(par); y++)
    {
	if (y != 1)
	{
	    SGL_CoordinatesY(par)[y] = SGL_CoordinatesY(par)[y-1] + MaxHeight[y-1]
				    + SGL_SepH(par);
	}
	else
	{
	    SGL_CoordinatesY(par)[y] = SGL_CoordinatesY(par)[y-1] + MaxHeight[y-1];
	}
    }
/*    for (y = 0; y < SGL_NumRows(par); y++)
    {
	if (y != 0)
	{
	    SGL_CoordinatesY(par)[y] = SGL_CoordinatesY(par)[y-1] + MaxHeight[y-1]
				    + SGL_SepH(par);
	}
	else
	{
	    SGL_CoordinatesY(par)[y] = SGL_CoordinatesY(par)[y-1] + MaxHeight[y-1];
	}
    }*/


// this code was moved here 27.12.05
     /* recount window size */
    x = SGL_NumColumns(par) - 1;
    y = SGL_NumRows(par) - 1;

    //here was a bug 011104 - it was not took into account that there could be 
    //only ONE row/column
    if (SGL_NumColumns(par)>1)
    {
	SGL_Width(par) = SGL_CoordinatesX(par)[x] + MaxWidth[x] +  SGL_SepW(par);
    }
    else
    {
	SGL_Width(par) = SGL_CoordinatesX(par)[x] + MaxWidth[x];
    }
    
    if (SGL_NumRows(par)>1)
    {
	SGL_Height(par) = SGL_CoordinatesY(par)[y] + MaxHeight[y] +  SGL_SepH(par);
    }
    else
    {
	SGL_Height(par) = SGL_CoordinatesY(par)[y] + MaxHeight[y];
    }
#if RECOUNT_STAT
    printf("!!! Recount_Coordinates SGL_Height=%d %x\n",SGL_Height(par),par);
#endif     
//end of moved here
    
    /* recount widget coordinates and sizes*/
    for (x = 0;x < SGL_NumColumns(par); x++)
	for (y = 0; y < SGL_NumRows(par); y++)
	{
	    i = x + y * SGL_NumColumns(par);
	    if (SGL_Children(par)[i])/*if there is a child*/
	    {
		con = GetSGLConstraint(SGL_Children(par)[i]);
		if ((con->fillType & 1) == 0 )/* no horizontal fill */
		{
/*		    delta = (MaxWidth[x]  - 2*SGL_MarginW(par) - SGL_Children(par)[i]->core.width) * 
			(1 + shiftX[con->gravity - 1])/2 + SGL_MarginW(par);*/
//changed 26.12.05
		    delta = (MaxWidth[x]  - 2*SGL_MarginW(par) - con->prefWidth) * 
			(1 + shiftX[con->gravity - 1])/2 + SGL_MarginW(par);			
		    //SGL_Children(par)[i]->core.width = con->prefWidth;
		    //that's wrong!!!
		    //it is unable to write into the core.height/width
	//	    printf("new width = %d\n",con->prefWidth);
		    w = con->prefWidth;//!!!
#if RECOUNT_STAT		    
		    printf("RC: w=%d\n",w);
#endif		    
		    
		    //w = SGL_Children(par)[i]->core.width; //commented 15.09.05
		}
		else
		{
		    delta = SGL_MarginW(par)/* + SGL_SepW(par)*/;
		    //SGL_Children(par)[i]->core.width = MaxWidth[x] - 2*SGL_MarginW(par);
		    //that's wrong!!!
		    //it is unable to write into the core.height/width
		    w = MaxWidth[x] - 2*SGL_MarginW(par);
		}
		/*SGL_Children(par)[i]->core.x = SGL_CoordinatesX(par)[x] + delta;*/
		wc->x = SGL_CoordinatesX(par)[x] + delta;
		if (x>0) wc->x += SGL_SepW(par);
#if RECOUNT_STAT
		printf("SGL_CoordiantesX[%d]=%d\n",x,SGL_CoordinatesX(par)[x]);
		printf("Recount cordinates: delta = %d, widget[%d].x = %d\n",
			delta, i,wc->x);
#endif
#if RECOUNT_STAT
		printf("Delta = %d\n", delta);
#endif		

		if ((con->fillType & 2) == 0) /* no vertical fill */
		{
/*		    delta = (MaxHeight[y] - 2*SGL_MarginH(par) - SGL_Children(par)[i]->core.height) *
			(1 + shiftY[con->gravity - 1])/2 + SGL_MarginH(par);*/
//changed 26.12.05
		    delta = (MaxHeight[y] - 2*SGL_MarginH(par) - con->prefHeight) *
			(1 + shiftY[con->gravity - 1])/2 + SGL_MarginH(par);		    	
		    //h = SGL_Children(par)[i]->core.height;
		    h = con->prefHeight; //changed 15.09.05
#if RECOUNT_STAT		    
		    printf("prefHeight = %d\n",con->prefHeight);
#endif
		}
		else
		{
		    delta = SGL_MarginH(par);
		    
		    h = MaxHeight[y] - 2*SGL_MarginH(par);
		}

		wc->y = SGL_CoordinatesY(par)[y] + delta;
		if (y>0) wc->y += SGL_SepH(par);

#if RECOUNT_STAT		
		printf("Recount cordinates: delta = %d, widget[%d].x=%d widget[%d].y=%d w=%d h=%d\n",
			delta, i,wc->x, i, wc->y, w, h);
			
		printf("RC: widget[%d] core.w=%d, core.h=%d grid core.width=%d %d\n", i, SGL_Children(par)[i]->core.width,
		    SGL_Children(par)[i]->core.height, par->core.width, SGL_Width(par));
		if (XtIsRealized(SGL_Children(par)[i])) printf("Realized\n"); else printf("Not Realized\n");
		if (XtIsManaged(SGL_Children(par)[i])) printf("Managed\n");else printf("Not Managed\n");
#endif

		//added 27.12.05 - wrong!!!
//		if (SGL_Width(par) < par->core.width) SGL_Width(par) = par->core.width;
//		if (SGL_Height(par) < par->core.height) SGL_Height(par) = par->core.height;
#if RECOUNT_STAT
    printf("!!! 2 RecountCoordinates SGL_Height=%d %x\n",SGL_Height(par),par);
#endif 		
		//end of addition
#if RECOUNT_STAT		
		printf("Core.width = %d, SGL_width = %d\n",par->core.width, SGL_Width(par));
#endif
		
		XtConfigureWidget(SGL_Children(par)[i], wc->x, wc->y, w, h,
		    SGL_Children(par)[i]->core.border_width);

	    }
	}
    
    
    //moved up
    
    /* recount window size */
//    x = SGL_NumColumns(par) - 1;
//    y = SGL_NumRows(par) - 1;

    //here was a bug 011104 - it was not took into account that there could be 
    //only ONE row/column
/*    if (SGL_NumColumns(par)>1)
    {
	SGL_Width(par) = SGL_CoordinatesX(par)[x] + MaxWidth[x] +  SGL_SepW(par);
    }
    else
    {
	SGL_Width(par) = SGL_CoordinatesX(par)[x] + MaxWidth[x];
    }
    
    if (SGL_NumRows(par)>1)
    {
	SGL_Height(par) = SGL_CoordinatesY(par)[y] + MaxHeight[y] +  SGL_SepH(par);
    }
   
*/    
    //moved up
    
    /* make geometry request*/
    curWidth = par->core.width;
    curHeight = par->core.height;

    width = SGL_Width(par);
    height = SGL_Height(par);
    
    width_return = (Dimension*) malloc(sizeof(Dimension));
    height_return = (Dimension*) malloc(sizeof(Dimension));
    *width_return = 0;
    *height_return = 0;
    
#if RECOUNT_STAT    
    printf("RC: Cur window %d x %d. Making request: %d x %d\n", curWidth, curHeight,
	    width, height);
    printf("RC: width_return = %p height_ret = %p\n",width_return, height_return);
    printf("RC: *width_return = %d *height_ret = %d\n",*width_return, *height_return);
    
#endif    
    
    if (width!=curWidth || height!=curHeight)    //old and new values differ
    {
	if (width==0) width=1;
	if (height==0) height=1;
	res = XtMakeResizeRequest((Widget)par, width, height, width_return, height_return);
    }
    else
    {
	res = XtGeometryYes;
	*width_return = width;
	*height_return = height;
    }
    
#if RECOUNT_STAT    
    printf("RC: AFter - width_return = %p height_ret = %p\n",width_return, height_return);
    printf("RC: After - *width_return = %d *height_ret = %d\n",*width_return, *height_return);
    
#endif     
    
    if (res == XtGeometryNo)
     {
#if RECOUNT_STAT
	printf("RC: Geom reply = No\n");
#endif 	
    }
    
    if (res == XtGeometryAlmost)
    {
#if RECOUNT_STAT
	printf("RC: Geom reply = Almost\n");
#endif 	
	res = XtGeometryNo;
    }
    if (res == XtGeometryYes)
    {
#if RECOUNT_STAT
	printf("RC: Geom reply = YES\n");
#endif    
	XtResizeWidget((Widget)par, *width_return, *height_return, par->core.border_width);
    }
    else
    {
#if RECOUNT_STAT
	printf("RC: Geom reply = not Yes:(\n");
#endif     
	//commented 20.02.06
	//XtResizeWidget((Widget)par, curWidth, curHeight, par->core.border_width);	
    }
    
#if RECOUNT_STAT
    for (i = 0;i < SGL_NumRows(par)*SGL_NumColumns(par); i++)
    {	
	if (SGL_Children(par)[i]!=NULL)
	{
	    printf("Child[%d].x = %d, y = %d\n ",i, SGL_Children(par)[i]->core.x,
		    SGL_Children(par)[i]->core.y);
	    printf("Child[%d].width = %d, height = %d\n ",i, SGL_Children(par)[i]->core.width,
		    SGL_Children(par)[i]->core.height);
	}
    }
#endif    
    
    /* free memory */
    free(MaxWidth);
    free(MaxHeight);
    free(wc);
    
#if DEBUG_MODE
    printf("Recount coordinates finished\n");
    printf("Num children = %d\n",SGL_NumChildren(par));
    printf("Core height = %d\n",par->core.height);
#endif    
    
}

static Boolean	ConstraintSetValues(
			register Widget old,
			__attribute__((unused)) register Widget ref,//unused
			register Widget new_w,
			__attribute__((unused)) ArgList args,//unused
			__attribute__((unused)) Cardinal *num_args)//unused
{
    //int num,i;//for debugging
    
    XmSepGridLayoutWidget par;
    register XmSepGridLayoutConstraint oldc, newc;

#if CON_SetValues    
    int delta,x,y,i,temp;
#endif    

    int numx, numy;    

#if DEBUG_MODE
    printf("Constraint set values started \n");
#endif

     par = (XmSepGridLayoutWidget) XtParent(new_w);
    
#if CON_SetValues  
    printf("SGL_NumColumns = %d, SGL_NumRows = %d\n",SGL_NumColumns(par),
    SGL_NumRows(par));
    for (i = 0; i < SGL_NumRows(par)*SGL_NumColumns(par); i++)
    {	
	if (SGL_Children(par)[i]) printf("CSV i = %d OK!\n",i); 
	else printf("CSV i = %d Empty!\n",i);
    }
#endif
    
    oldc = GetSGLConstraint(old);
    newc = GetSGLConstraint(new_w);
    
#if CON_SetValues
    printf("CSV: Old numCellX = %d, Old numCellY = %d\n",
	    oldc->num_cell_x, oldc->num_cell_y);
    printf("CSV: New numCellX = %d, New numCellY = %d\n",
	    newc->num_cell_x, newc->num_cell_y);
    printf("CSV: Old pref width = %d, Old pref height = %d\n",
	    oldc->prefWidth, oldc->prefHeight);
    printf("CSV: New pref width = %d, New pref height = %d\n",
	    newc->prefWidth, newc->prefHeight);
    printf("CSV: Old fill type = %d, old fill type = %d\n",
	    newc->fillType, oldc->fillType);
    printf("CSV: new gravity = %d, old gravity = %d\n",
	    newc->gravity, oldc->gravity);
#endif

    /*printf("Constraints: oldc : numx = %d, numy = %d\n",oldc->num_cell_x,
	oldc->num_cell_y);
    printf("Constraints: newc : numx = %d, numy = %d\n",newc->num_cell_x,
	newc->num_cell_y);*/
    
    if (newc->fillType != oldc->fillType)
    {
	/* do some check */
#if CON_SetValues
    printf("CSV: reason - change fillType from %d to %d\n", oldc->fillType, newc->fillType);
#endif
    }
    
    if (newc->gravity!= oldc->gravity)
    {
	if (newc->gravity<1 || newc->gravity>9)	
	    newc->gravity = oldc->gravity;
	//added 9.10.04
	RecountCoordinates(par);
	//---
    }
    
    
    if (newc->prefWidth!= oldc->prefWidth 
	|| newc->prefHeight!=oldc->prefHeight)
    {
#if CON_SetValues    
	printf("CSV: reason - change prefWidth/Height from %d/%d to %d/%d\n", oldc->prefWidth, oldc->prefHeight,
		newc->prefWidth, newc->prefHeight);
#endif	
    }
    
    if (new_w->core.width!= old->core.width 
	|| new_w->core.height!=old->core.height)
    {
#if CON_SetValues    
	printf("CSV: reason - change Width/Height from %d/%d to %d/%d\n", old->core.width, old->core.height,
		new_w->core.width, new_w->core.height);
#endif	
	if ((newc->fillType&1) == 0)
	{
	    newc->prefWidth = new_w->core.width;
	}
	
	if ((newc->fillType&2) == 0)
	{
	    newc->prefHeight = new_w->core.height;
	}

#if CON_SetValues	
	printf("CSV: New pref width = %d, New pref height = %d\n",
	    newc->prefWidth, newc->prefHeight);
#endif	    
	//added 14.09.05
	//вот тут может глючить фишка с ChangeManaged
	RecountCoordinates(par);
    }
    
    if (newc->num_cell_x!= oldc->num_cell_x
	||newc->num_cell_y!= oldc->num_cell_y)
    {
	par->sep_grid_layout.add_flag = False;//there could be no linear addition now
#if CON_SetValues
	printf("SV:Flag dropped!\n");
#endif	
	/*printf("Old widget coordinates: x = %d, y = %d\n",old->core.x,
				old->core.y);
	printf("removing child from %d\n",oldc->num_cell_x + 
			SGL_NumColumns(par)* oldc->num_cell_y);*/
	SGL_Children(par)[oldc->num_cell_x + SGL_NumColumns(par)*
		    oldc->num_cell_y] = NULL;
		    
	//printf("Child succesfully removed\n");
/* ------------------ Recount coordinates begin -----------------------------*/		    
//	I don't need this already		    
//	printf("recount coordinates 1...\n");
//	RecountCoordinates(par, oldc); 
/*------------------- Recount coordinates end -------------------------------*/	
	
	/* rearrange grid if we need*/
	if (SGL_ResizeFlag(par))
	{
#if CON_SetValues	
	    printf("Resize flag is On\n");
#endif	    
	    if (newc->num_cell_x >= SGL_NumColumns(par))
		numx = newc->num_cell_x+1;
	    else numx = SGL_NumColumns(par);
	    
	    if (newc->num_cell_y >= SGL_NumRows(par))
		numy = newc->num_cell_y+1;
	    else numy = SGL_NumRows(par);
	
	    /*recount grid if need*/
	    if (numx > SGL_NumColumns(par) || 
		numy > SGL_NumRows(par))
	    {
#if CON_SetValues	    
		printf("Constraint set values: recount grid from x = %d, y =%d to x = %d, y = %d\n",
			SGL_NumColumns(par),SGL_NumRows(par),
			numx, numy);
#endif			
		RecountGrid(par, numx, numy);
	    }
	}
#if CON_SetValues	
	else printf("Resize flag is Off\n");
#endif	

	if (newc->num_cell_x < SGL_NumColumns(par) &&
	    newc->num_cell_y < SGL_NumRows(par) &&
	    newc->num_cell_x >= 0 &&
	    newc->num_cell_y >= 0 &&
	    SGL_Children(par)[newc->num_cell_x + SGL_NumColumns(par)*
		    newc->num_cell_y] == NULL)
	{
	    /* insert child into new cell */
	    SGL_Children(par)[newc->num_cell_x + SGL_NumColumns(par)*
			    newc->num_cell_y] = new_w;

	    /* check if we need to recount any */
	    if (!par->sep_grid_layout.recount_flag)
	    {
		/* ---- Recount coordinates begin ----  */		    
		//printf("recount coordinates 2...\n");
#if CON_SetValues    
		printf("CON_SetValues: ConstraintSetValues - RecountCoordinates\n");
#endif  		
		RecountCoordinates(par);
		    
		//new_w->core.x = SGL_CoordinatesX(par)[newc->num_cell_x] + SGL_MarginW(par);
		//new_w->core.y = SGL_CoordinatesY(par)[newc->num_cell_y] + SGL_MarginH(par);
		/*printf("New widget coordinates: x = %d, y = %d\n",new_w->core.x,
				new_w->core.y);*/
		/*----- Recount Coordinates End ------  */		    
	    }
	}
	else
	{
#if CON_SetValues	
	    printf("CON_SetValues: The cell is already busy\n");
#endif	    
	    newc->num_cell_x = -1;
	    newc->num_cell_y = -1;
	    new_w->core.x = 0;
	    new_w->core.y = 0;
#if CON_SetValues
	    printf("CON_SetValues: RecountCoordinates\n");
#endif
	    RecountCoordinates(par);
	}
/* moved to Recount Coordinates	
	new_w->core.border_width ++;
	par->sep_grid_layout.processing_constraints = True;
	*/
    }/*if (newc->num_cell_x < SGL_NumColumns(par) &&
		newc->num_cell_y < SGL_NumRows(par) )*/
#if CON_SetValues
    printf("CON_SetValues: final grid size x = %d, y = %d\n",
	    SGL_NumColumns(par), SGL_NumRows(par));    
#endif	    
    return False;
}
