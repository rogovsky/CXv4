#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <math.h>

#include <X11/Intrinsic.h>
#include <Xm/Xm.h>
#include <Xm/DrawingA.h>

#include "misc_macros.h"

#include "Xh.h"
#include "Xh_globals.h"
#include "Xh_utils.h"

#include "Xh_monoimg.h"


//////////////////////////////////////////////////////////////////////

static GC AllocInvrGC(Widget w, const char *spec)
{
  XGCValues  vals;

  Colormap   cmap;
  XColor     xcol;

    XtVaGetValues(w, XmNcolormap, &cmap, NULL);
    if (!XAllocNamedColor(XtDisplay(w), cmap, spec, &xcol, &xcol))
    {
        xcol.pixel = WhitePixelOfScreen(XtScreen(w));
    }
    vals.foreground = xcol.pixel;
  
    vals.function = GXxor;
  
    return XtGetGC(w, GCForeground | GCFunction, &vals);
}
//////////////////////////////////////////////////////////////////////

enum
{
    PLUS_INVVAL = 1,
    PLUS_MDYVAL = 2,

    PLUS_COUNT  = 2,
};


typedef uint32 img_datum_t;

typedef void   (*img_manip_t)(struct _XhMonoimg_info_t_struct *mi);
typedef uint32 (*img_getpv_t)(struct _XhMonoimg_info_t_struct *mi, int x, int y);


typedef struct _XhMonoimg_info_t_struct
{
    void        *privptr;

    XhWindow     top_win;
    Widget       area;
    XImage      *img;
    int          red_max,   red_shift;
    int          green_max, green_shift;
    int          blue_max,  blue_shift;
    GC           copyGC;
    GC           invrGC[3];


    int          data_w, data_h;
    int          show_w, show_h;
    int          sofs_x, sofs_y;
    int          nb;
    int          bpp;
    int          datacount;
    int          bitmask;     /*!!! uint32? */
    int          srcmaxval;
    int          maxval;
    int          invval;
    int          mdyval;

    img_manip_t  PerformCopyMskd;
    img_manip_t  PerformUndefect;
    img_manip_t  PerformNormalize;
    img_manip_t  BlitDspToImg;
    img_getpv_t  GetValueAtXY;

    int          v_undefect;
    int          v_normalize;
    int          v_violet0;
    int          v_red_max;
    int          v_dpymode;

    int          mouse_x;
    int          mouse_y;
    int          prev_mouse_x;
    int          prev_mouse_y;

    void        *nrm_ramp;
    void        *mes_data;
    void        *dsp_data;
    void        *img_data;

    int          ramp[0];   /*!!! uint32? */
} monoimg_info_t;

//////////////////////////////////////////////////////////////////////

/*
 *  Note: SplitComponentMask() supposed that the mask is contiguos, so
 *        this func ...
 */

static void SplitComponentMask(unsigned long component_mask, int *max_p, int *shift_p)
{
  int  shift_factor;
  int  num_bits;
    
    /* Sanity check */
    if (component_mask == 0)
    {
        *max_p = 1;
        *shift_p = 0;
        return;
    }

    /* I. Find out how much is it shifted */
    for (shift_factor = 0;  (component_mask & 1) == 0;  shift_factor++)
        component_mask >>= 1;

    /* II. And how many bits are there */
    for (num_bits = 1;  (component_mask & 2) != 0;  num_bits++)
        component_mask >>= 1;

    /* Return values */
    *max_p   = (1 << num_bits) - 1;
    *shift_p = shift_factor;
}

static unsigned long GetComponentBits(int v, int v_max,
                                      int component_max, int shift_factor)
{
    return ((v * component_max) / v_max) << shift_factor;
}

static unsigned long GetRgbPixelValue(monoimg_info_t *mi,
                                      int r, int r_max,
                                      int g, int g_max,
                                      int b, int b_max)

{
    return GetComponentBits(r, r_max, mi->red_max,   mi->red_shift)   |
           GetComponentBits(g, g_max, mi->green_max, mi->green_shift) |
           GetComponentBits(b, b_max, mi->blue_max,  mi->blue_shift);
}

static unsigned long GetGryPixelValue(monoimg_info_t *mi,
                                      int gray, int gray_max)
{
    return GetRgbPixelValue(mi,
                            gray, gray_max,
                            gray, gray_max,
                            gray, gray_max);
}

