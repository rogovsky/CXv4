#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>

#include "misc_macros.h"
#include "timeval_utils.h"

#include "cxscheduler.h"


enum {DEFAULT_IDLE_USECS = 10*1000*1000};

enum {MAX_ALLOWED_FD = FD_SETSIZE - 1};


typedef struct
{
    int             is_used;
    sl_tid_t        next;     // Link to next timeout
    sl_tid_t        prev;     // Link to previous timeout
    struct timeval  when;     // When this timeout expires
    sl_tout_proc    cb;       // Function to call
    int             uniq;
    void           *privptr1; // Some private info1
    void           *privptr2; // Some private info2
} trec_t;


enum {TOUT_ALLOC_INC = 10};
static trec_t *tout_list        = NULL;
static int     tout_list_allocd = 0;

static sl_tid_t  frs_tid = -1;
static sl_tid_t  lst_tid = -1;
static sl_tid_t  avl_tid = -1;


static fd_set         used,     rfds,     wfds,     efds;
static int         maxused,  maxrfds,  maxwfds,  maxefds;
static fd_set               sel_rfds, sel_wfds, sel_efds;

GENERIC_FD_DEFINE(Used, used, static);
GENERIC_FD_DEFINE(Rfds, rfds, static);
GENERIC_FD_DEFINE(Wfds, wfds, static);
GENERIC_FD_DEFINE(Efds, efds, static);

static sl_fd_proc  fd_cbs      [MAX_ALLOWED_FD + 1];
static int         fd_uniqs    [MAX_ALLOWED_FD + 1];
static void       *fd_privptr1s[MAX_ALLOWED_FD + 1];
static void       *fd_privptr2s[MAX_ALLOWED_FD + 1];


static int                  select_idle_usecs = DEFAULT_IDLE_USECS;
static sl_at_select_proc    before_select     = NULL;
static sl_at_select_proc    after_select      = NULL;

static sl_on_timeback_proc  on_timeback_proc  = NULL;


static int is_initialized = 0;
static int is_running     = 0;
static int should_break   = 0;

static sl_uniq_checker_t  uniq_checker = NULL;


static void CheckInitialized(void)
{
    if (is_initialized) return;

    ClearUsed();
    ClearRfds();
    ClearWfds();
    ClearEfds();
    
    is_initialized = 1;
}

//////////////////////////////////////////////////////////////////////

static sl_tid_t  GetToutSlot(void)
{
  sl_tid_t  tid;
  trec_t   *p;
  trec_t   *new_list;
  int       x;

    if (avl_tid < 0)
    {
        /* Okay, let's grow the list... */
        new_list = safe_realloc(tout_list,
                                sizeof(trec_t)
                                *
                                (tout_list_allocd + TOUT_ALLOC_INC));
        if (new_list == NULL) return -1;

        /* ...zero-fill newly allocated space... */
        bzero(new_list + tout_list_allocd,
              sizeof(trec_t) * TOUT_ALLOC_INC);
        /* ...initialize it... */
        for (x = tout_list_allocd;  x < tout_list_allocd + TOUT_ALLOC_INC;  x++)
            new_list[x].next = x + 1;
        new_list[tout_list_allocd + TOUT_ALLOC_INC - 1].next = -1;
        avl_tid = tout_list_allocd;
        if (avl_tid == 0) avl_tid++;
        /* ...and record its presence */
        tout_list         = new_list;
        tout_list_allocd += TOUT_ALLOC_INC;
    }
    
    tid = avl_tid;
    p = tout_list + tid;
    avl_tid = p->next;
    p->is_used = 1;

    return tid;
}

static void      RlsToutSlot(sl_tid_t tid)
{
  trec_t *p = tout_list + tid;

    p->is_used = 0;
    p->next = avl_tid;
    avl_tid = tid;
}

//////////////////////////////////////////////////////////////////////

sl_tid_t  sl_enq_tout_at   (int uniq, void *privptr1,
                            struct timeval *when,
                            sl_tout_proc cb, void *privptr2)
{
  sl_tid_t  tid;
  trec_t   *p;
  sl_tid_t  prev;
  sl_tid_t  next;

    if (uniq_checker != NULL  &&  uniq_checker(__FUNCTION__, uniq)) return -1;

    /*  */
    tid = GetToutSlot();
    if (tid < 0) return -1;
    p = tout_list + tid;

    p->when     = *when;
    p->cb       = cb;
    p->uniq     = uniq;
    p->privptr1 = privptr1;
    p->privptr2 = privptr2;

    /* Find a place in the queue */
    for (next = frs_tid;
         next >= 0  &&  TV_IS_AFTER(*when, tout_list[next].when);
         next = tout_list[next].next);

    if (next >= 0)  prev = tout_list[next].prev; else prev = lst_tid;

    /* Insert this timeout between prev and next */
    p->prev = prev;
    if (prev >= 0) tout_list[prev].next = tid; else frs_tid = tid;
    p->next = next;
    if (next >= 0) tout_list[next].prev = tid; else lst_tid = tid;

    return tid;
}

