#ifndef __MISC_MACROS_H
#define __MISC_MACROS_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>

#include "misc_types.h"


/* What a mess!  Instead of writing something like
       #define __CX_STRINGIZE(a) #a
       #define ABSTRACT_DRIVER_FILE __CX_STRINGIZE(a_##ABSTRACT_NAME.h)
   we need that multilayer of code, simply because CPP has "features"
   of not expanding the macros (happily, not parameters!)
   used in '#' and '##'!
 */
#define __CX_STRINGIZE_HELPER(x)      #x
#define __CX_STRINGIZE(x)             __CX_STRINGIZE_HELPER(x)

#define __CX_CONCATENATE_HELPER(a,b)  a##b
#define __CX_CONCATENATE(a,b)         __CX_CONCATENATE_HELPER(a,b)


#define countof(array) ((signed int)(sizeof(array) / sizeof(array[0])))

#ifndef offsetof
  #define offsetof(struct_type,field_name) \
      ((int) ( ((char *)(&(((struct_type *)NULL)->field_name))) - ((char *)NULL) ))
#endif
#define sizeof_field(struct_type,field) sizeof(((struct_type *)NULL)->field)


static inline void *lint2ptr(long  v){return (void*)v;}
static inline long  ptr2lint(void *p){return (long )p;}

#define LINT2PTR(v) ((void*)v)
#define PTR2LINT(p) ((long )p)


#define RESCALE_VALUE(v, from_min, from_max, to_min, to_max) \
    ( (v - (from_min)) * ((to_max) - (to_min)) / ((from_max) - (from_min)) + (to_min) )


#if defined(ERESTART)
  #define SHOULD_RESTART_SYSCALL() (errno == EINTR  ||  errno == ERESTART)
#else
  #define SHOULD_RESTART_SYSCALL() (errno == EINTR)
#endif

#define ERRNO_WOULDBLOCK() (errno == EAGAIN  ||  errno == EWOULDBLOCK)

#define IS_A_TEMPORARY_CONNECT_ERROR() \
    (                                  \
     errno == ETIMEDOUT     ||         \
     errno == ECONNREFUSED  ||         \
     errno == EHOSTUNREACH  ||         \
     errno == ENETUNREACH   ||         \
     errno == ENETDOWN                 \
    )

static inline int CheckReadR(int r)
{
    if (r >  0) return 0;
    if (r == 0) errno = 0;
    
    return -1;
}



/*
 *  strzcpy -- same as strncpy(), but always adds a trailing '\0'
 */

static inline char *strzcpy(char *dest, const char *src, size_t n)
{
  char *result;

  result = strncpy(dest, src, n);
  dest[n - 1] = '\0';
  return result;
}


#if !defined(CPU_MCF5200) /*!!!A "hackish" check -- should better introduce an "OPTION_HAS_VSNPRINTF"; If we could only check for "uClibc"! */

static int check_snprintf(char *str, size_t size, const char *format, ...)
                          __attribute__((format (printf, 3, 4)))
                          __attribute__((unused));
static int check_snprintf(char *str, size_t size, const char *format, ...)
{
  int      r;
  va_list  ap;
  
    va_start(ap, format);
    r = vsnprintf(str, size, format, ap);
    va_end(ap);
    
    return r < 0  ||  ((unsigned int) r) > size - 1;
}

static inline char *snprintf_dbl_trim(char *buf, size_t bufsize,
                                      const char *dpyfmt, double v, int ltrim)
{
  char *p;
  int   len;

    len = snprintf(buf, bufsize, dpyfmt, v);
    if (len < 0  ||  (size_t)len > bufsize-1) buf[len = bufsize-1] = '\0';

    p = buf + len - 1;
    while (p > buf  &&  isspace(*p)) *p-- = '\0';

    p = buf;
    if (ltrim) while (*p == ' ') p++;

    return p;
}
#endif