//--------------------------------------------------------------------

/*
   Based on http://en.wikipedia.org/wiki/HSV_color_space#From_HSV_to_RGB
   H is [0,360]; S and V are [0,1]; R, G, B are [0, maxRGB]
 */

static void MapHSVtoRGB(double  H, double  S, double  V,
                        int    *R, int    *G, int    *B, int    maxRGB)
{
  int     Hi = fmod(trunc(H / 60), 6);
  double  f  = H / 60 - Hi;
  double  p  = V*(1 - S);
  double  q  = V*(1 - f*S);
  double  t  = V*(1 - (1-f)*S);

    switch (Hi)
    {
        case 0: *R = V*maxRGB; *G = t*maxRGB; *B = p*maxRGB; break;
        case 1: *R = q*maxRGB; *G = V*maxRGB; *B = p*maxRGB; break;
        case 2: *R = p*maxRGB; *G = V*maxRGB; *B = t*maxRGB; break;
        case 3: *R = p*maxRGB; *G = q*maxRGB; *B = V*maxRGB; break;
        case 4: *R = t*maxRGB; *G = p*maxRGB; *B = V*maxRGB; break;
        case 5: *R = V*maxRGB; *G = p*maxRGB; *B = q*maxRGB; break;
    }
}

//--------------------------------------------------------------------

static void FillRamp(monoimg_info_t *mi)
{
  int  n;
  int  r, g, b;
    
    if      (mi->v_dpymode == XH_MONOIMG_DPYMODE_DIRECT)
    {
        for (n = 0;  n <= mi->maxval;  n++)
            mi->ramp[n] = GetGryPixelValue(mi, n, mi->maxval);
    }
    else if (mi->v_dpymode == XH_MONOIMG_DPYMODE_INVERSE)
    {
        for (n = 0;  n <= mi->maxval;  n++)
            mi->ramp[n] = GetGryPixelValue(mi, mi->maxval - n, mi->maxval);
    }
    else /*if (mi->v_dpymode == XH_MONOIMG_DPYMODE_RAINBOW)*/
    {
        mi->ramp[0] = GetGryPixelValue(mi, 0, mi->maxval);
        for (n = 1;  n <= mi->maxval;  n++)
        {
            MapHSVtoRGB(RESCALE_VALUE(n, 0, mi->maxval, 240.0, 0.0), 0.9, 0.9,
                        &r, &g, &b, 255);
            mi->ramp[n] = GetRgbPixelValue(mi, r, 255, g, 255, b, 255);
        }
    }

    mi->ramp[mi->invval] = GetRgbPixelValue(mi, 255, 255, 0,   255, 255, 255);
    mi->ramp[mi->mdyval] = GetRgbPixelValue(mi, 255, 255, 0,   255, 0,   255);
    
    if (mi->v_dpymode != XH_MONOIMG_DPYMODE_RAINBOW  &&  mi->v_red_max)
    {
        mi->ramp[mi->maxval] = mi->ramp[mi->mdyval];
        if (mi->v_normalize == 0  &&
            mi->srcmaxval > 0  &&  mi->srcmaxval < mi->maxval)
            mi->ramp[mi->srcmaxval] = mi->ramp[mi->mdyval];
    }

    if (mi->v_dpymode != XH_MONOIMG_DPYMODE_INVERSE  &&  mi->v_violet0)
        mi->ramp[0] = GetRgbPixelValue(mi, 160, 255, 0,   255, 160, 255);
}

//////////////////////////////////////////////////////////////////////

static inline int square(int x){return x*x;}

//- 1 ----------------------------------------------------------------

//- 2 ----------------------------------------------------------------

static void PerformCopyMskd2(monoimg_info_t *mi)
{
  register uint16 *src;
  register uint16 *dst;
  register uint16  msk = mi->bitmask;
  register int     ctr;
  
    for (ctr = mi->datacount,
         src = mi->mes_data,
         dst = mi->dsp_data;
         ctr > 0;
         ctr--)
        *dst++ = (*src++) & msk;
}

