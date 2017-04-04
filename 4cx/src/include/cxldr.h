#ifndef __CXLDR_H
#define __CXLDR_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


enum
{
    CXLDR_FLAG_NONE   = 0,
    CXLDR_FLAG_GLOBAL = 1 << 0,
};


typedef int  (*cxldr_checker_t)(const char *name, void *symptr, void *privptr);
typedef int  (*cxldr_cm_init_t)(const char *name, void *symptr);
typedef void (*cxldr_cm_fini_t)(const char *name, void *symptr);


typedef struct
{
    const char *name;
    void       *handle;
    void       *symptr;
    int         ref_count;
    int         is_bltn;
} cxldr_used_rec_t;

typedef struct
{
    const char *name;
    void       *symptr;
} cxldr_bltn_rec_t;

typedef struct
{
    int               flags;
    const char       *path;
    const char       *fprefix;
    const char       *fsuffix;
    const char       *symprefix;
    const char       *symsuffix;
    
    cxldr_checker_t   checker;
    cxldr_cm_init_t   cm_init;
    cxldr_cm_fini_t   cm_fini;

    cxldr_bltn_rec_t *builtins;

    int               curlistcount;
    cxldr_used_rec_t *curlist;
    int               curlistfixed;
} cxldr_context_t;


#define CXLDR_DECLARE_CONTEXT(varname) \
    cxldr_context_t varname

#define CXLDR_DEFINE_CONTEXT(varname,                              \
                             flags_m,                              \
                             path_s,                               \
                             fprefix_s,   fsuffix_s,               \
                             symprefix_s, symsuffix_s,             \
                             checker_f,                            \
                             cm_init_f, cm_fini_f,                 \
                             builtins_list,                        \
                             curlist_buf, curlist_buf_count)       \
    cxldr_context_t varname =                                      \
    {                                                              \
        .flags   = flags_m,                                         \
        .path    = path_s,                                         \
        .fprefix   = fprefix_s,   .fsuffix   = fsuffix_s,          \
        .symprefix = symprefix_s, .symsuffix = symsuffix_s,        \
        .checker = checker_f,                                      \
        .cm_init = cm_init_f, .cm_fini = cm_fini_f,                \
        .builtins = builtins_list,                                 \
        .curlist = curlist_buf, .curlistfixed = curlist_buf_count, \
    }


int  cxldr_get_module(cxldr_context_t  *ctx,
                      const char       *name,
                      const char       *argv0,
                      void            **symptr_p,
                      char            **errstr_p,
                      void             *privptr);
int  cxldr_fgt_module(cxldr_context_t  *ctx,
                      const char       *name);

int  cxldr_set_path  (cxldr_context_t  *ctx, const char *path);
int  cxldr_set_bltns (cxldr_context_t  *ctx, cxldr_bltn_rec_t *list);


/*
 Note:
     As for now, contexts are designed to be statically-allocated,
     and aren't intended to be created/deleted on-the-fly, so that
     there's no such thing as "cxldr_destroy_ctx()" (which should
     have freed all resources).
 */


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CXLDR_H */