static inline int32 *memset32(int32 *s, int32 c, unsigned int count)
{
  register int32 *p = s;
  
    for (;  count > 0;  count--) *p++ = c;
    
    return s;
}


#define fast_memcpy  memcpy
#define fast_memmove memmove


/*
 *  safe_free
 *      Calls free() only if the pointer is non-NULL.
 *          Is useful since old systems (like SunOS 4.1) deviate
 *      from standards and don't treat free(NULL) as NO-OP,
 *      but causes errno=EINVAL.
 *
 *  Note:
 *     Should be used only when ptr *may* legitimately be NULL.
 */

static inline void safe_free(void *ptr)
{
    if (ptr != NULL) free(ptr);
}

static inline void *safe_realloc(void *ptr, size_t size)
{
    return ptr == NULL? malloc(size) : realloc(ptr, size);
}


/*
 *  GENERIC_FD_xxx
 *      Macros for operating the max::set pair.
 *
 *  GENERIC_FD_CLEAR   clears the `set', and assigns -1 to `max'
 *  GENERIC_FD_ADD     adds a descripror `s' to the `set', adjusting `max'
 *  GENERIC_FD_REMOVE  removes `s' from the `set', adjusting `max'
 */

#define GENERIC_FD_CLEAR(set, max)         \
    do {                                   \
        FD_ZERO(&set);                     \
        max = -1;                          \
    } while (0)

#define GENERIC_FD_ADD(s, set, max)        \
    do {                                   \
        FD_SET(s, &set);                   \
        if (s > max)  max = s;             \
    } while (0)

#define GENERIC_FD_REMOVE(s, set, max)     \
    do {                                   \
        if (s >= 0  &&  FD_ISSET(s, &set)) \
        {                                  \
            FD_CLR(s, &set);               \
            if (s == max)                  \
                while (max >= 0  &&  !FD_ISSET(max, &set))      \
                    --max;                 \
        }                                  \
    } while (0)

#define GENERIC_FD_DECLARE(Name)           \
    void Clear##Name(void);                \
    void Add##Name(int s);                 \
    void Remove##Name(int s);

#define GENERIC_FD_DEFINE(Name, var, storage) \
    storage void Clear##Name (void)           \
    {                                         \
        GENERIC_FD_CLEAR (   var, max##var);  \
    }                                         \
    storage void Add##Name   (int s)          \
    {                                         \
        GENERIC_FD_ADD   (s, var, max##var);  \
    }                                         \
    storage void Remove##Name(int s)          \
    {                                         \
        GENERIC_FD_REMOVE(s, var, max##var);  \
    }


/*
 *
 */

#define GENERIC_SLOTARRAY_DECLARE(storage, cname, slot_type,              \
                                  single_obj_arg, last_obj_arg...)        \
    storage int  FindFree##cname##Slot(single_obj_arg);                   \
    storage int  Get##cname##Slot     (single_obj_arg);                   \
    storage slot_type * Access##cname##Slot(int id , ##last_obj_arg)      \
                        __attribute__((unused));                          \
    storage int  Foreach##cname##Slot (int (*checker)(slot_type*, void*), \
                                       void *privptr,                     \
                                       ##last_obj_arg)                    \
                 __attribute__((unused));                                 \
    storage void Rls##cname##Slot     (int id , ##last_obj_arg)           \
                 __attribute__((unused));                                 \
    storage void Destroy##cname##SlotArray(single_obj_arg)                \
                 __attribute__((unused));

#define GENERIC_SLOTARRAY_DECLARE_FIXED_FIELDS(storage, cname, slot_type, \
                                               fieldname_pfx, MAX_COUNT)  \
    storage slot_type fieldname_pfx##_list[MAX_COUNT];
#define GENERIC_SLOTARRAY_DEFINE_FIXED(storage, cname, slot_type,             \
                                       fieldname_pfx, in_use_field,           \
                                       in_use_val_unused, in_use_val_used,    \
                                       FIRST_INDEX, ALLOC_INC, MAX_COUNT,     \
                                       obj_pfx, obj_ref,                      \
                                       single_obj_arg, last_obj_arg...)       \