static void PerformUndefect2(monoimg_info_t *mi)
{
#if 1
    PerformCopyMskd2(mi);
#else
  register uint16 *src;
  register uint16 *dst;
  register int     ctr;
  register int     B;
  int              sigma2;
  
    for (ctr = PIX_W + 1,
         src = mi->mes_data,
         dst = mi->dsp_data;
         ctr > 0;
         ctr--)
        *dst++ = (*src++) & mi->bitmask;
    for (ctr = PIX_W + 1,
         src = mi->mes_data + PIX_W*PIX_H - (PIX_W+1),
         dst = mi->dsp_data + PIX_W*PIX_H - (PIX_W+1);
         ctr > 0;
         ctr--)
        *dst++ = (*src++) & mi->bitmask;
  
    for (ctr = PIX_W*(PIX_H-2) - 2,
         src = mi->mes_data + PIX_W + 1,
         dst = mi->dsp_data + PIX_W + 1;
         ctr > 0;
         ctr--, src++, dst++)
    {
        B = (
             (
                        src[-PIX_W] +
              src[-1] +               src[+1] +
                        src[+PIX_W]
             ) & 4095
            ) / 4;
        sigma2 = (
                                      square(src[-PIX_W]-B) +
                  square(src[-1]-B) +                         square(src[+1]-B) +
                                      square(src[+PIX_W]-B)
                 ) / 8;

        *dst = (square(*src - B) > 9 * sigma2)? B : *src & mi->bitmask;
    }
#endif
}

static void PerformNormalize2(monoimg_info_t *mi)
{
  register uint16 *p;
  register uint16  v;
  register uint16  curmin, curmax;
  register int     ctr;
  uint16 *max_p __attribute__((unused));

  uint16           minval;
  uint16           maxval;

  uint16          *table = mi->nrm_ramp;

    for (p      = mi->dsp_data,
         ctr    = mi->datacount,
         curmin = mi->maxval,
         curmax = 0;

         ctr != 0;
         ctr--)
    {
        v = *p++;
        if (v > curmax) curmax = v, max_p = p-1;
        if (v < curmin) curmin = v;
    }
    minval = curmin;
    maxval = curmax;

    ////fprintf(stderr, "[%d,%d], max=@%d,%d\n", minval, maxval, (max_p-(uint16*)(mi->dsp_data))%mi->data_w, (max_p-(uint16*)(mi->dsp_data))/mi->data_w);

    // Prevent division by zero
    if (minval == maxval)
    {
        if (maxval == 0) maxval = mi->maxval;
        else             minval = 0;
    }

    for (ctr = 0;  ctr <= mi->maxval;  ctr++)
        table[ctr] = mi->invval;

    for (ctr = minval;  ctr <= maxval;  ctr++)
        table[ctr] = RESCALE_VALUE(ctr, minval, maxval, 0, mi->maxval);

    for (p = mi->dsp_data,  ctr = mi->datacount;
         ctr != 0;
         p++,               ctr--)
        *p = table[*p];
}

static void BlitDspToImg2(monoimg_info_t *mi)
{
  register uint16 *src;
  register uint32 *dst;
  register uint16  msk  = mi->bitmask;
  register int     ctr;
  register uint32 *ramp = mi->ramp;
  
    for (ctr = mi->datacount,  src = mi->dsp_data,  dst = mi->img_data;
         ctr != 0;
         ctr--,                src++,               dst++)
        *dst = ramp[*src & msk];
}

static uint32 GetValueAtXY2(monoimg_info_t *mi, int x, int y)
{
    if (x < 0  ||  x >= mi->show_w  ||
        y < 0  ||  y >= mi->show_h)
        return 0;

    return
        ((uint16 *)(mi->dsp_data))[(y + mi->sofs_y) * mi->data_w +
                                   (x + mi->sofs_x)];
}

//- 4 ----------------------------------------------------------------

static void PerformCopyMskd4(monoimg_info_t *mi)
{
  register uint32 *src;
  register uint32 *dst;
  register uint32  msk = mi->bitmask;
  register int     ctr;
  
    for (ctr = mi->datacount,
         src = mi->mes_data,
         dst = mi->dsp_data;
         ctr > 0;
         ctr--)
        *dst++ = (*src++) & msk;
}

static void PerformUndefect4(monoimg_info_t *mi)
{
#if 1
    PerformCopyMskd2(mi);
#else
#endif
}

