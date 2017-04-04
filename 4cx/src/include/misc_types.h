#ifndef __MISC_TYPES_H
#define __MISC_TYPES_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


/*====================================================================
|                                                                    |
|  Bit types                                                         |
|                                                                    |
====================================================================*/

typedef   signed int    int32;
typedef unsigned int    uint32;
typedef   signed short  int16;
typedef unsigned short  uint16;
typedef   signed char   int8;
typedef unsigned char   uint8;

typedef unsigned char   char8;
typedef        uint32   char32;

typedef   signed long long int int64;
typedef unsigned long long int uint64;


static inline int32 scale32via64(int32 v32, int32 m32, int32 d32)
{
    return ((int64)v32) * m32 / d32;
}


typedef float   float32;
typedef double  float64;


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __MISC_TYPES_H */