sl_tid_t  sl_enq_tout_after(int uniq, void *privptr1,
                            int             usecs,
                            sl_tout_proc cb, void *privptr2)
{
  struct timeval  when;
    
    if (uniq_checker != NULL  &&  uniq_checker(__FUNCTION__, uniq)) return -1;

    gettimeofday(&when, NULL);
    timeval_add_usecs(&when, &when, usecs);
    return sl_enq_tout_at(uniq, privptr1, &when, cb, privptr2);
}

int       sl_deq_tout      (sl_tid_t        tid)
{
  trec_t   *p = tout_list + tid;
  sl_tid_t  prev;
  sl_tid_t  next;
    
    /* Check if it is used */
    if (tid < 0  ||  tid >= tout_list_allocd  ||  p->is_used == 0)
    {
        errno = EINVAL;
        return -1;
    }
    
    prev = p->prev;
    next = p->next;

    if (prev < 0) frs_tid = next; else tout_list[prev].next = next;
    if (next < 0) lst_tid = prev; else tout_list[next].prev = prev;

    RlsToutSlot(tid);

    return 0;
}


sl_fdh_t  sl_add_fd     (int uniq, void *privptr1,
                         int fd, int mask, sl_fd_proc cb, void *privptr2)
{
    CheckInitialized();

    if (uniq_checker != NULL  &&  uniq_checker(__FUNCTION__, uniq)) return -1;

    if (fd < 0  ||  fd > MAX_ALLOWED_FD)
    {
        errno = EBADF;
        return -1;
    }

    if (FD_ISSET(fd, &used))
    {
        errno = EINVAL;
        return -1;
    }

    AddUsed(fd);
    sl_set_fd_mask(fd, mask);

    fd_cbs      [fd] = cb;
    fd_uniqs    [fd] = uniq;
    fd_privptr1s[fd] = privptr1;
    fd_privptr2s[fd] = privptr2;
    
    return fd;
}

int       sl_del_fd     (sl_fdh_t fdh)
{
  int  fd = fdh;
    
    CheckInitialized();
    
    if (fd < 0  ||  fd > MAX_ALLOWED_FD  ||  !FD_ISSET(fd, &used))
    {
        errno = EBADF;
        return -1;
    }

    sl_set_fd_mask(fdh, 0);
    FD_CLR(fd, &sel_rfds);
    FD_CLR(fd, &sel_wfds);
    FD_CLR(fd, &sel_efds);
    RemoveUsed(fd);
    fd_uniqs[fd] = 0; //!!! Note: we employ "==0" as "unused".

    return 0;
}

int       sl_set_fd_mask(sl_fdh_t fdh, int mask)
{
  int  fd = fdh;
    
    CheckInitialized();
    
    if (fd < 0  ||  fd > MAX_ALLOWED_FD  ||  !FD_ISSET(fd, &used))
    {
        errno = EBADF;
        return -1;
    }

    if (mask & SL_RD) AddRfds(fd); else RemoveRfds(fd);
    if (mask & SL_WR) AddWfds(fd); else RemoveWfds(fd);
    if (mask & SL_EX) AddEfds(fd); else RemoveEfds(fd);

    /*!!! Shouldn't we clear bits in sel_NNN? */
    
    return 0;
}


