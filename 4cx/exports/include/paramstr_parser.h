#ifndef __PARAMSTR_PARSER_H
#define __PARAMSTR_PARSER_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <sys/types.h>


#define PSP_OFFSET_OF(struct_type,field_name) \
    ((int) ( ((char *)(&(((struct_type *)NULL)->field_name))) - ((char *)NULL) ))

#define PSP_SIZEOF(type,field) sizeof(((type *)NULL)->field)

enum
{
    PSP_R_OK      =  0,
    PSP_R_USRERR  = -1,
    PSP_R_APPERR  = -2,
    PSP_R_FAILURE = -3
};

enum
{
    PSP_MF_NOINIT       = 1 << 0,
    PSP_MF_SKIP_UNKNOWN = 1 << 1
};

typedef enum
{
    PSP_T_NULL     = 0,
    PSP_T_NOP      = 1,
    PSP_T_VOID     = 2,

    PSP_T_INCLUDE  = 10,
    PSP_T_PSP      = 11,
    PSP_T_PLUGIN   = 12,

    PSP_T_INT      = 50,
    PSP_T_SELECTOR = 51,
    PSP_T_LOOKUP   = 52,
    PSP_T_FLAG     = 53,

    PSP_T_REAL     = 70,

    PSP_T_STRING   = 80,
    PSP_T_MSTRING  = 81,
} psp_paramtype_t;

typedef struct
{
    const char *label;
    int         val;
} psp_lkp_t;

typedef int (*psp_pluginp_t)(const char *str, const char **endptr,
                             void *rec, size_t recsize,
                             const char *separators, const char *terminators,
                             void *privptr, char **errstr);

typedef struct
{
    struct _psp_paramdescr_t_struct *table;
} psp_var_p_include_t;

typedef struct
{
    struct _psp_paramdescr_t_struct *descr;
    const char *separators;
    char        equals_c;
} psp_var_p_psp_t;

typedef struct
{
    psp_pluginp_t  parser;
    void          *privptr;
} psp_var_p_plugin_t;

typedef struct
{
    int         defval;
    int         minval;
    int         maxval;
} psp_var_p_int_t;

typedef struct
{
    int         defval;
    char      **list;
} psp_var_p_selector_t;

typedef struct
{
    int         defval;
    psp_lkp_t  *table;
} psp_var_p_lookup_t;

typedef struct
{
    int         theval;
    int         is_defval;
} psp_var_p_flag_t;

typedef struct
{
    const char *defval;
} psp_var_p_string_t;

typedef struct
{
    const char *defval;
} psp_var_p_mstring_t;

typedef struct
{
    double      defval;
    double      minval;
    double      maxval;
} psp_var_p_real_t;


typedef union
{
    psp_var_p_include_t  p_include;
    psp_var_p_psp_t      p_psp;
    psp_var_p_plugin_t   p_plugin;
    psp_var_p_int_t      p_int;
    psp_var_p_selector_t p_selector;
    psp_var_p_lookup_t   p_lookup;
    psp_var_p_flag_t     p_flag;
    psp_var_p_string_t   p_string;
    psp_var_p_mstring_t  p_mstring;
    psp_var_p_real_t     p_real;
} psp_var_t;


typedef struct _psp_paramdescr_t_struct
{
    const char      *name;
    psp_paramtype_t  t;
    int              offset;
    size_t           size;
    int              rsrvd1;
    int              rsrvd2;
    psp_var_t        var;
} psp_paramdescr_t;


#define PSP_P_NOP(     n, sname)                                             \
    {n, PSP_T_NOP, 0, 0, 0, 0, {}}

#define PSP_P_VOID(    n)                                                    \
    {n, PSP_T_VOID, 0, 0, 0, 0, {}}

#define PSP_P_INCLUDE( n, table, aux_offset)                                 \
    {"#"n, PSP_T_INCLUDE, aux_offset, 0,                                     \
    0, 0,                                                                    \
    {.p_include  = {table}}}

