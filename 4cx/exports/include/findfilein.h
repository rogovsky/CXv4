#ifndef __FINDFILEIN_H
#define __FINDFILEIN_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#if 1  /* *nix */
  #define FINDFILEIN_SEP      ":"
  #define FINDFILEIN_SEP_CHAR ':'
#else  /* Windows, ?Symbian?, etc. -- where ':' is drive-letter designator */
  #define FINDFILEIN_SEP      ";"
  #define FINDFILEIN_SEP_CHAR ';'
#endif


#define FINDFILEIN_ERR ((void*)1)

typedef void * (*findfilein_check_proc)(const char *name,
                                        const char *path,
                                        void       *privptr);

void *findfilein(const char            *name,
                 const char            *argv0,
                 findfilein_check_proc  checker, void *privptr,
                 const char            *search_path);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __FINDFILEIN_H */
