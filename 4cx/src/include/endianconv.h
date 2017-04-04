#ifndef __ENDIANCONV_H
#define __ENDIANCONV_H


#include "misc_types.h"
#include "cx_sysdeps.h"


static inline uint32  swab32(uint32 ui32)
{
    return
        ((ui32 << 24) & 0xFF000000) |
        ((ui32 << 8 ) & 0x00FF0000) |
        ((ui32 >> 8 ) & 0x0000FF00) |
        ((ui32 >> 24) & 0x000000FF);
}

static inline uint16  swab16(uint16 ui16)
{
    return
        ((ui16 << 8) & 0xFF00) |
        ((ui16 >> 8) & 0x00FF);
}

typedef float  flt32;
typedef double flt64;

#if BYTE_ORDER == LITTLE_ENDIAN

static inline uint16  l2h_u16(uint16 lite_u16) {return lite_u16;}
static inline uint32  l2h_u32(uint32 lite_u32) {return lite_u32;}
static inline uint64  l2h_u64(uint64 lite_u64) {return lite_u64;}
static inline flt32   l2h_f32(flt32  lite_f32) {return lite_f32;}
static inline flt64   l2h_f64(flt64  lite_f64) {return lite_f64;}

static inline uint16  h2l_u16(uint16 host_u16) {return host_u16;}
static inline uint32  h2l_u32(uint32 host_u32) {return host_u32;}
static inline uint64  h2l_u64(uint32 host_u64) {return host_u64;}
static inline flt32   h2l_f32(flt32  host_f32) {return host_f32;}
static inline flt64   h2l_f64(flt64  host_f64) {return host_f64;}

static inline uint16  b2h_u16(uint16 bige_u16) {return swab16(bige_u16);}
static inline uint32  b2h_u32(uint32 bige_u32) {return swab32(bige_u32);}
static inline uint64  b2h_u64(uint64 bige_u64) {return bige_u64;}
static inline flt32   b2h_f32(flt32  bige_f32) {return swab32(bige_f32);}
static inline flt64   b2h_f64(flt64  bige_f64) {return bige_f64;}

static inline uint16  h2b_u16(uint16 host_u16) {return swab16(host_u16);}
static inline uint32  h2b_u32(uint32 host_u32) {return swab32(host_u32);}
static inline uint64  h2b_u64(uint32 host_u64) {return host_u64;}
static inline flt32   h2b_f32(flt32  host_f32) {return swab32(host_f32);}
static inline flt64   h2b_f64(flt64  host_f64) {return host_f64;}

#elif BYTE_ORDER == BIG_ENDIAN

#else

#error Sorry, the "BYTE_ORDER" is neither "LITTLE_ENDIAN" nor "BIG_ENDIAN"

#endif


#endif /* __ENDIANCONV_H */
