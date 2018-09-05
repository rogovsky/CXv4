#ifndef __PPF4TD_H
#define __PPF4TD_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <unistd.h>

#include "misc_macros.h"
#include "cx_version.h"
#include "cx_module.h"


/* Unified input API */

struct _ppf4td_ctx_t_struct;

typedef int (*ppf4td_open_t) (struct _ppf4td_ctx_t_struct *ctx, const char *reference);
typedef int (*ppf4td_peekc_t)(struct _ppf4td_ctx_t_struct *ctx, int *ch_p);
typedef int (*ppf4td_nextc_t)(struct _ppf4td_ctx_t_struct *ctx, int *ch_p);
typedef int (*ppf4td_close_t)(struct _ppf4td_ctx_t_struct *ctx);

enum
{
    PPF4TD_LINESYNC_NONE = 0,
    PPF4TD_LINESYNC_HASH = 1,
    PPF4TD_LINESYNC_HLIN = 2,
};

typedef struct _ppf4td_vmt_t_struct
{
    cx_module_rec_t  mr;

    ppf4td_open_t    open;
    ppf4td_close_t   close;
    ppf4td_peekc_t   peekc;
    ppf4td_nextc_t   nextc;

    int              linesync_type;

    struct _ppf4td_vmt_t_struct *next;
} ppf4td_vmt_t;


/*  */

enum
{
    PPF4TD_OK  = +1,
    PPF4TD_EOF =  0,
    PPF4TD_ERR = -1,
};

/*==== Some additional error codes -- all are negative =============*/
enum
{
    PPF4TD_EEOF   =  0,
    PPF4TD_EEOL   = -1,
    PPF4TD_EIDENT = -2,
    PPF4TD_EINT   = -3,
    PPF4TD_E2LONG = -4,
    PPF4TD_EQUOTE = -5,
    PPF4TD_EXDIG  = -6,
};


enum
{
    PPF4TD_FLAG_NONE      = 0,
    PPF4TD_FLAG_DASH      = 1 << 0,           // get_ident()
    PPF4TD_FLAG_UNSIGNED  = PPF4TD_FLAG_DASH, // get_int()
    PPF4TD_FLAG_NOQUOTES  = PPF4TD_FLAG_DASH, // get_string()

    PPF4TD_FLAG_DOT       = 1 << 1,           // get_ident()
    PPF4TD_FLAG_IGNQUOTES = PPF4TD_FLAG_DOT,  // ppf4td_get_string()

    PPF4TD_FLAG_EOLTERM   = 1 << 10, // EOL (End Of Line)
    PPF4TD_FLAG_HSHTERM   = 1 << 11, // HaSH
    PPF4TD_FLAG_SPCTERM   = 1 << 12, // SPaCe
    PPF4TD_FLAG_COMTERM   = 1 << 13, // COMma
    PPF4TD_FLAG_SEMTERM   = 1 << 14, // SEMicolon
    PPF4TD_FLAG_BRCTERM   = 1 << 15, // '}'-BRaCe, ']'-BRacKet, ')'-PaReN

    PPF4TD_FLAG_JUST_SKIP = 1 << 31, // get_string()
};

enum {PPF4TD_UCBUF_SIZE = 100}; // This is mainly for "#line ...", so a small buf is enough

typedef struct _ppf4td_ctx_t_struct
{
    ppf4td_vmt_t   *vmt;
    void           *imp_privptr;

    char            _curref[1000];
    int             _curline;

    int             _is_at_bol;
    int             _prev_ch;

    int             _ucbuf[PPF4TD_UCBUF_SIZE]; // Note 'int', to be able to hold ANY value from getc(), incl. EOF
    int             _ucbuf_used;
    int             _l0buf[PPF4TD_UCBUF_SIZE]; // Note 'int', to be able to hold ANY value from getc(), incl. EOF
    int             _l0buf_used;
} ppf4td_ctx_t;

int ppf4td_open (ppf4td_ctx_t *ctx, const char *def_scheme, const char *reference);
int ppf4td_close(ppf4td_ctx_t *ctx);

int         ppf4td_peekc     (ppf4td_ctx_t *ctx, int *ch_p);
int         ppf4td_nextc     (ppf4td_ctx_t *ctx, int *ch_p);
int         ppf4td_ungetchars(ppf4td_ctx_t *ctx, const char *buf, int len);
int         ppf4td_is_at_eol (ppf4td_ctx_t *ctx);

int         ppf4td_skip_white(ppf4td_ctx_t *ctx);
int         ppf4td_get_ident (ppf4td_ctx_t *ctx, int flags, char *buf, size_t bufsize);
int         ppf4td_get_int   (ppf4td_ctx_t *ctx, int flags, int  *vp,  int defbase, int *base_p);
int         ppf4td_get_double(ppf4td_ctx_t *ctx, int flags, double *vp);
int         ppf4td_get_string(ppf4td_ctx_t *ctx, int flags, char *buf, size_t bufsize);
int         ppf4td_read_line (ppf4td_ctx_t *ctx, int flags, char *buf, size_t bufsize);

const char *ppf4td_cur_ref   (ppf4td_ctx_t *ctx);
int         ppf4td_cur_line  (ppf4td_ctx_t *ctx);

char       *ppf4td_strerror  (int errnum);


#define PPF4TD_PLUGIN_MODREC_SUFFIX     _ppf4td_modrec
#define PPF4TD_PLUGIN_MODREC_SUFFIX_STR __CX_STRINGIZE(PPF4TD_PLUGIN_MODREC_SUFFIX)

enum {PPF4TD_MODREC_MAGIC = 0x74346670}; /* Little-endian 'g4tp' */
enum
{
    PPF4TD_MODREC_VERSION_MAJOR = 1,
    PPF4TD_MODREC_VERSION_MINOR = 0,
    PPF4TD_MODREC_VERSION = CX_ENCODE_VERSION(PPF4TD_MODREC_VERSION_MAJOR,
                                              PPF4TD_MODREC_VERSION_MINOR)
};

#define PPF4TD_PLUGIN_MODREC_NAME(name) \
    __CX_CONCATENATE(name, PPF4TD_PLUGIN_MODREC_SUFFIX)

#define DECLARE_PPF4TD_PLUGIN(name) \
    ppf4td_vmt_t PPF4TD_PLUGIN_MODREC_NAME(name)

#define DEFINE_PPF4TD_PLUGIN(name, comment,             \
                             init_m, term_m,            \
                             open_f, close_f,           \
                             peekc_f, nextc_f,          \
                             sync_type)                 \
    ppf4td_vmt_t PPF4TD_PLUGIN_MODREC_NAME(name) =      \
    {                                                   \
        {                                               \
            PPF4TD_MODREC_MAGIC, PPF4TD_MODREC_VERSION, \
            __CX_STRINGIZE(name), comment,              \
            init_m, term_m,                             \
        },                                              \
        open_f, close_f, peekc_f, nextc_f,              \
        sync_type,                                      \
        NULL                                            \
    }

int ppf4td_register_plugin  (ppf4td_vmt_t *metric);
int ppf4td_deregister_plugin(ppf4td_vmt_t *metric);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __PPF4TD_H */