#define PSP_P_PSP(     n, sname, field, items, eqc, seprs)                   \
    {n, PSP_T_PSP,      PSP_OFFSET_OF(sname,field), PSP_SIZEOF(sname,field), \
    0, 0,                                                                    \
    {.p_psp      = {items, seprs, eqc}}}

#define PSP_P_PLUGIN(  n, sname, field, plugin, uptr)                        \
    {n, PSP_T_PLUGIN,   PSP_OFFSET_OF(sname,field), PSP_SIZEOF(sname,field), \
    0, 0,                                                                    \
    {.p_plugin   = {plugin, uptr}}}

#define PSP_P_INT(     n, sname, field, defv, minv, maxv)                    \
    {n, PSP_T_INT,      PSP_OFFSET_OF(sname,field), PSP_SIZEOF(sname,field), \
    0, 0,                                                                    \
    {.p_int      = {defv, minv, maxv}}}

#if 0
#undef PSP_P_INT
#define PSP_P_INT(     n, sname, field, defv, minv, maxv)                    \
    {n, PSP_T_INT,      PSP_OFFSET_OF(sname,field), PSP_SIZEOF(sname,field), \
    0, 0,                                                                    \
    ((psp_var_t) (psp_var_p_int_t){defv, minv, maxv})}
#endif

#define PSP_P_SELECTOR(n, sname, field, defv, items)                         \
    {n, PSP_T_SELECTOR, PSP_OFFSET_OF(sname,field), PSP_SIZEOF(sname,field), \
    0, 0,                                                                    \
    {.p_selector = {defv, items}}}

#define PSP_P_LOOKUP(  n, sname, field, defv, items)                         \
    {n, PSP_T_LOOKUP,   PSP_OFFSET_OF(sname,field), PSP_SIZEOF(sname,field), \
    0, 0,                                                                    \
    {.p_lookup   = {defv, items}}}

#define PSP_P_FLAG(    n, sname, field, v, isdef)                            \
    {n, PSP_T_FLAG,     PSP_OFFSET_OF(sname,field), PSP_SIZEOF(sname,field), \
    0, 0,                                                                    \
    {.p_flag     = {v, isdef}}}

#define PSP_P_MSTRING( n, sname, field, defv, maxsize)                       \
    {n, PSP_T_MSTRING,  PSP_OFFSET_OF(sname,field), maxsize,                 \
    0, 0,                                                                    \
    {.p_mstring  = {defv}}}

#define PSP_P_STRING(  n, sname, field, defv)                                \
    {n, PSP_T_STRING,   PSP_OFFSET_OF(sname,field), PSP_SIZEOF(sname,field), \
    0, 0,                                                                    \
    {.p_string   = {defv}}}

#define PSP_P_REAL(    n, sname, field, defv, minv, maxv)                    \
    {n, PSP_T_REAL,     PSP_OFFSET_OF(sname,field), PSP_SIZEOF(sname,field), \
    0, 0,                                                                    \
    {.p_real     = {defv, minv, maxv}}}

#define PSP_P_END() {NULL, 0, 0, 0, 0, 0, {}}


int psp_parse   (const char *str, const char **endptr,
                 void *rec,
                 char equals_c, const char *separators, const char *terminators,
                 psp_paramdescr_t *table);

int psp_parse_as(const char *str, const char **endptr,
                 void *rec,
                 char equals_c, const char *separators, const char *terminators,
                 psp_paramdescr_t *table,
                 int mode_flags);

int psp_parse_v (const char *str, const char **endptr,
                 void *recs[], psp_paramdescr_t *tables[], int pair_count,
                 char equals_c, const char *separators, const char *terminators,
                 int mode_flags);

void psp_free(void *rec, psp_paramdescr_t *table);

int  psp_set_include_table(psp_paramdescr_t *main_table,
                           const char *name, psp_paramdescr_t *include_table);

const char *psp_error(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __PARAMSTR_PARSER_H */
