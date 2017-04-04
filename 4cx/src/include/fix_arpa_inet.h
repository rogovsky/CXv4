/*********************************************************************
*                                                                    *
*  fix_arpa_inet.h                                                   *
*      Should be included instead of <arpa/inet.h> in order to       *
*      avoid conflicts between declarations of htons() and ntohs()   *
*      in .../gcc-lib/.../include/sys/byteorder.h (which are bogus)  *
*      and arpa/inet.h.                                              *
*                                                                    *
*  Solution:                                                         *
*      arpa/inet.h declares these functions only if there are        *
*      no #defines (macros) with same names.                         *
*      So, this file checks if those macros are actually defined,    *
*      and if not, defines them for arpa/inet.h and undefines        *
*      afterwards.                                                   *
*                                                                    *
*********************************************************************/

#if defined(OS_UNIXWARE)
  #undef ___HTONS_IS_DEFINED
  #ifdef htons
    #define ___HTONS_IS_DEFINED
  #else
    #define htons(x) (x)
  #endif

  #undef ___NTOHS_IS_DEFINED
  #ifdef ntohs
    #define ___NTOHS_IS_DEFINED
  #else
    #define ntohs(x) (x)
  #endif
  
  #include <arpa/inet.h>

  #ifndef ___HTONS_IS_DEFINED
    #undef htons
  #endif

  #ifndef ___NTOHS_IS_DEFINED
    #undef ntohs
  #endif

#else
  #include <arpa/inet.h>
#endif /* OS_UNIXWARE */
