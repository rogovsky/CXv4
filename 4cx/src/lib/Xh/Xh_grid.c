#include "Xh_includes.h"

#include "XmSepGridLayout.h"


CxWidget  XhCreateGrid(CxWidget parent, const char *name)
{
    return ABSTRZE(XtVaCreateManagedWidget(name, xmSepGridLayoutWidgetClass, CNCRTZE(parent),
//                                           XmNmarginWidth,  1,
//                                           XmNmarginHeight, 1,
                                           XmNsepHType,     XmSHADOW_ETCHED_IN_DASH,
                                           XmNsepVType,     XmSHADOW_ETCHED_IN_DASH,
//                                           XmNresFlag,      False,
//                                           XmNrows,         10,
//                                           XmNcolumns,      10,
                                           NULL));
}

void      XhGridSetPadding  (CxWidget w, int hpad, int vpad)
{
    if (hpad >= 0) XtVaSetValues(CNCRTZE(w), XmNmarginWidth,  hpad, NULL);
    if (vpad >= 0) XtVaSetValues(CNCRTZE(w), XmNmarginHeight, vpad, NULL);
}

void      XhGridSetSpacing  (CxWidget w, int hspc, int vspc)
{
    if (hspc >= 0) XtVaSetValues(CNCRTZE(w), XmNsepWidth,  hspc, NULL);
    if (vspc >= 0) XtVaSetValues(CNCRTZE(w), XmNsepHeight, vspc, NULL);
}

void      XhGridSetGrid     (CxWidget w, int horz, int vert)
{
  unsigned char  hst = XmNO_LINE;
  unsigned char  vst = XmNO_LINE;

    if (horz < 0) hst = XmSHADOW_ETCHED_IN_DASH;
    if (vert < 0) vst = XmSHADOW_ETCHED_IN_DASH;
    if (horz > 0) hst = XmSHADOW_ETCHED_IN;
    if (vert > 0) vst = XmSHADOW_ETCHED_IN;
  
    XtVaSetValues(CNCRTZE(w), XmNsepHType, hst, XmNsepVType, vst, NULL);
}

void      XhGridSetSize     (CxWidget w, int cols, int rows)
{
    if (cols >= 0) XtVaSetValues(CNCRTZE(w), XmNcolumns,  cols, NULL);
    if (rows >= 0) XtVaSetValues(CNCRTZE(w), XmNrows,     rows, NULL);
}

void      XhGridSetChildPosition (CxWidget w, int  x,   int  y)
{
    XtVaSetValues(CNCRTZE(w),
                  XmNcellX, x,
                  XmNcellY, y,
                  NULL);
}

void      XhGridGetChildPosition (CxWidget w, int *x_p, int *y_p)
{
  short  x, y;
    
    XtVaGetValues(CNCRTZE(w),
                  XmNcellX, &x,
                  XmNcellY, &y,
                  NULL);
    
    if (x_p != NULL) *x_p = x;
    if (y_p != NULL) *y_p = y;
}

static int sign_of(int x)
{
    if (x < 0) return -1;
    if (x > 0) return 1;
    return 0;
}

void      XhGridSetChildAlignment(CxWidget w, int  h_a, int  v_a)
{
  int  a[9] =
        {
            NorthWestGravity, NorthGravity,  NorthEastGravity,
            WestGravity,      CenterGravity, EastGravity,
            SouthWestGravity, SouthGravity,  SouthEastGravity
        };
    
    XtVaSetValues(CNCRTZE(w),
                  XmNcellGravity, a[sign_of(h_a) + 1 + sign_of(v_a) * 3 + 3],
                  NULL);
}

void      XhGridSetChildFilling  (CxWidget w, Bool h_f, Bool v_f)
{
    XtVaSetValues(CNCRTZE(w),
                  XmNfillType, (h_f != 0) * 1 + (v_f != 0) * 2,
                  NULL);
}

