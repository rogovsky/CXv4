#ifndef __REMSRV_H
#define __REMSRV_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include "cxsd_driver.h"
#include "remcxsd.h"


/* General definitions */

typedef const char * (*remsrv_clninfo_t)(int devid);

typedef void (*remsrv_cprintf_t)(void *context, const char *format, ...)
    /*!!! vvv because of GCC bug#3481 "function attributes should apply to function pointers too"; fixed in 2005-12-10, at least in 4.1.0 */
    /*__attribute__(( format (printf, 2, 3) ))*/;

typedef void (*remsrv_conscmd_proc_t)(int               nargs,
                                      const char       *argp[],
                                      void             *context,
                                      remsrv_cprintf_t  do_printf);

typedef struct
{
    const char            *name;
    remsrv_conscmd_proc_t  processer;
    const char            *comment;
} remsrv_conscmd_descr_t;


/* remsrv_main */

// This should be provided by "user"
extern CxsdDriverModRec **remsrv_drivers;

int remsrv_main(int argc, char *argv[],
                const char             *prog_prompt_name,
                const char             *prog_issue_name,
                remsrv_conscmd_descr_t *prog_cmd_table,
                remsrv_clninfo_t        prog_clninfo);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __REMSRV_H */