\
    storage int  FindFree##cname##Slot(single_obj_arg)                        \
    {                                                                         \
      int  i;                                                                 \
                                                                              \
        for (i = FIRST_INDEX;  i < MAX_COUNT;  i++)                           \
            if (obj_pfx fieldname_pfx##_list[i].in_use_field ==               \
                in_use_val_unused)                                            \
            {                                                                 \
                bzero(obj_pfx fieldname_pfx##_list + i, sizeof(slot_type));   \
                obj_pfx fieldname_pfx##_list[i].in_use_field = in_use_val_used; \
                return i;                                                     \
            }                                                                 \
                                                                              \
        return -1;                                                            \
    }                                                                         \
\
    storage int  Get##cname##Slot     (single_obj_arg)                 \
    {                                                                  \
        return FindFree##cname##Slot(obj_ref);                         \
    }                                                                  \
\
    storage slot_type * Access##cname##Slot(int id , ##last_obj_arg)   \
    {                                                                  \
        return obj_pfx fieldname_pfx##_list + id;                      \
    }                                                                  \
\
    storage int  Foreach##cname##Slot (int (*checker)(slot_type*, void*), \
                                       void *privptr,                  \
                                       ##last_obj_arg)                 \
    {                                                                  \
      int  i;                                                          \
      int  r;                                                          \
                                                                       \
        for (i = FIRST_INDEX;  i < MAX_COUNT;  i++)                    \
            if (obj_pfx fieldname_pfx##_list[i].in_use_field !=        \
                in_use_val_unused)                                     \
            {                                                          \
                r = checker(obj_pfx fieldname_pfx##_list + i, privptr);\
                if (r != 0) return i;                                  \
            }                                                          \
                                                                       \
        return -1;                                                     \
    }                                                                  \
\
    storage void Destroy##cname##SlotArray(single_obj_arg)             \
    {                                                                  \
    }

#define GENERIC_SLOTARRAY_DECLARE_GROWING_FIELDS(storage, cname, slot_type, \
                                                 fieldname_pfx, MAX_COUNT)  \
    storage slot_type *fieldname_pfx##_list;                                \
    storage int        fieldname_pfx##_list_allocd;
#define GENERIC_SLOTARRAY_DEFINE_GROWING(storage, cname, slot_type,           \
                                         fieldname_pfx, in_use_field,         \
                                         in_use_val_unused, in_use_val_used,  \
                                         FIRST_INDEX, ALLOC_INC, MAX_COUNT,   \
                                         obj_pfx, obj_ref,                    \
                                         single_obj_arg, last_obj_arg...)     \
