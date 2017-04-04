#ifndef _XmSepGridLayoutP_h
#define _XmSepGridLayoutP_h

#include "XmSepGridLayout.h"
#include <Xm/BulletinBP.h>
//#include <Xm/ManagerP.h>

#ifdef __cplusplus
extern "C"{
#endif

/* picked up from RowColumnI.h */
#define ForManagedChildren(m, i, q) \
    for (i = 0, q = m->composite.children;\
    i < m->composite.num_children;\
    i++, q++)\
    if (XtIsManaged(*q))

#define SGL_MarginW(m)		m->sep_grid_layout.margin_width
#define SGL_MarginH(m)		m->sep_grid_layout.margin_height
#define SGL_NumRows(m) 		m->sep_grid_layout.num_rows
#define SGL_NumColumns(m) 	m->sep_grid_layout.num_columns
#define SGL_MaxRows(m) 		m->sep_grid_layout.max_rows
#define SGL_MaxColumns(m) 	m->sep_grid_layout.max_columns
#define SGL_CoordinatesX(m) 	m->sep_grid_layout.coordinates_x
#define SGL_CoordinatesY(m) 	m->sep_grid_layout.coordinates_y
#define SGL_SepHType(m)		m->sep_grid_layout.separator_type_h
#define SGL_SepVType(m)		m->sep_grid_layout.separator_type_v
#define SGL_Children(m)		m->sep_grid_layout.children
#define SGL_LastCellX(m)	m->sep_grid_layout.last_num_cell_x
#define SGL_LastCellY(m)	m->sep_grid_layout.last_num_cell_y
#define SGL_Width(m)		m->sep_grid_layout.width
#define SGL_Height(m)		m->sep_grid_layout.height
#define SGL_SepW(m)		m->sep_grid_layout.sep_width
#define SGL_SepH(m)		m->sep_grid_layout.sep_height
#define SGL_AddW(m)		m->sep_grid_layout.add_width
#define SGL_AddH(m)		m->sep_grid_layout.add_height
#define SGL_ResizeFlag(m)	m->sep_grid_layout.resize_flag
#define SGL_NumChildren(m)	m->sep_grid_layout.num_children

typedef struct _XmSepGridLayoutConstraintPart
{
    short num_cell_x;	  /* position (cell number) by X axes */
    short num_cell_y;	  /* the same by the Y axes */
    
    Dimension prefWidth;  /* preferable width for child */
    Dimension prefHeight; /* preferable height for child */
    
    int	fillType;	  /* the way how the child absorb free space
			     in the cell; 0 - no fill, 1 - fill by x axes,
			     2 - fill by y axes, 3 - fill by both axes */
    
    int   gravity ; 	  /* position within cell (i.e. aligment)*/    
} XmSepGridLayoutConstraintPart, *XmSepGridLayoutConstraint;

typedef struct _XmSepGridLayoutConstraintRec
{
    XmManagerConstraintPart manager;
    XmSepGridLayoutConstraintPart sep_grid_layout;
} XmSepGridLayoutConstraintRec, *XmSepGridLayoutConstraintPtr;

/* New fields for XmSepGridLayout widget class record */

typedef struct
{
    XtPointer extension;
} XmSepGridLayoutClassPart;

/* Full class record declaration */

typedef struct _XmSepGridLayoutClassRec
{
    CoreClassPart		core_class;
    CompositeClassPart		composite_class;
    ConstraintClassPart		constraint_class;
    XmManagerClassPart		manager_class;
    XmBulletinBoardClassPart	bulletin_board_class;
    XmSepGridLayoutClassPart	sep_grid_layout_class;
} XmSepGridLayoutClassRec;

externalref XmSepGridLayoutClassRec xmSepGridLayoutClassRec;

/* New fields for SepGridLayout widget record */

typedef struct
{
    short	num_rows;	/* dimension of grid */
    short 	num_columns;

    short	max_rows;	/* memory available */
    short 	max_columns;
    
    short	add_width;	/* resize addition*/
    short	add_height;
    
    short	margin_height;	/* margin around inside of each internal widget */
    short	margin_width;
    
    short	sep_height;	/* space for separators */
    short	sep_width;
    
    Dimension 	width;		/* buffer of widget size*/
    Dimension	height;
    
    short	last_num_cell_x;// the cell where a new child
    short	last_num_cell_y;//will be added
    
    Dimension * coordinates_x;
    Dimension * coordinates_y;
    Widget*    	children;
    int 	num_children;	//current number of children
    
    unsigned char separator_type_h;
    unsigned char separator_type_v;
    GC		separator_GC_h;
    GC		separator_GC_v;
    
    Boolean 	add_flag;
    Boolean	processing_constraints;
    Boolean 	resize_flag;//add cells if needed

    int 	recount_flag;    
} XmSepGridLayoutPart;

/* Full instance record declaration */

typedef struct _XmSepGridLayoutRec
{
    CorePart		core;
    CompositePart	composite;
    ConstraintPart	constraint;
    XmManagerPart	manager;
    XmBulletinBoardPart bulletin_board;
    XmSepGridLayoutPart sep_grid_layout;
}XmSepGridLayoutRec;


#ifdef __cplusplus
}
#endif

#endif