static void PerformNormalize4(monoimg_info_t *mi)
{
  register uint32 *p;
  register uint32  v;
  register uint32  curmin, curmax;
  register int     ctr;
  uint32 *max_p __attribute__((unused));

  uint32           minval;
  uint32           maxval;

  uint32          *table = mi->nrm_ramp;

    for (p      = mi->dsp_data,
         ctr    = mi->datacount,
         curmin = mi->maxval,
         curmax = 0;

         ctr != 0;
         ctr--)
    {
        v = *p++;
        if (v > curmax) curmax = v, max_p = p-1;
        if (v < curmin) curmin = v;
    }
    minval = curmin;
    maxval = curmax;

    ////fprintf(stderr, "[%d,%d], max=@%d,%d\n", minval, maxval, (max_p-(uint32*)(mi->dsp_data))%mi->data_w, (max_p-(uint32*)(mi->dsp_data))/mi->data_w);

    // Prevent division by zero
    if (minval == maxval)
    {
        if (maxval == 0) maxval = mi->maxval;
        else             minval = 0;
    }

    for (ctr = 0;  ctr <= mi->maxval;  ctr++)
        table[ctr] = mi->invval;

    for (ctr = minval;  ctr <= maxval;  ctr++)
        table[ctr] = RESCALE_VALUE(/*!!! (int64)ctr? */ctr, minval, maxval, 0, mi->maxval);

    for (p = mi->dsp_data,  ctr = mi->datacount;
         ctr != 0;
         p++,               ctr--)
        *p = table[*p];
}

static void BlitDspToImg4(monoimg_info_t *mi)
{
  register uint32 *src;
  register uint32 *dst;
  register uint32  msk  = mi->bitmask;
  register int     ctr;
  register uint32 *ramp = mi->ramp;
  
    for (ctr = mi->datacount,  src = mi->dsp_data,  dst = mi->img_data;
         ctr != 0;
         ctr--,                src++,               dst++)
        *dst = ramp[*src & msk];
}

static uint32 GetValueAtXY4(monoimg_info_t *mi, int x, int y)
{
    if (x < 0  ||  x >= mi->show_w  ||
        y < 0  ||  y >= mi->show_h)
        return 0;

    return
        ((uint32 *)(mi->dsp_data))[(y + mi->sofs_y) * mi->data_w +
                                   (x + mi->sofs_x)];
}

//--------------------------------------------------------------------

static void ShowMouseStats(monoimg_info_t *mi, int x, int y)
{
  char buf[1000];

    if (x >= mi->show_w) x = -1;
    if (y >= mi->show_h) y = -1;

    if ((x < 0  &&  mi->prev_mouse_x >= 0)  ||
        (x < 0  &&  mi->prev_mouse_y >= 0))
        XhMakeTempMessage(mi->top_win, NULL);

    if (x >= 0  &&  y >= 0)
    {
        sprintf(buf, "(%d,%d): %u", x, y, mi->GetValueAtXY(mi, x, y));
        XhMakeTempMessage(mi->top_win, buf);
    }

    mi->prev_mouse_x = x;
    mi->prev_mouse_y = y;
}

//--------------------------------------------------------------------

static void CopyMesToDsp(monoimg_info_t *mi)
{
    if (mi->v_undefect)  mi->PerformUndefect (mi);
    else                 mi->PerformCopyMskd (mi);

    if (mi->v_normalize) mi->PerformNormalize(mi);
}

static void ShowImage(monoimg_info_t *mi,
                      int x, int y, int width, int height)
{
    XPutImage(XtDisplay(mi->area), XtWindow(mi->area),
              mi->copyGC, mi->img,
              x + mi->sofs_x, y + mi->sofs_y, x, y, width, height);
}

static void ShowFrame(monoimg_info_t *mi)
{
    CopyMesToDsp    (mi);
    mi->BlitDspToImg(mi);
    ShowImage       (mi, 0, 0, mi->show_w, mi->show_h);
}


static void ExposureCB(Widget     w         __attribute__((unused)),
                       XtPointer  closure,
                       XtPointer  call_data)
{
  monoimg_info_t *mi = (monoimg_info_t *) closure;
  XExposeEvent   *ev = (XExposeEvent *)   (((XmDrawingAreaCallbackStruct *)call_data)->event);

    ShowImage(mi, ev->x, ev->y, ev->width, ev->height);
}

