#ifndef __XH_H
#define __XH_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <time.h>
#include <stdarg.h>

#include "cx_common_types.h"


/*** Types **********************************************************/

typedef struct _WindowInfo   *XhWindow;

typedef int (*XhCommandProc)(XhWindow window, const char *cmd, int info_int);

enum {
    XhACT_END = 0,
    XhACT_COMMAND,
    XhACT_CHECKBOX,
    XhACT_SUBMENU,
    XhACT_SEPARATOR,
    XhACT_FILLER,
    XhACT_LEDS,
    XhACT_LABEL,
    XhACT_NOP,
    XhACT_BANNER,
    XhACT_LOGGIA,
};

typedef struct
{
    int          type;
    const char  *cmd;
    const char  *label;
    const char  *tip;
    char       **pixmap;
    char       **mini_pixmap;
} xh_actdescr_t;

#define XhXXX_TOOLCMD(cmd, tip, pixmap, mini_pixmap) \
                              {XhACT_COMMAND,   cmd,  NULL, tip,  pixmap, mini_pixmap}
#define XhXXX_TOOLCHK(cmd, tip, pixmap, mini_pixmap) \
                              {XhACT_CHECKBOX,  cmd,  NULL, tip,  pixmap, mini_pixmap}
#define XhXXX_SEPARATOR()     {XhACT_SEPARATOR, NULL, NULL, NULL, NULL,   NULL}
#define XhXXX_TOOLFILLER()    {XhACT_FILLER,    NULL, NULL, NULL, NULL,   NULL}
#define XhXXX_TOOLLEDS()      {XhACT_LEDS,      NULL, NULL, NULL, NULL,   NULL}
#define XhXXX_TOOLLABEL(lab)  {XhACT_LABEL,     NULL, lab,  NULL, NULL,   NULL}
#define XhXXX_TOOLBANNER(nc)  {XhACT_BANNER,    (void*)(nc), \
                                                      NULL, NULL, NULL,   NULL}
#define XhXXX_TOOLLOGGIA()    {XhACT_LOGGIA,    NULL, NULL, NULL, NULL,   NULL}
#define XhXXX_END()           {XhACT_END,       NULL, NULL, NULL, NULL,   NULL}

/*** Application ****************************************************/

void  XhInitApplication(int         *argc,     char **argv,
                        const char  *app_name,
                        const char  *app_class,
                        char       **fallbacks);

void  XhRunApplication(void);


/*** Utilities ******************************************************/

CxWidget XhTopmost(CxWidget w);

int  XhAssignPixmap        (CxWidget w, char **pix);
int  XhAssignPixmapFromFile(CxWidget w, char *filename);
void XhInvertButton        (CxWidget w);

void XhAssignVertLabel(CxWidget w, const char *text, int alignment);

void XhSwitchContentFoldedness(CxWidget parent);

void XhENTER_FAILABLE(void);
int  XhLEAVE_FAILABLE(void);

void XhProcessPendingXEvents(void);
int  XhCompressConfigureEvents  (CxWidget w);
void XhRemoveExposeEvents       (CxWidget w);
void XhAdjustPreferredSizeInForm(CxWidget w);


/*** Colors management **********************************************/

enum
{
    XH_COLOR_FG_DEFAULT,
    XH_COLOR_BG_DEFAULT,
    XH_COLOR_TS_DEFAULT,
    XH_COLOR_BS_DEFAULT,
    XH_COLOR_BG_ARMED,
    XH_COLOR_BG_TOOL_ARMED,
    XH_COLOR_HIGHLIGHT,
    XH_COLOR_BORDER,

    XH_COLOR_FG_BALLOON,
    XH_COLOR_BG_BALLOON,
    
    XH_COLOR_BG_FDLG_INPUTS,
    
    XH_COLOR_BG_DATA_INPUT,

    XH_COLOR_THERMOMETER,

    XH_COLOR_INGROUP,

    XH_COLOR_CTL3D_BG,
    XH_COLOR_CTL3D_TS,
    XH_COLOR_CTL3D_BS,
    XH_COLOR_CTL3D_BG_ARMED,
    
    XH_COLOR_GRAPH_BG,
    XH_COLOR_GRAPH_AXIS,
    XH_COLOR_GRAPH_GRID,
    XH_COLOR_GRAPH_TITLES,
    XH_COLOR_GRAPH_REPERS,
    XH_COLOR_GRAPH_LINE1,
    XH_COLOR_GRAPH_LINE2,
    XH_COLOR_GRAPH_LINE3,
    XH_COLOR_GRAPH_LINE4,
    XH_COLOR_GRAPH_LINE5,
    XH_COLOR_GRAPH_LINE6,
    XH_COLOR_GRAPH_LINE7,
    XH_COLOR_GRAPH_LINE8,
    XH_COLOR_GRAPH_BAR1,
    XH_COLOR_GRAPH_BAR2,
    XH_COLOR_GRAPH_BAR3,
    XH_COLOR_GRAPH_PREV,
    XH_COLOR_GRAPH_OUTL,

