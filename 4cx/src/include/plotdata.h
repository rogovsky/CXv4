#ifndef __PLOTDATA_H
#define __PLOTDATA_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "cx.h"


typedef union
{
    int     int_r[2];
    double  dbl_r[2];
} plotdata_range_t;

typedef struct
{
    void             *privptr;

    int               on;             // Is data present
    int               numpts;

    plotdata_range_t  all_range;
    plotdata_range_t  cur_range;
    int               cur_int_zero;

    void             *x_buf;
    cxdtype_t         x_buf_dtype;
} plotdata_t;


static inline int    plotdata_get_int(plotdata_t *p, int x)
{
  size_t  dsize = sizeof_cxdtype(p->x_buf_dtype);

    if (p->x_buf_dtype & CXDTYPE_USGN_MASK)
    {
        if      (dsize == sizeof(uint32)) return ((uint32 *)(p->x_buf))[x];
        else if (dsize == sizeof(uint16)) return ((uint16 *)(p->x_buf))[x];
        else                              return ((uint8  *)(p->x_buf))[x];
    }
    else
    {
        if      (dsize == sizeof(int32))  return ((int32  *)(p->x_buf))[x];
        else if (dsize == sizeof(int16))  return ((int16  *)(p->x_buf))[x];
        else                              return ((int8   *)(p->x_buf))[x];
    }
}

static inline double plotdata_get_dbl(plotdata_t *p, int x)
{
    if (p->x_buf_dtype == CXDTYPE_SINGLE) return ((float32 *)(p->x_buf))[x];
    else                                  return ((float64 *)(p->x_buf))[x];
}

static inline void   plotdata_put_int(plotdata_t *p, int x, int v)
{
  size_t  dsize = sizeof_cxdtype(p->x_buf_dtype);

    if      (dsize == sizeof(uint32)) ((int32 *)(p->x_buf))[x] = v;
    else if (dsize == sizeof(uint16)) ((int16 *)(p->x_buf))[x] = v;
    else                              ((int8  *)(p->x_buf))[x] = v;
}

static inline void   plotdata_put_dbl(plotdata_t *p, int x, double v)
{
    if (p->x_buf_dtype == CXDTYPE_SINGLE) ((float32 *)(p->x_buf))[x] = v;
    else                                  ((float64 *)(p->x_buf))[x] = v;
}


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __PLOTDATA_H */