static void PointerHandler(Widget     w,
                           XtPointer  closure,
                           XEvent    *event,
                           Boolean   *continue_to_dispatch)
{
  monoimg_info_t    *mi  = closure;

  XMotionEvent      *mev = (XMotionEvent      *) event;
  XButtonEvent      *bev = (XButtonEvent      *) event;
  XCrossingEvent    *cev = (XEnterWindowEvent *) event;

  int                ev_type;
  int                state;
  int                mod_mask;
  int                bn;
  int                x;
  int                y;

    /* Get parameters of this event */
    bn = -1;
    if      (event->type == MotionNotify)
    {
        ev_type = MotionNotify;
        state   = mev->state;
        x       = mev->x;
        y       = mev->y;
    }
    else if (event->type == ButtonPress  ||  event->type == ButtonRelease)
    {
        ev_type = event->type == ButtonPress? ButtonPress : ButtonRelease;
        state   = bev->state;
        x       = bev->x;
        y       = bev->y;
        bn      = bev->button - Button1;
    }
    else if (event->type == EnterNotify  ||  event->type == LeaveNotify)
    {
        ev_type = event->type == EnterNotify? EnterNotify : LeaveNotify;
        state   = cev->state;
        x       = cev->x;
        y       = cev->y;
    }
    else
    {
        return;
    }

    /* Do processing */

    /* Take care of status line display */
    if(ev_type == LeaveNotify)
    {
        mi->mouse_x = mi->mouse_y = -1;
    }
    else
    {
        mi->mouse_x = x;
        mi->mouse_y = y;
    }
    ShowMouseStats(mi, mi->mouse_x, mi->mouse_y);
}

//////////////////////////////////////////////////////////////////////

static inline size_t size16(size_t size) // Aligns to 16-byte boundary
{
    return (size + 15) &~ ((size_t)15);
}

