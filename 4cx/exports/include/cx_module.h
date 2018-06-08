#ifndef __CX_MODULE_H
#define __CX_MODULE_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


typedef int  (*cx_module_init_func)(void);
typedef void (*cx_module_term_proc)(void);

typedef struct
{
    int                  magicnumber;
    int                  version;
    const char          *name;
    const char          *comment;

    cx_module_init_func  init_mod;
    cx_module_term_proc  term_mod;
} cx_module_rec_t;


typedef struct
{
    const char *kindname;
    int         magicnumber;
    int         version;
} cx_module_desc_t;


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __CX_MODULE_H */
