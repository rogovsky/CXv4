#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "sendqlib.h"


typedef char int8;

enum {SQ_MAGICNUMBER = ('S' << 0) | ('q' << 8) | ('L' << 16) | ('b' << 24)};

static inline void enq_tout(sq_q_t *q, int usecs)
{
  sq_port_t *port_p = q->port;

    if (port_p == NULL)
    {
        q->tim_enq(q->privptr, usecs);
        q->tout_set = 1;
    }
    else
    {
        if (port_p->current_to_send != q) return;
        port_p->tim_enq(port_p->privptr, usecs);
        port_p->tout_set = 1;
    }
}

static inline void deq_tout(sq_q_t *q, int upon_erase_first)
{
  sq_port_t *port_p = q->port;

    if (port_p == NULL)
    {
        if (q->tout_set)
        {
            q->tim_deq(q->privptr);
            q->tout_set = 0;
        }
    }
    else
    {
        if (port_p->current_to_send != q  ||  !upon_erase_first) return;
        port_p->tim_deq(port_p->privptr);
        port_p->tout_set = 0;
    }
}


//////////////////////////////////////////////////////////////////////

static void perform_sendnext(sq_q_t *q, int from_enq);

//////////////////////////////////////////////////////////////////////

int          sq_port_init(sq_port_t    *port_p,
                          void         *privptr,
                          sq_tim_enq_t  tim_enq,
                          sq_tim_deq_t  tim_deq)
{
    bzero(port_p, sizeof(*port_p));

    port_p->privptr = privptr;
    port_p->tim_enq = tim_enq;
    port_p->tim_deq = tim_deq;

    return 0;
}

int          sq_port_fini(sq_port_t *port_p)
{
    /* What can we do here?
       Dequeue timeout -- definitely.
       Anything more? */

    return 0;
}

static void add_to_send(sq_q_t *q)
{
  sq_port_t *port_p = q->port;

    q->prev_to_send = port_p->last_to_send;
    q->next_to_send = NULL;

    if (port_p->last_to_send != NULL)
        port_p->last_to_send->next_to_send = q;
    else
        port_p->first_to_send              = q;
    port_p->last_to_send = q;

    /*Note: managing "current_to_send" is caller's --
      perform_sendnext()'s -- responsibility */
}

static void zer_to_send(sq_q_t *q)
{
  sq_port_t *port_p = q->port;
  sq_q_t    *p1, *p2;

    if (port_p == NULL)
    {
        /* Bark something? */
        return;
    }

    p1 = q->prev_to_send;
    p2 = q->next_to_send;

    if (p1 == NULL) port_p->first_to_send = p2; else p1->next_to_send = p2;
    if (p2 == NULL) port_p->last_to_send  = p1; else p2->prev_to_send = p1;

    q->prev_to_send = q->next_to_send = NULL;

    if (port_p->current_to_send == q)
    {
        /* Note: managing consequenses -- such as removing timeout,
           "bumping" next to run, etc. is CALLER'S responsibility,
           so here we do nothing. */
    }
}

static void bind_to_port(sq_q_t *q, sq_port_t *port_p)
{
    q->port = port_p;

    q->prev_in_port = port_p->last_in_port;
    q->next_in_port = NULL;

    if (port_p->last_in_port != NULL)
        port_p->last_in_port->next_in_port = q;
    else
        port_p->first_in_port              = q;
    port_p->last_in_port = q;

    q->prev_to_send = q->next_to_send = NULL;
}

static void rm_from_port(sq_q_t *q)
{
  sq_port_t *port_p = q->port;
  sq_q_t    *p1, *p2;

    /* I. Remove from to_send if it is there */
    if (q->ring_used != 0)
    {
        p1 = q->prev_to_send; // Note: we step back to PREVIOUS, for next to become current later
        zer_to_send(q);
        if (port_p->current_to_send == q)
        {
            port_p->current_to_send = p1;
            if (port_p->current_to_send == NULL) // Wrap-around
                port_p->current_to_send = port_p->last_to_send;
            if (port_p->current_to_send != NULL)
                perform_sendnext(port_p->current_to_send, 0);
        }
    }

    /* II. Remove from port */
    p1 = q->prev_in_port;
    p2 = q->next_in_port;

    if (p1 == NULL) port_p->first_in_port = p2; else p1->next_in_port = p2;
    if (p2 == NULL) port_p->last_in_port  = p1; else p2->prev_in_port = p1;

    q->prev_in_port = q->next_in_port = NULL;
    q->port = NULL;
}

//////////////////////////////////////////////////////////////////////