XhMonoimg  XhCreateMonoimg(Widget parent,
                           int data_w, int data_h,
                           int show_w, int show_h,
                           int sofs_x, int sofs_y,
                           int nb, int bpp, int srcmaxval,
                           void *privptr)
{
  monoimg_info_t *mi;
  size_t          recsize;
  size_t          bufsize = size16(data_w * data_h * nb);
  size_t          ofs;
  size_t          o_nrm;
  size_t          o_mes;
  size_t          o_dsp;
  size_t          o_img;

  int             maxval  = (1 << bpp) - 1;

  Widget          shell;
  Visual         *visual;
  int             depth;
  XGCValues       vals;


    /* Perform checks */
    if (
        (nb  < 1  ||  nb  > 4  ||  nb == 3)       ||
        (bpp < 1  ||  bpp > 24)                   ||
        (bpp > nb * 8)                            ||
        (sofs_x < 0              ||  sofs_y < 0)  ||
        (show_w < 1              ||  show_h < 1)  ||
        (data_w < show_w+sofs_x  ||  data_h < show_h+sofs_y)
       )
    {
        errno = EINVAL;
        return NULL;
    }


    /* Ramp */   ofs  = size16(sizeof(mi->ramp[0]) * (maxval + PLUS_COUNT + 1));
    o_nrm = ofs; ofs += size16((maxval + 1) * nb);
    o_mes = ofs; ofs += bufsize;
    o_dsp = ofs; ofs += bufsize;
    o_img = ofs; ofs += size16(sizeof(img_datum_t) * data_w * data_h);

    recsize = sizeof(*mi) + ofs;

    mi = malloc(recsize);
    if (mi == NULL) return NULL;
    bzero(mi, recsize);

    mi->data_w    = data_w;
    mi->data_h    = data_h;
    mi->show_w    = show_w;
    mi->show_h    = show_h;
    mi->sofs_x    = sofs_x;
    mi->sofs_y    = sofs_y;
    mi->nb        = nb;
    mi->bpp       = bpp;

    mi->datacount = data_w * data_h;
    mi->bitmask   = maxval;
    mi->srcmaxval = srcmaxval;
    mi->maxval    = maxval;
    mi->invval    = maxval + PLUS_INVVAL;
    mi->mdyval    = maxval + PLUS_MDYVAL;

    if      (nb == 1)
    {
    }
    else if (nb == 2)
    {
        mi->PerformCopyMskd  = PerformCopyMskd2;
        mi->PerformUndefect  = PerformUndefect2;
        mi->PerformNormalize = PerformNormalize2;
        mi->BlitDspToImg     = BlitDspToImg2;
        mi->GetValueAtXY     = GetValueAtXY2;
    }
    else if (nb == 4)
    {
        mi->PerformCopyMskd  = PerformCopyMskd4;
        mi->PerformUndefect  = PerformUndefect4;
        mi->PerformNormalize = PerformNormalize4;
        mi->BlitDspToImg     = BlitDspToImg4;
        mi->GetValueAtXY     = GetValueAtXY4;
    }

    mi->mouse_x = mi->prev_mouse_x = -1;
    mi->mouse_y = mi->prev_mouse_y = -1;

    mi->nrm_ramp = (uint8 *)(mi->ramp) + o_nrm;
    mi->mes_data = (uint8 *)(mi->ramp) + o_mes;
    mi->dsp_data = (uint8 *)(mi->ramp) + o_dsp;
    mi->img_data = (uint8 *)(mi->ramp) + o_img;

    mi->top_win  = XhWindowOf(parent);

    mi->area = XtVaCreateManagedWidget("image",
                                        xmDrawingAreaWidgetClass,
                                        parent,
                                        XmNwidth,       mi->show_w,
                                        XmNheight,      mi->show_h,
                                        XmNtraversalOn, False,
                                        NULL);
    XtAddCallback    (mi->area, XmNexposeCallback, ExposureCB, (XtPointer)mi);
    XtAddEventHandler(mi->area,
                      EnterWindowMask | LeaveWindowMask | PointerMotionMask |
                      ButtonPressMask | ButtonReleaseMask |
                      Button1MotionMask | Button2MotionMask | Button3MotionMask,
                      False, PointerHandler, mi);

    shell  = CNCRTZE(XhGetWindowShell(XhWindowOf(ABSTRZE(mi->area))));
    XtVaGetValues(shell,
                  XmNvisual, &visual,
                  XmNdepth,  &depth,
                  NULL);
    if (visual == NULL)  visual = DefaultVisualOfScreen(XtScreen(shell));

    mi->img = XCreateImage(XtDisplay(mi->area), visual, depth, ZPixmap,
                           0, mi->img_data, mi->data_w, mi->data_h,
                           sizeof(img_datum_t) * 8, 0);
    SplitComponentMask(mi->img->red_mask,   &(mi->red_max),   &(mi->red_shift));
    SplitComponentMask(mi->img->green_mask, &(mi->green_max), &(mi->green_shift));
    SplitComponentMask(mi->img->blue_mask,  &(mi->blue_max),  &(mi->blue_shift));
    vals.foreground = XhGetColor(XH_COLOR_JUST_RED);
    mi->copyGC = XtGetGC(mi->area, GCForeground, &vals);
    mi->invrGC[0] = AllocInvrGC(mi->area, "green");
    mi->invrGC[1] = AllocInvrGC(mi->area, "blue");
    mi->invrGC[2] = AllocInvrGC(mi->area, "red");
    FillRamp(mi);

    return mi;
}

void      *XhMonoimgGetBuf(XhMonoimg mi)
{
    return mi->mes_data;
}

void       XhMonoimgUpdate(XhMonoimg mi)
{
    ShowFrame(mi);
    ShowMouseStats(mi, mi->mouse_x, mi->mouse_y);
}

void       XhMonoimgSetUndefect (XhMonoimg mi, int mode, int update)
{
    mi->v_undefect  = (mode != 0);
    FillRamp(mi);
    if (update) XhMonoimgUpdate(mi);
}

void       XhMonoimgSetNormalize(XhMonoimg mi, int mode, int update)
{
    mi->v_normalize = (mode != 0);
    FillRamp(mi);
    if (update) XhMonoimgUpdate(mi);
}

void       XhMonoimgSetViolet0  (XhMonoimg mi, int mode, int update)
{
    mi->v_violet0   = (mode != 0);
    FillRamp(mi);
    if (update) XhMonoimgUpdate(mi);
}

void       XhMonoimgSetRed_max  (XhMonoimg mi, int mode, int update)
{
    mi->v_red_max   = (mode != 0);
    FillRamp(mi);
    if (update) XhMonoimgUpdate(mi);
}

void       XhMonoimgSetDpyMode  (XhMonoimg mi, int mode, int update)
{
    mi->v_dpymode   = mode;
    FillRamp(mi);
    if (update) XhMonoimgUpdate(mi);
}

Widget     XhWidgetOfMonoimg (XhMonoimg mi)
{
    return mi->area;
}
