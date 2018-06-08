#ifndef __XH_MONOIMG_H
#define __XH_MONOIMG_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <X11/Intrinsic.h>


enum
{
    XH_MONOIMG_DPYMODE_DIRECT  = 0,
    XH_MONOIMG_DPYMODE_INVERSE = 1,
    XH_MONOIMG_DPYMODE_RAINBOW = 2,
};


typedef struct _XhMonoimg_info_t_struct *XhMonoimg;


XhMonoimg  XhCreateMonoimg(Widget parent,
                           int data_w, int data_h,
                           int show_w, int show_h,
                           int sofs_x, int sofs_y,
                           int nb, int bpp, int srcmaxval,
                           void *privptr);
void      *XhMonoimgGetBuf(XhMonoimg mi);

void       XhMonoimgUpdate(XhMonoimg mi);
                           
void       XhMonoimgSetUndefect (XhMonoimg mi, int mode, int update);
void       XhMonoimgSetNormalize(XhMonoimg mi, int mode, int update);
void       XhMonoimgSetViolet0  (XhMonoimg mi, int mode, int update);
void       XhMonoimgSetRed_max  (XhMonoimg mi, int mode, int update);
void       XhMonoimgSetDpyMode  (XhMonoimg mi, int mode, int update);

Widget     XhWidgetOfMonoimg (XhMonoimg mi);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __XH_MONOIMG_H */