    XH_COLOR_BGRAPH_BG,
    XH_COLOR_BGRAPH_AXIS,
    XH_COLOR_BGRAPH_GRID,
    XH_COLOR_BGRAPH_TITLES,
    XH_COLOR_BGRAPH_REPERS,
    XH_COLOR_BGRAPH_LINE1,
    XH_COLOR_BGRAPH_LINE2,
    XH_COLOR_BGRAPH_LINE3,
    XH_COLOR_BGRAPH_LINE4,
    XH_COLOR_BGRAPH_LINE5,
    XH_COLOR_BGRAPH_LINE6,
    XH_COLOR_BGRAPH_LINE7,
    XH_COLOR_BGRAPH_LINE8,
    XH_COLOR_BGRAPH_BAR1,
    XH_COLOR_BGRAPH_BAR2,
    XH_COLOR_BGRAPH_BAR3,
    XH_COLOR_BGRAPH_PREV,
    XH_COLOR_BGRAPH_OUTL,

    XH_COLOR_FG_HILITED,
    XH_COLOR_FG_IMPORTANT,
    XH_COLOR_FG_DIM,
    XH_COLOR_FG_WEIRD,
    XH_COLOR_BG_VIC,
    XH_COLOR_FG_HEADING,
    XH_COLOR_BG_HEADING,
    XH_COLOR_BG_NORM_YELLOW,
    XH_COLOR_BG_NORM_RED,
    XH_COLOR_BG_IMP_YELLOW,
    XH_COLOR_BG_IMP_RED,
    XH_COLOR_BG_WEIRD,
    XH_COLOR_BG_JUSTCREATED,
    XH_COLOR_BG_DEFUNCT,
    XH_COLOR_BG_HWERR,
    XH_COLOR_BG_SFERR,
    XH_COLOR_BG_OTHEROP,
    XH_COLOR_BG_PRGLYCHG,
    XH_COLOR_BG_NOTFOUND,
    
    XH_COLOR_JUST_RED,
    XH_COLOR_JUST_ORANGE,
    XH_COLOR_JUST_YELLOW,
    XH_COLOR_JUST_GREEN,
    XH_COLOR_JUST_BLUE,
    XH_COLOR_JUST_INDIGO,
    XH_COLOR_JUST_VIOLET,
    
    XH_COLOR_JUST_BLACK,
    XH_COLOR_JUST_WHITE,
    XH_COLOR_JUST_GRAY,

    XH_COLOR_JUST_AMBER,
    XH_COLOR_JUST_BROWN,
};

enum
{
    XH_NUM_DISTINCT_LINE_COLORS = XH_COLOR_GRAPH_LINE8 - XH_COLOR_GRAPH_LINE1 + 1,
    XH_NUM_DISTINCT_BAR_COLORS  = XH_COLOR_GRAPH_BAR3  - XH_COLOR_GRAPH_BAR1  + 1,
};

int     XhSetColorBinding(const char *name, const char *defn);
CxPixel XhGetColor       (int n);
CxPixel XhGetColorByName (const char *name);
const char *XhStrColindex(int n);
int     XhColorCount     (void);


/*** Balloon ********************************************************/

void XhSetBalloon(CxWidget w, const char *text);


/*** Hilite rectangle ***********************************************/

void XhCreateHilite(XhWindow window);

void XhShowHilite(CxWidget w);
void XhHideHilite(CxWidget w);


/*** Grid ***********************************************************/

CxWidget  XhCreateGrid(CxWidget parent, const char *name);
void      XhGridSetPadding  (CxWidget w, int hpad, int vpad);
void      XhGridSetSpacing  (CxWidget w, int hspc, int vspc);
void      XhGridSetSize     (CxWidget w, int cols, int rows);
void      XhGridSetGrid     (CxWidget w, int horz, int vert);
void      XhGridSetChildPosition (CxWidget w, int  x,   int  y);
void      XhGridGetChildPosition (CxWidget w, int *x_p, int *y_p);
void      XhGridSetChildAlignment(CxWidget w, int  h_a, int  v_a);
void      XhGridSetChildFilling  (CxWidget w, int  h_f, int  v_f);


/*** Windows ********************************************************/

/* Main XhWindow-management API */

enum
{
    XhWindowMenuMask          = 1 << 0,
    XhWindowToolbarMask       = 1 << 1,
    XhWindowStatuslineMask    = 1 << 2,
    XhWindowCompactMask       = 1 << 3,

    XhWindowWorkspaceTypeMask = 7 << 4,
    XhWindowFormMask          = 0 << 4,
    XhWindowRowColumnMask     = 1 << 4,
    XhWindowGridMask          = 2 << 4,
    
