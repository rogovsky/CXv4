#ifndef __CX_COMMON_TYPES_H
#define __CX_COMMON_TYPES_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "misc_types.h"


typedef unsigned long         CxPixel;
typedef struct _CxWidgetType *CxWidget;

typedef int                   CxDataRef_t;
enum {NULL_CxDataRef = 0};

typedef struct
{
    char   *ident;
    char   *label;
    int     readonly;
    int     modified;
    double  value;
    double  minalwd;
    double  maxalwd;
    double  rsrvd_d;
} CxKnobParam_t;


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CX_COMMON_TYPES_H */