int          sq_init    (sq_q_t *q, sq_port_t *port,
                         int qsize, size_t elem_size,
                         int   timeout_us, void *privptr,
                         sq_sender_t        sender,
                         sq_elem_checker_t  eq_cmp_func,
                         sq_tim_enq_t       tim_enq,
                         sq_tim_deq_t       tim_deq)
{
    if (qsize < 2  ||  elem_size < sizeof(sq_eprops_t))
    {
        errno = EINVAL;
        return -1;
    }
    
    bzero(q, sizeof(*q));
    q->magicnumber = SQ_MAGICNUMBER;
    q->elem_size   = elem_size;
    q->timeout_us  = timeout_us;
    q->privptr     = privptr;
    q->sender      = sender;
    q->eq_cmp_func = eq_cmp_func;
    q->tim_enq     = tim_enq;
    q->tim_deq     = tim_deq;

    q->ring_size   = qsize;
    q->ring        = malloc(q->elem_size * q->ring_size);
    if (q->ring == NULL)
    {
        q->magicnumber = 0;
        return -1;
    }

    if (port != NULL) bind_to_port(q, port);

    return 0;
}

void         sq_fini    (sq_q_t *q)
{
    if (q == NULL  ||  q->magicnumber != SQ_MAGICNUMBER) return;
    deq_tout(q, 0);
    if (q->port != NULL) rm_from_port(q);
    if (q->ring) free(q->ring); // "safe_free(q->ring);"
    bzero(q, sizeof(*q));
}

void         sq_clear   (sq_q_t *q)
{
    deq_tout(q, 0);
    q->ring_used = 0;
}

void         sq_pause   (sq_q_t *q, int timeout_us)
{
    enq_tout(q, timeout_us);
}

sq_status_t  sq_enq     (sq_q_t *q, sq_eprops_t *e, sq_enqcond_t how, void *m)
{
  int          r;
  sq_eprops_t *plc;

    /* If it is a direct-send -- then just send and buzz off */
    if (e->tries == SQ_TRIES_DIR)
    {
        r = q->sender(e, q->privptr);
        
        if (e->timeout_us > 0  &&  q->tout_set == 0)
            enq_tout(q, e->timeout_us);

        return r == 0? SQ_NOTFOUND : SQ_ERROR;
    }

    /* Check if we should enqueue it at all */
    if (
        (how == SQ_IF_ABSENT       &&
         sq_foreach(q, SQ_ELEM_IS_EQUAL, e, SQ_FIND_FIRST, NULL) != SQ_NOTFOUND)
        ||
        (how == SQ_IF_NONEORFIRST  &&
         sq_foreach(q, SQ_ELEM_IS_EQUAL, e, SQ_FIND_LAST,  NULL) == SQ_FOUND)
       ) return SQ_FOUND;

    /* A special case: replacement */
    if (how == SQ_REPLACE_NOTFIRST  &&
        sq_foreach(q, SQ_ELEM_IS_EQUAL,
                   m != NULL? m : e,
                   SQ_FIND_LAST,  (void **)&plc) == SQ_FOUND)
    {
        memcpy(plc, e, q->elem_size);
        return SQ_FOUND;
    }

    /* Is there free space? */
    if (q->ring_used >= q->ring_size)
    {
        errno = ENOMEM;
        return SQ_ERROR;
    }

    /* Copy to queue */
    plc = (sq_eprops_t *)
        (
         (int8*)(q->ring) +
         ((q->ring_start + q->ring_used) % q->ring_size) * q->elem_size
        );
    memcpy(plc, e, q->elem_size);
    plc->tries_done = 0;
    q->ring_used++;

    /* May we send right now? */  /*!!!vvv is_pausing==0? */
    /* SCHEDULER */
    if (q->port != NULL)
        perform_sendnext(q, 1);
    else
        if (q->ring_used == 1  &&  q->tout_set == 0) perform_sendnext(q, 1);

    return SQ_NOTFOUND;
}

sq_status_t  sq_foreach (sq_q_t *q,
                         sq_elem_checker_t checker, void *ptr,
                         sq_action_t action,
                         void **found_p)
{
  int   i;
  int   j;
  int   last_found;
  void *plc;
  void *src;

    if (checker == SQ_ELEM_IS_EQUAL) checker = q->eq_cmp_func;

    for (i = 0, last_found = -1;  i < q->ring_used;  i++)
    {
        plc = (int8*)(q->ring) +
              ((q->ring_start + i) % q->ring_size) * q->elem_size;
        if (checker(plc, ptr))
        {
            last_found = i;
            if (action == SQ_FIND_FIRST) return i == 0? SQ_ISFIRST : SQ_FOUND;
            if (action == SQ_FIND_LAST)  goto CONTINUE_SEARCH;
            /* Okay -- that's one of SQ_ERASE_* */

            /* Should we erase at begin of queue? */
            if (i == 0)
            {
                deq_tout(q, 1);
                q->ring_start = (q->ring_start + 1) % q->ring_size;
            }
            /* No, we must do a clever "ringular" squeeze */
            else
            {
                for (j = i;  j < q->ring_used - 1;  j++)
                {
                    plc = (int8*)(q->ring) +
                          ((q->ring_start + j)     % q->ring_size) * q->elem_size;
                    src = (int8*)(q->ring) +
                          ((q->ring_start + j + 1) % q->ring_size) * q->elem_size;
                    memcpy(plc, src, q->elem_size);
                }
            }

            q->ring_used--;
            if (action == SQ_ERASE_FIRST) return i == 0? SQ_ISFIRST : SQ_FOUND;
            i--; // To neutralize loop's "i++" in order to stay at the same position
        }

 CONTINUE_SEARCH:;
    }

    if (last_found >= 0  &&  found_p != NULL)
        *found_p = (int8*)(q->ring) +
                   ((q->ring_start + last_found) % q->ring_size) * q->elem_size;
    
    if (last_found == 0) return SQ_ISFIRST;
    if (last_found >  0) return SQ_FOUND;
    return SQ_NOTFOUND;
}

