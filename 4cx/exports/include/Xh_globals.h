/*

*/

#ifndef _XH_GLOBALS_H
#define _XH_GLOBALS_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <X11/Intrinsic.h>
#include <Xm/Xm.h>


#if defined(D) || defined(V)
  #error Sorry, unable to compile when D or V defined
  #error (see warning(s) below for details).
#endif

#ifdef __XH_GLOBALS_C
  #define D
  #define V(value) = value
#else
  #define D extern
  #define V(value)
#endif /* __XH_GLOBALS_C */


D XtAppContext     xh_context;
D XmStringCharSet  xh_charset V(XmSTRING_DEFAULT_CHARSET);
D Display         *TheDisplay;


#undef D
#undef V


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* _XH_GLOBALS_H */
