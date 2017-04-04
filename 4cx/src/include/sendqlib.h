#ifndef __SENDQLIB_H
#define __SENDQLIB_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <stddef.h>


#define SQ_ELEM_IS_EQUAL NULL

/*  */
typedef enum
{
    SQ_FIND_FIRST,
    SQ_FIND_LAST,
    SQ_ERASE_FIRST,
    SQ_ERASE_ALL,
} sq_action_t;

/*  */
typedef enum
{
    SQ_ERROR    = -1,
    SQ_NOTFOUND =  0,
    SQ_FOUND    = +1,
    SQ_ISFIRST  = +2,
} sq_status_t;

/*  */
typedef enum
{
    SQ_ALWAYS,
    SQ_IF_ABSENT,
    SQ_IF_NONEORFIRST,
    SQ_REPLACE_NOTFIRST,
} sq_enqcond_t;

/*  */
enum
{
    SQ_TRIES_DIR = -1,
    SQ_TRIES_INF =  0,
    SQ_TRIES_ONS = +1,
};

/*  */
typedef enum
{
    SQ_TRY_NONE  = 0,
    SQ_TRY_FIRST = 1,
    SQ_TRY_LAST  = 2,
} sq_try_n_t;

enum
{
    SQ_MAX_TRIES = 999999
};


struct _sq_port_t_struct;
struct _sq_q_t_struct;
struct _sq_eprops_t_struct;

/* "Environment"-related types */
typedef int  (*sq_sender_t) (void *elem, void *privptr);
typedef int  (*sq_tim_enq_t)(void *privptr, int usecs);
typedef void (*sq_tim_deq_t)(void *privptr);

/* Element-checker: returns 1 upon match, 0 otherwise */
typedef int  (*sq_elem_checker_t)(void *elem, void *ptr);

typedef void (*sq_cb_t)          (void        *q_privptr,
                                  struct _sq_eprops_t_struct *e,
                                  sq_try_n_t   try_n);


typedef struct _sq_port_t_struct
{
    struct _sq_q_t_struct *first_in_port;
    struct _sq_q_t_struct *last_in_port;

    struct _sq_q_t_struct *first_to_send;
    struct _sq_q_t_struct *last_to_send;
    struct _sq_q_t_struct *current_to_send;

    void              *privptr;

    sq_tim_enq_t       tim_enq;
    sq_tim_deq_t       tim_deq;

    int                tout_set;
} sq_port_t;

typedef struct _sq_q_t_struct
{
    int                magicnumber;

    sq_port_t         *port;
    struct _sq_q_t_struct *prev_in_port;
    struct _sq_q_t_struct *next_in_port;
    struct _sq_q_t_struct *prev_to_send;
    struct _sq_q_t_struct *next_to_send;
    
    size_t             elem_size;
    int                timeout_us;
    void              *privptr;
    sq_sender_t        sender;
    sq_elem_checker_t  eq_cmp_func;
    sq_tim_enq_t       tim_enq;
    sq_tim_deq_t       tim_deq;

    void              *ring;
    int                ring_size;
    int                ring_start;
    int                ring_used;

    int                tout_set;
} sq_q_t;

typedef struct _sq_eprops_t_struct
{
    /* Client-supplied info */
    int      tries;
    int      timeout_us;
    sq_cb_t  callback;
    void    *unused_ptr;

    /* sendqlib-private data */
    int      tries_done;
    int      padding[3];
} sq_eprops_t;


/* Public API */

int          sq_port_init(sq_port_t *port_p,
                          void         *privptr,
                          sq_tim_enq_t  tim_enq,
                          sq_tim_deq_t  tim_deq);
int          sq_port_fini(sq_port_t *port_p);
void         sq_port_tout(sq_port_t *port_p);

int          sq_init    (sq_q_t *q, sq_port_t *port,
                         int qsize, size_t elem_size,
                         int   timeout_us, void *privptr,
                         sq_sender_t        sender,
                         sq_elem_checker_t  eq_cmp_func,
                         sq_tim_enq_t       tim_enq,
                         sq_tim_deq_t       tim_deq);
void         sq_fini    (sq_q_t *q);

void         sq_clear   (sq_q_t *q);
void         sq_pause   (sq_q_t *q, int timeout_us);

sq_status_t  sq_enq     (sq_q_t *q, sq_eprops_t *e, sq_enqcond_t how, void *m);
sq_status_t  sq_foreach (sq_q_t *q,
                         sq_elem_checker_t checker, void *ptr,
                         sq_action_t action,
                         void **found_p);
void         sq_sendnext(sq_q_t *q);

sq_eprops_t *sq_access_e(sq_q_t *q, int n);

void         sq_timeout (sq_q_t *q);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __SENDQLIB_H */
