#ifndef __CX_VERSION_H
#define __CX_VERSION_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


/*** Common version-encoding scheme *********************************/

#define CX_ENCODE_VERSION_MmPs(MM, mm, PP, sss) \
    ((MM) * 10000000 + (mm) * 100000 + (PP) * 1000 + (sss))
#define CX_ENCODE_VERSION(M, m) CX_ENCODE_VERSION_MmPs(M, m, 0, 0)

static inline int CX_MAJOR_OF(int v) {return  v / 10000000;}
static inline int CX_MINOR_OF(int v) {return (v /   100000) % 100;}
static inline int CX_PATCH_OF(int v) {return (v /     1000) % 100;}
static inline int CX_SNAP_OF (int v) {return             v  % 1000;}

static inline int CX_VERSION_IS_COMPATIBLE(int that, int my)
{
    return CX_MAJOR_OF(that) == CX_MAJOR_OF(my)  &&
           CX_MINOR_OF(that) <= CX_MINOR_OF(my);
}


/*** CX version *****************************************************/

enum
{
    CX_VERSION_MAJOR      = 4,
    CX_VERSION_MINOR      = 0,
    CX_VERSION_PATCHLEVEL = 0,
    CX_VERSION = CX_ENCODE_VERSION_MmPs(CX_VERSION_MAJOR,
                                        CX_VERSION_MINOR,
                                        CX_VERSION_PATCHLEVEL,
                                        0)
};


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CX_VERSION_H */