    XhWindowUnresizableMask   = 1 << 8,
    XhWindowOnTopMask         = 1 << 9,   /*!!! <- Currently not implemented  */
    XhWindowSecondaryMask     = 1 << 10,
    XhWindowToolbarMiniMask   = 1 << 11,
};

XhWindow  XhCreateWindow(const char    *title,
                         const char    *app_name, const char *app_class,
                         int            contents,
                         xh_actdescr_t *menus,
                         xh_actdescr_t *tools);
void        XhDeleteWindow(XhWindow window);
void        XhShowWindow  (XhWindow window);
void        XhHideWindow  (XhWindow window);

void        XhSetWindowIcon(XhWindow window, char **pix);
void        XhSetWindowCmdProc(XhWindow window, XhCommandProc proc);
void        XhSetWindowCloseBh(XhWindow window, const char *cmd);
void        XhGetWindowSize   (XhWindow window, int *wid, int *hei);
void        XhSetWindowLimits (XhWindow window,
                               int  min_w, int min_h,
                               int  max_w, int max_h);

XhWindow XhWindowOf(CxWidget w);
CxWidget    XhGetWindowShell    (XhWindow window);
CxWidget    XhGetWindowWorkspace(XhWindow window);
CxWidget    XhGetWindowAlarmleds(XhWindow window);

void       *XhGetHigherPrivate  (XhWindow window);
void        XhSetHigherPrivate  (XhWindow window, void *privptr);

void XhSetCommandOnOff  (XhWindow window, const char *cmd, int is_pushed);
void XhSetCommandEnabled(XhWindow window, const char *cmd, int is_enabled);

void XhSetToolbarBanner (XhWindow window, const char *text);

CxWidget    XhGetToolbarLoggia (XhWindow window);

/* Status line */

void XhMakeMessage     (XhWindow window, const char *format, ...)
                       __attribute__((format (printf, 2, 3)));
void XhVMakeMessage    (XhWindow window, const char *format, va_list ap);
void XhMakeTempMessage (XhWindow window, const char *format, ...)
                       __attribute__((format (printf, 2, 3)));
void XhVMakeTempMessage(XhWindow window, const char *format, va_list ap);


/*** Various dialogs ************************************************/

/* "Standard" (messagebox-based) dialogs */

enum
{
    XhStdDlgFOk          = 1 << 0,
    //XhStdDlgFApply       = 1 << 1,
    XhStdDlgFCancel      = 1 << 2,
    XhStdDlgFHelp        = 1 << 3,
    XhStdDlgFNothing     = 1 << 4,
    XhStdDlgFCenterMsg   = 1 << 5,
    XhStdDlgFNoMargins   = 1 << 6,

    XhStdDlgFModal       = 1 << 8,
    XhStdDlgFNoAutoUnmng = 1 << 9,
    XhStdDlgFNoDefButton = 1 << 10,
    
    XhStdDlgFResizable   = 1 << 16,
    XhStdDlgFZoomable    = 1 << 17,
};

CxWidget XhCreateStdDlg    (XhWindow window, const char *name,
                            const char *title, const char *ok_text,
                            const char *msg, int flags);
void     XhStdDlgSetMessage(CxWidget dlg, const char *msg);
void     XhStdDlgShow      (CxWidget dlg);
void     XhStdDlgHide      (CxWidget dlg);


/* File dialogs */

CxWidget XhCreateFdlg(XhWindow window, const char *name,
                      const char *title, const char *ok_text,
                      const char *pattern, const char *cmd);
void     XhFdlgShow       (CxWidget  fdlg);
void     XhFdlgHide       (CxWidget  fdlg);
void     XhFdlgGetFilename(CxWidget  fdlg, char *buf, size_t bufsize);

/* Simplified-file-interface dialogs */

/* a. Save */
CxWidget XhCreateSaveDlg(XhWindow window, const char *name,
                              const char *title, const char *ok_text,
                              const char *cmd);
void     XhSaveDlgShow       (CxWidget dlg);
void     XhSaveDlgHide       (CxWidget dlg);
void     XhSaveDlgGetComment (CxWidget dlg, char *buf, size_t bufsize);
void     XhFindFilename      (const char *pattern,
                              char *buf, size_t bufsize);

/* b. Load */
typedef int (*xh_loaddlg_filter_proc)(const char *fname,
                                      time_t *cr_time,
                                      char *labelbuf, size_t labelbufsize,
                                      void *privptr);
CxWidget XhCreateLoadDlg     (XhWindow window, const char *name,
                              const char *title, const char *ok_text,
                              xh_loaddlg_filter_proc filter, void *privptr,
                              const char *pattern,
                              const char *cmd, const char *flst_cmd);
void     XhLoadDlgShow       (CxWidget dlg);
void     XhLoadDlgHide       (CxWidget dlg);
void     XhLoadDlgGetFilename(CxWidget dlg, char *buf, size_t bufsize);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __XH_H */