\
    storage int FindFree##cname##Slot(single_obj_arg)                         \
    {                                                                         \
      int  i;                                                                 \
                                                                              \
        for (i = FIRST_INDEX;  i < obj_pfx fieldname_pfx##_list_allocd;  i++) \
            if (obj_pfx fieldname_pfx##_list[i].in_use_field ==               \
                in_use_val_unused)                                            \
            {                                                                 \
                bzero(obj_pfx fieldname_pfx##_list + i, sizeof(slot_type));   \
                obj_pfx fieldname_pfx##_list[i].in_use_field = in_use_val_used; \
                return i;                                                     \
            }                                                                 \
                                                                              \
        return -1;                                                            \
    }                                                                         \
\
    storage int Get##cname##Slot     (single_obj_arg)                  \
    {                                                                  \
      int        i;                                                    \
      slot_type *new_list;                                             \
                                                                       \
        /* Try to find a free slot */                                  \
        i = FindFree##cname##Slot(obj_ref);                            \
        if (i >= 0) return i;                                          \
                                                                       \
        /* None found -- okay, let's grow the list... */               \
        new_list = safe_realloc(obj_pfx fieldname_pfx##_list,          \
                                sizeof(slot_type)                      \
                                *                                      \
                                (obj_pfx fieldname_pfx##_list_allocd + \
                                 ALLOC_INC));                          \
        if (new_list == NULL) return -1;                               \
                                                                       \
        /* ...zero-fill newly allocated space... */                    \
        bzero(new_list + obj_pfx fieldname_pfx##_list_allocd,          \
              sizeof(slot_type) * ALLOC_INC);                          \
        /* ...mark as unused... */                                     \
        for (i = 0;  i < ALLOC_INC;  i++)                              \
            new_list[obj_pfx fieldname_pfx##_list_allocd + i].in_use_field = \
                in_use_val_unused;                                     \
        /* ...and record its presence */                               \
        obj_pfx fieldname_pfx##_list         = new_list;               \
        obj_pfx fieldname_pfx##_list_allocd += ALLOC_INC;              \
                                                                       \
        /* Re-do search */                                             \
        return FindFree##cname##Slot(obj_ref);                         \
    }                                                                  \
\
    storage slot_type * Access##cname##Slot(int id , ##last_obj_arg)   \
    {                                                                  \
        return obj_pfx fieldname_pfx##_list + id;                      \
    }                                                                  \
\
    storage int  Foreach##cname##Slot (int (*checker)(slot_type*, void*),     \
                                       void *privptr,                         \
                                       ##last_obj_arg)                        \
    {                                                                         \
      int  i;                                                                 \
      int  r;                                                                 \
                                                                              \
        for (i = FIRST_INDEX;  i < obj_pfx fieldname_pfx##_list_allocd;  i++) \
            if (obj_pfx fieldname_pfx##_list[i].in_use_field !=               \
                in_use_val_unused)                                            \
            {                                                                 \
                r = checker(obj_pfx fieldname_pfx##_list + i, privptr);       \
                if (r != 0) return i;                                         \
            }                                                                 \
                                                                              \
        return -1;                                                            \
    }                                                                         \
\
    storage void Destroy##cname##SlotArray(single_obj_arg)             \
    {                                                                  \
        safe_free(obj_pfx fieldname_pfx##_list);                       \
        obj_pfx fieldname_pfx##_list        = NULL;                    \
        obj_pfx fieldname_pfx##_list_allocd = 0;                       \
    }

#define GENERIC_SLOTARRAY_DECLARE_GROWFIXELEM_FIELDS(storage, cname, slot_type, \
                                                     fieldname_pfx, MAX_COUNT)  \
    storage slot_type **fieldname_pfx##_list;                                 \
    storage int         fieldname_pfx##_list_allocd;
#define GENERIC_SLOTARRAY_DEFINE_GROWFIXELEM(storage, cname, slot_type,       \
                                             fieldname_pfx, in_use_field,     \
                                             in_use_val_unused, in_use_val_used,\
                                             FIRST_INDEX, ALLOC_INC, MAX_COUNT, \
                                             obj_pfx, obj_ref,                \
                                             single_obj_arg, last_obj_arg...) \
\
    storage int FindFree##cname##Slot(single_obj_arg)                         \
    {                                                                         \
      int  i;                                                                 \
                                                                              \
        for (i = FIRST_INDEX;  i < obj_pfx fieldname_pfx##_list_allocd;  i++) \
        {                                                                     \
            if      (obj_pfx fieldname_pfx##_list[i] == NULL)                 \
            {                                                                 \
                obj_pfx fieldname_pfx##_list[i] = malloc(sizeof(slot_type));  \
                if (obj_pfx fieldname_pfx##_list[i] == NULL) return -1;       \
            }                                                                 \
            else if (obj_pfx fieldname_pfx##_list[i]->in_use_field !=         \
                     in_use_val_unused)                                       \
                continue;                                                     \
                                                                              \
            bzero(obj_pfx fieldname_pfx##_list[i], sizeof(slot_type));        \
            obj_pfx fieldname_pfx##_list[i]->in_use_field = in_use_val_used;  \
            return i;                                                         \
        }                                                                     \
                                                                              \
        return -1;                                                            \
    }                                                                         \
\
    storage int Get##cname##Slot     (single_obj_arg)                  \
    {                                                                  \
      int         i;                                                   \
      slot_type **new_list;                                            \
                                                                       \
        /* Try to find a free slot */                                  \
        i = FindFree##cname##Slot(obj_ref);                            \
        if (i >= 0) return i;                                          \
                                                                       \
        /* None found -- okay, let's grow the list... */               \
        new_list = safe_realloc(obj_pfx fieldname_pfx##_list,          \
                                sizeof(slot_type *)                    \
                                *                                      \
                                (obj_pfx fieldname_pfx##_list_allocd + \
                                 ALLOC_INC));                          \
        if (new_list == NULL) return -1;                               \
                                                                       \
        /* ...zero-fill newly allocated space... */                    \
        bzero(new_list + obj_pfx fieldname_pfx##_list_allocd,          \
              sizeof(slot_type *) * ALLOC_INC);                        \
        /* ...and record its presence */                               \
        obj_pfx fieldname_pfx##_list         = new_list;               \
        obj_pfx fieldname_pfx##_list_allocd += ALLOC_INC;              \
                                                                       \
        /* Re-do search */                                             \
        return FindFree##cname##Slot(obj_ref);                         \
    }                                                                  \
\
    storage slot_type * Access##cname##Slot(int id , ##last_obj_arg)   \
    {                                                                  \
        return obj_pfx fieldname_pfx##_list[id];                       \
    }                                                                  \
\
    storage int  Foreach##cname##Slot (int (*checker)(slot_type*, void*),     \
                                       void *privptr,                         \
                                       ##last_obj_arg)                        \
    {                                                                         \
      int  i;                                                                 \
      int  r;                                                                 \
                                                                              \
        for (i = FIRST_INDEX;  i < obj_pfx fieldname_pfx##_list_allocd;  i++) \
            if (obj_pfx fieldname_pfx##_list[i] != NULL  &&                   \
                obj_pfx fieldname_pfx##_list[i]->in_use_field !=              \
                in_use_val_unused)                                            \
            {                                                                 \
                r = checker(obj_pfx fieldname_pfx##_list[i], privptr);        \
                if (r != 0) return i;                                         \
            }                                                                 \
                                                                              \
        return -1;                                                            \
    }                                                                         \
\
    storage void Destroy##cname##SlotArray(single_obj_arg)             \
    {                                                                  \
      int  i;                                                          \
                                                                       \
        for (i = FIRST_INDEX;  i < obj_pfx fieldname_pfx##_list_allocd;  i++) \
            safe_free(obj_pfx fieldname_pfx##_list[i]);                \
                                                                       \
        safe_free(obj_pfx fieldname_pfx##_list);                       \
    }



/*
 *
 */

#define ENCODE_FLAG(f,  shift, mask) ((f << shift) & mask)
#define EXTRACT_FLAG(v, shift, mask) ((v & mask) >> shift)


#if 0
/*
 *
 */

typedef {ENUM_LINEAR, ENUM_SHIFT, ENUM_RANDOM} ENUM_KIND_T;

#define DECLARE_ENUM_STRTABLE(name) \
    static struct {int val; const char *stext, *ltext} name[]

#define DEFINE_ENUM_STRTABLELINE(v, s, l) {v, s, l}

#define DEFINE_ENUM_STRFUNCS(typename, min, max, enum_kind, table, \
                             short_miss_format, long_miss_format,  \
                             short_func_name,   long_func_name)    \

char *short_func_name(int val)
{
  static char buf[100];
  
    if (val >= (min)  &&  val <= (max))
    {
    }

    sprintf(buf, short_miss_format, val);
    return buf;
}

#endif


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __MISC_MACROS_H */