int  sl_main_loop(void)
{
  int             ret = 0;

  int             r;            /* Result of system calls */
  int             maxall;

  struct timeval  before;       /* "Previous now" */
  struct timeval  now;          /* The time "now" for diff calculation */
  struct timeval  timeout;      /* Timeout for select() */
  int             saved_errno;

  sl_tid_t        tid;
  sl_tout_proc    cb;
  int             uniq;
  void           *privptr1;
  void           *privptr2;

  int             fd;
  int             mask;

    CheckInitialized();

    bzero(&now, sizeof(now));

    for (should_break = 0, is_running = 1;  !should_break;  )
    {
        before = now;
        gettimeofday(&now, NULL);
        if (TV_IS_AFTER(before, now)  &&  on_timeback_proc != NULL)
            on_timeback_proc();
        /* Walk through timeouts */
        /* Note: here we rely on an assumption that any subsequently
           added timeouts will be TV_IS_AFTER(subsequent,now), so that
           this loop will process not more than what exists at its
           beginning, and thus would be finite. */
        while (frs_tid >= 0  &&  TV_IS_AFTER(now, tout_list[frs_tid].when))
        {
            tid      = frs_tid;
            cb       = tout_list[tid].cb;
            uniq     = tout_list[tid].uniq;
            privptr1 = tout_list[tid].privptr1;
            privptr2 = tout_list[tid].privptr2;
            sl_deq_tout(tid);

            cb(uniq, privptr1, tid, privptr2);
        }

        /* For timeouts to be able to break the loop too */
        if (should_break) break;

        /* Prepare descriptor sets info... */
        sel_rfds = rfds;
        sel_wfds = wfds;
        sel_efds = efds;
        maxall = maxrfds;
        if (maxwfds > maxall) maxall = maxwfds;
        if (maxefds > maxall) maxall = maxefds;

        /* ...find out how much should we sleep... */
        gettimeofday(&now, NULL);
        /* If there's nothing to happen in a predictable period of time,
           we just sleep for an arbitrary time; ideally it must be NULL. */
        if      (frs_tid < 0)
        {
            timeout.tv_sec  = select_idle_usecs / 1000000;
            timeout.tv_usec = select_idle_usecs % 1000000;
        }
        /* Note: even if some other timeout had arrived while we were
           processing previous timeouts, we *must* poll descriptors
           anyway, to give them a chance and to prevent infinite loop
           on timeouts.  So, in such a case we force a poll. */
        else if (timeval_subtract(&timeout, &(tout_list[frs_tid].when), &now) != 0)
            timeout.tv_sec = timeout.tv_usec = 0;

        /* ...and do a select() */
        if (before_select != NULL) before_select();
        r = select(maxall + 1, &sel_rfds, &sel_wfds, &sel_efds, &timeout);
        saved_errno = errno;
        if (after_select  != NULL) after_select ();
        errno = saved_errno;

        if (r < 0)
        {
            if (!SHOULD_RESTART_SYSCALL())
            {
                ret = -1;
                goto END_LOOP;
            }
        }
        else if (r != 0)
        {
            for (fd = 0;  fd <= maxall;  fd++)
                if (FD_ISSET(fd, &sel_rfds)  ||
                    FD_ISSET(fd, &sel_wfds)  ||
                    FD_ISSET(fd, &sel_efds))
                {
                    mask = 0;
                    if (FD_ISSET(fd, &sel_rfds)) mask |= SL_RD;
                    if (FD_ISSET(fd, &sel_wfds)) mask |= SL_WR;
                    if (FD_ISSET(fd, &sel_efds)) mask |= SL_EX;

                    fd_cbs[fd](fd_uniqs[fd], fd_privptr1s[fd],
                               fd, fd, mask, fd_privptr2s[fd]);
                }
        }
    }

 END_LOOP:
    is_running = 0;

    return ret;
}

int  sl_break    (void)
{
    if (!is_running) return -1;

    should_break = 1;
    
    return 0;
}


int  sl_set_select_behaviour(sl_at_select_proc  before,
                             sl_at_select_proc  after,
                             int                usecs_idle)
{
    if (usecs_idle <= 0) usecs_idle = DEFAULT_IDLE_USECS;

    select_idle_usecs = usecs_idle;
    before_select     = before;
    after_select      = after;

    return 0;
}

int  sl_set_on_timeback_proc(sl_on_timeback_proc proc)
{
    on_timeback_proc = proc;

    return 0;
}

void sl_set_uniq_checker(sl_uniq_checker_t checker)
{
    uniq_checker = checker;
}

void sl_do_cleanup(int uniq)
{
  sl_tid_t  tid;
  sl_tid_t  next;
  trec_t   *p;
  int       fd;

    if (uniq == 0) return;

    /* 1. Timeouts */
    for (tid = frs_tid;  tid > 0;  )
    {
        p    = tout_list + tid;
        next = p->next;
        if (p->uniq == uniq)
            sl_deq_tout(tid);
        tid = next;
    }

    /* 2. File descriptors */
    for (fd = 0;  fd <= maxused;  fd++)
        if (FD_ISSET(fd, &used)  &&  fd_uniqs[fd] == uniq)
        {
            sl_del_fd(fd);
            close(fd);
        }
}