static void perform_sendnext_in_queue(sq_q_t *q)
{
  sq_eprops_t *epp;
  int          r;
  int          timeout_us;
  sq_try_n_t   try_n;

//fprintf(stderr, "%s(%p)\n", __FUNCTION__, q);
    while (q->ring_used != 0  &&  q->tout_set == 0  &&
           (q->port == NULL  ||  q->port->tout_set == 0))
    {
        epp = (sq_eprops_t *)((int8*)(q->ring) + q->ring_start * q->elem_size);

        /* Try to send the item */
        r = q->sender(epp, q->privptr);

        /* In case of error -- just set timeout for retransmission and break */
        if (r != 0)
        {
            enq_tout(q, q->timeout_us);
            return;
        }

        /* We've successfully sent it, may count this try */
        if (epp->tries_done < SQ_MAX_TRIES) epp->tries_done++;

        /* Should we call the callback? */
        if      (epp->tries_done == 1)          try_n = SQ_TRY_FIRST;
        else if (epp->tries_done == epp->tries) try_n = SQ_TRY_LAST; // Note: this check implies epp->tries>0, so that is omitted
        else                                    try_n = SQ_TRY_NONE;
        
        if (epp->callback != NULL  && try_n != SQ_TRY_NONE)
            epp->callback(q->privptr, epp, try_n);

        /* Is it a oneshot-with-pause? */
        if (epp->tries == SQ_TRIES_ONS  &&  epp->timeout_us > 0)
            enq_tout(q, epp->timeout_us);
        
        /* Should we dequeue this one and continue, ... */
        if (try_n == SQ_TRY_LAST  ||  epp->tries == SQ_TRIES_ONS)
        {
            q->ring_start = (q->ring_start + 1) % q->ring_size;
            q->ring_used--;
        }
        /* ...or should we request a timeout and stop sending for now? */
        else
        {
            timeout_us = q->timeout_us;
            if (epp->timeout_us > 0) timeout_us = epp->timeout_us;
            enq_tout(q, timeout_us);
        }
    }
}

static void perform_sendnext(sq_q_t *q, int from_enq)
{
  sq_port_t *port_p = q->port;

    if (port_p == NULL)
    {
        perform_sendnext_in_queue(q);
    }
    else
    {
        if (from_enq)
        {
            if (q->ring_used == 1) add_to_send(q);
            /* Scheduling-queue is empty? Then set current to q and
               fallthrough to send loop */;
            if (port_p->current_to_send == NULL)
            {
                port_p->current_to_send = q;
            }
            else
                return;
        }
        else if (port_p->current_to_send != q) return;

        /* Loop as long as possible */
        while (port_p->current_to_send           != NULL  &&
               port_p->current_to_send->tout_set == 0     &&
               port_p->tout_set                  == 0)
        {
            // Switch to next (wrap-around is later, AFTER possible dequeueing)
            q = port_p->current_to_send;
            port_p->current_to_send = q->next_to_send;
            // Dequeue if empty
            if (q->ring_used == 0) zer_to_send(q);
            if (port_p->current_to_send == NULL) // Wrap-around
                port_p->current_to_send =  port_p->first_to_send;
            // Nothing left?
            if (port_p->current_to_send == NULL) break;
            // Send
            perform_sendnext_in_queue(port_p->current_to_send);
        }
    }
}

void         sq_sendnext(sq_q_t *q)
{
    perform_sendnext(q, 0);
}

sq_eprops_t *sq_access_e(sq_q_t *q, int n)
{
    if (n >= q->ring_used) return NULL;

    return (sq_eprops_t *)( (int8 *)(q->ring) + q->ring_start * q->elem_size );
}

void         sq_timeout (sq_q_t *q)
{
    /* SCHEDULER */
    q->tout_set = 0;
    perform_sendnext(q, 0);
}

void         sq_port_tout(sq_port_t *port_p)
{
    port_p->tout_set = 0;
    if (port_p->current_to_send == NULL) return; /*!!!???*/
    perform_sendnext(port_p->current_to_send, 0);
}
