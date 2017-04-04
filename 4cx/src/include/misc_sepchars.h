#ifndef __MISC_SEPCHARS_H
#define __MISC_SEPCHARS_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


enum
{
    CX_GS_C = '\f',    // group separator
#define CX_GS_S "\f"
    CX_RS_C = '\v',    // record separator
#define CX_RS_S "\f"
    CX_US_C = '\t',    // unit separator
#define CX_US_S "\t"
    CX_SS_C = '\b'     // subfield separator
#define CX_SS_S "\b"
};

enum
{
    CX_KNOB_NOLABELTIP_PFX_C = '\n',
#define CX_KNOB_NOLABELTIP_PFX_S "\n"
};


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __MISC_SEPCHARS_H */
