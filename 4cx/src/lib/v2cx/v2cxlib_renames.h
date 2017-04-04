#ifndef __V2CXLIB_RENAMES_H
#define __V2CXLIB_RENAMES_H


// cxlib API
#define cx_connect        v2_cx_connect
#define cx_connect_n      v2_cx_connect_n
#define cx_openbigc       v2_cx_openbigc
#define cx_openbigc_n     v2_cx_openbigc_n
#define cx_consolelogin   v2_cx_consolelogin
#define cx_consolelogin_n v2_cx_consolelogin_n
#define cx_close          v2_cx_close
#define cx_ping           v2_cx_ping
#define cx_nonblock       v2_cx_nonblock
#define cx_isready        v2_cx_isready
#define cx_result         v2_cx_result
#define cx_setnotifier    v2_cx_setnotifier
#define cx_getcyclesize   v2_cx_getcyclesize
#define cx_execcmd        v2_cx_execcmd
#define cx_getprompt      v2_cx_getprompt
#define cx_begin          v2_cx_begin
#define cx_run            v2_cx_run
#define cx_subscribe      v2_cx_subscribe
#define cx_setvalue       v2_cx_setvalue
#define cx_setvgroup      v2_cx_setvgroup
#define cx_setvset        v2_cx_setvset
#define cx_getvalue       v2_cx_getvalue
#define cx_getvgroup      v2_cx_getvgroup
#define cx_getvset        v2_cx_getvset
#define cx_bigcmsg        v2_cx_bigcmsg
#define cx_bigcreq        v2_cx_bigcreq
#define cx_strerror       v2_cx_strerror
#define cx_perror         v2_cx_perror
#define cx_perror2        v2_cx_perror2
#define cx_strreason      v2_cx_strreason
#define cx_strrflag_short v2_cx_strrflag_short
#define cx_strrflag_long  v2_cx_strrflag_long
#define cx_parse_chanref  v2_cx_parse_chanref

// cxlib_wait_procs API
#define _cxlib_begin_wait v2__cxlib_begin_wait
#define _cxlib_break_wait v2__cxlib_break_wait

//

/*
    Note:
        Thus we prevent v4's cx.h and cxlib.h (which are also being included,
        because of #include "cx.h","cxlib.h") from expansion, since they
        employ the same "__CXLIB_H" and "__CX_H" guards.
 */
#include "src_cx.h"
#include "src_cxlib.h"


#endif /* __V2CXLIB_RENAMES_H */
