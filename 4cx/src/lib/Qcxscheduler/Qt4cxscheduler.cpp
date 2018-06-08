#include "Qt4cxscheduler.h"

/* ============================================================================= */

QSGNSignalManager g_sgn;

#include "misc_macros.h"
#include "timeval_utils.h"

#include "cxscheduler.h"

typedef struct
{
    int             is_used;
    sl_tid_t        next;     // Link to next timeout
    QSGNIntervalId  timer;
    sl_tout_proc    cb;       // Function to call
    int             uniq;
    void           *privptr1; // Some private info
    void           *privptr2; // Some private info
} trec_t;


enum {TOUT_ALLOC_INC = 10};
static trec_t *tout_list        = NULL;
static int     tout_list_allocd = 0;

static sl_tid_t  avl_tid = -1;

static sl_uniq_checker_t  uniq_checker = NULL;


enum {FD_UNUSED = -1};

typedef struct
{
    int         fd;
    sl_fd_proc  cb;
    int         uniq;
    void       *privptr1;
    void       *privptr2;
    int         curmask;
    QSGNInputId rd_id;
    QSGNInputId wr_id;
    QSGNInputId ex_id;
} fdrec_t;

static fdrec_t *fddata        = NULL;
static int      fddata_allocd = 0;

static int FindFD(int fd)
{
  int  n;

    for (n = 0;  n < fddata_allocd;  n++)
        if (fddata[n].fd == fd) return n;

    return -1;
}

static int GrowFddata(void)
{
  enum    {GROW_INCREMENT = 10};
  fdrec_t *new_fddata;
  int      n;

    new_fddata = (fdrec_t *)safe_realloc(fddata, sizeof(*fddata) * (fddata_allocd + GROW_INCREMENT));
    if (new_fddata == NULL) return -1;
    fddata = new_fddata;

    for (n = 0;  n < GROW_INCREMENT;  n++)
        fddata[fddata_allocd + n].fd = FD_UNUSED;
    fddata_allocd += GROW_INCREMENT;

    return 0;
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
        new_list = (trec_t *)safe_realloc(tout_list,
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

static void TimeoutProc(QSGNPointer closure, QSGNIntervalId)
{
  int           tid      = ptr2lint(closure);
  trec_t       *p        = tout_list + tid;
  sl_tout_proc  cb       = p->cb;
  int           uniq     = p->uniq;
  void         *privptr1 = p->privptr1;
  void         *privptr2 = p->privptr2;

    sl_deq_tout(tid);
    cb(uniq, privptr1, tid, privptr2);
}

sl_tid_t  sl_enq_tout_at   (int uniq, void *privptr1,
                            struct timeval *when,
                            sl_tout_proc cb, void *privptr2)
{
  sl_tid_t  tid;
  trec_t   *p;

  struct timeval  timenow;
  struct timeval  timediff;
  unsigned long   interval;

    if (uniq_checker != NULL  &&  uniq_checker(__FUNCTION__, uniq)) return -1;

    /*  */
    tid = GetToutSlot();
    if (tid < 0) return -1;
    p = tout_list + tid;

    p->cb       = cb;
    p->uniq     = uniq;
    p->privptr1 = privptr1;
    p->privptr2 = privptr2;

    gettimeofday(&timenow, NULL);
    if (timeval_subtract(&timediff, when, &timenow))
        interval = 1;
    else
        interval = timediff.tv_sec * 1000 + timediff.tv_usec / 1000;

//    p->timer = XtAppAddTimeOut(xh_context, interval, TimeoutProc, (XtPointer)tid);
    p->timer = (QSGNIntervalId)(g_sgn.registerTimeOut (interval, TimeoutProc, (QSGNPointer)tid));

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

    /* Check if it is used */
    if (tid < 0  ||  tid >= tout_list_allocd  ||  p->is_used == 0)
    {
        errno = EINVAL;
        return -1;
    }

//    XtRemoveTimeOut(p->timer);
    g_sgn.removeTimeOut((QSGNTimeOut*)(p->timer));

    RlsToutSlot(tid);

    return 0;
}

sl_fdh_t  sl_add_fd     (int uniq, void *privptr1,
                         int fd, int mask, sl_fd_proc cb, void *privptr2)
{
  int  fdh;

    if (uniq_checker != NULL  &&  uniq_checker(__FUNCTION__, uniq)) return -1;

    if (fd < 0)
    {
        errno = EBADF;
        return -1;
    }

    fdh = FindFD(fd);
    if (fdh >= 0)
    {
        errno = EINVAL;
        return -1;
    }

    fdh = FindFD(FD_UNUSED);
    if (fdh < 0)
    {
        if (GrowFddata() < 0  ||  (fdh = FindFD(FD_UNUSED)) < 0)
        {
            errno = ENOMEM;
            return -1;
        }
    }

    fddata[fdh].fd       = fd;
    fddata[fdh].cb       = cb;
    fddata[fdh].uniq     = uniq;
    fddata[fdh].privptr1 = privptr1;
    fddata[fdh].privptr2 = privptr2;
    fddata[fdh].curmask  = 0;
    sl_set_fd_mask(fdh, mask);

    return fdh;
}

int       sl_del_fd     (sl_fdh_t fdh)
{
    if (fdh < 0  ||  fdh >= fddata_allocd  ||
        fddata[fdh].fd == FD_UNUSED)
    {
        errno = EINVAL;
        return -1;
    }

    sl_set_fd_mask(fdh, 0);
    fddata[fdh].fd = FD_UNUSED;

    return 0;
}

//static void HandleRD(XtPointer     closure,
//                     int          *source  __attribute__((unused)),
//                     XtInputId    *id      __attribute__((unused)))
static void HandleRD(QSGNPointer closure, int, QSGNInputId)
{
  int  fdh = ptr2lint(closure);

    fddata[fdh].cb(fddata[fdh].uniq, fddata[fdh].privptr2,
                   fdh, fddata[fdh].fd, SL_RD, fddata[fdh].privptr2);
}

//static void HandleWR(XtPointer     closure,
//                     int          *source  __attribute__((unused)),
//                     XtInputId    *id      __attribute__((unused)))
static void HandleWR(QSGNPointer closure, int, QSGNInputId)
{
  int  fdh = ptr2lint(closure);

    fddata[fdh].cb(fddata[fdh].uniq, fddata[fdh].privptr2,
                   fdh, fddata[fdh].fd, SL_WR, fddata[fdh].privptr2);
}

//static void HandleEX(XtPointer     closure,
//                     int          *source  __attribute__((unused)),
//                     XtInputId    *id      __attribute__((unused)))
static void HandleEX(QSGNPointer closure, int, QSGNInputId)
{
  int  fdh = ptr2lint(closure);

    fddata[fdh].cb(fddata[fdh].uniq, fddata[fdh].privptr2,
                   fdh, fddata[fdh].fd, SL_EX, fddata[fdh].privptr2);
}

int       sl_set_fd_mask(sl_fdh_t fdh, int mask)
{
    if (fdh < 0  ||  fdh >= fddata_allocd  ||
        fddata[fdh].fd == FD_UNUSED)
    {
        errno = EINVAL;
        return -1;
    }

    if (((mask ^ fddata[fdh].curmask) & SL_RD) != 0)
    {
        if (mask & SL_RD)
//            fddata[fdh].rd_id = XtAppAddInput(xh_context, fddata[fdh].fd,
//                                              (XtPointer)XtInputReadMask,
//                                              HandleRD, (XtPointer)fdh);
            fddata[fdh].rd_id = (QSGNInputId)g_sgn.registerSocket (fddata[fdh].fd,
                                                                   QSGN_SOCKET_READ,
                                                                   HandleRD, (QSGNPointer)fdh);
        else
//            XtRemoveInput(fddata[fdh].rd_id);
            g_sgn.removeSocket((QSGNSocket*)(fddata[fdh].rd_id));
    }

    if (((mask ^ fddata[fdh].curmask) & SL_WR) != 0)
    {
        if (mask & SL_WR)
//            fddata[fdh].wr_id = XtAppAddInput(xh_context, fddata[fdh].fd,
//                                              (XtPointer)
//                                                  (XtInputWriteMask |
//                                                   (
//                                                    (mask & SL_CE)? XtInputReadMask | XtInputExceptMask
//                                                                  : 0
//                                                   )
//                                                  ),
//                                              HandleWR, (XtPointer)fdh);
            fddata[fdh].wr_id = (QSGNInputId)g_sgn.registerSocket (fddata[fdh].fd,
                                                                   QSGN_SOCKET_WRITE,
                                                                   HandleWR, (QSGNPointer)fdh);
        else
//            XtRemoveInput(fddata[fdh].wr_id);
            g_sgn.removeSocket((QSGNSocket*)(fddata[fdh].wr_id));
    }

    if (((mask ^ fddata[fdh].curmask) & SL_EX) != 0)
    {
        if (mask & SL_EX)
//            fddata[fdh].ex_id = XtAppAddInput(xh_context, fddata[fdh].fd,
//                                              (XtPointer)XtInputExceptMask,
//                                              HandleEX, (XtPointer)fdh);
            fddata[fdh].ex_id = (QSGNInputId)g_sgn.registerSocket (fddata[fdh].fd,
                                                                   QSGN_SOCKET_EXCEPTION,
                                                                   HandleEX, (QSGNPointer)fdh);
        else
//            XtRemoveInput(fddata[fdh].ex_id);
            g_sgn.removeSocket((QSGNSocket*)(fddata[fdh].ex_id));
    }

    fddata[fdh].curmask = mask;

    return 0;
}

/*
 *  Note:
 *    sl_main_loop() and sl_break() are intentionally left out,
 *    since these calls can NOT be implemented in Xt environment
 *    (and Xh_cxscheduler isn't intended for "main" programs, but
 *    for cxscheduler-dependent libraries only).
 */

#if 0

int  sl_main_loop(void)
{
}

int  sl_break    (void)
{
}

#endif


void sl_set_uniq_checker(sl_uniq_checker_t checker)
{
    uniq_checker = checker;
}

void sl_do_cleanup(int uniq)
{
  sl_tid_t  tid;
  int       fdh;

    /* 1. Timeouts */
    for (tid = 0;  tid < tout_list_allocd;  tid++)
    {
        if (tout_list[tid].is_used  &&  tout_list[tid].uniq == uniq)
            sl_deq_tout(tid);
    }


    /* 2. File descriptors */
    for (fdh = 0;  fdh < fddata_allocd;  fdh++)
        if (fddata[fdh].fd != FD_UNUSED  &&  fddata[fdh].uniq == uniq)
        {
            close(fddata[fdh].fd);
            sl_del_fd(fdh);
        }
}

/* ============================================================================= */

static QSocketNotifier::Type getSocketNotifierType(QSGNSocketType iType)
{
    switch (iType)
    {
    case QSGN_SOCKET_READ:
        return QSocketNotifier::Read;

    case QSGN_SOCKET_WRITE:
        return QSocketNotifier::Write;

    case QSGN_SOCKET_EXCEPTION:
    default:
        return QSocketNotifier::Exception;
    }
}

QSGNSocket::QSGNSocket (int iSocket,
                        QSGNSocketType iType,
                        QSGNSocketCallbackProc iProc,
                        QSGNPointer iData)
    : mProc (iProc), mData (iData),
      mNotifier(iSocket, getSocketNotifierType(iType), this)
{
    mNotifier.setEnabled(true);
    connect (&mNotifier, SIGNAL (activated(int)), this, SLOT (triggered(int)));
}

void QSGNSocket::callProc (int iSource) const
{
    mProc (mData, iSource, (QSGNInputId)this);
}

void QSGNSocket::removeSocket()
{
    mNotifier.setEnabled(false);
}

void QSGNSocket::triggered (int iSource)
{
    callProc (iSource);
}

bool QSGNSocket::isEnabled() const
{
    return mNotifier.isEnabled();
}

/* ============================================================================= */

QSGNTimeOut::QSGNTimeOut (unsigned int iInterval,
                          QSGNTimerCallbackProc iProc,
                          QSGNPointer iData)
    : QObject (), mProc (iProc), mData (iData), mIsActive(true)

{
    mLocalTimer.start (iInterval, this);
}

void QSGNTimeOut::callProc() const
{
    mProc (mData, (QSGNIntervalId)this);
}

void QSGNTimeOut::removeTimeOut()
{
    mIsActive = false;
    mLocalTimer.stop ();
}

void QSGNTimeOut::timerEvent(QTimerEvent *event)
{
    if (event->timerId () == mLocalTimer.timerId ())
    {
        mLocalTimer.stop ();
        timeOut ();
    }
    else
    {
        QObject::timerEvent(event);
    }
}

void QSGNTimeOut::timeOut()
{
    callProc();
    removeTimeOut();  // timeout triggers only one time
}

/* ============================================================================= */

QSGNSignalManager::QSGNSignalManager()
{
}

QSGNTimeOut* QSGNSignalManager::registerTimeOut (unsigned long iInterval,
                                                 QSGNTimerCallbackProc iProc,
                                                 QSGNPointer iData)
{
    try
    {
        QSGNTimeOut* timeout = new QSGNTimeOut (iInterval, iProc, iData);
        mTimeOuts.append (timeout);
        return timeout;
    }
    catch (std::bad_alloc)
    {
        return 0;
    }
}

QSGNSocket* QSGNSignalManager::registerSocket (int iSocket,
                                               QSGNSocketType iType,
                                               QSGNSocketCallbackProc iProc,
                                               QSGNPointer iData)
{
    try
    {
        QSGNSocket* socket = new QSGNSocket (iSocket, iType, iProc, iData);
        mSockets.append (socket);
        return socket;
    }
    catch (std::bad_alloc)
    {
        return 0;
    }
}

void QSGNSignalManager::removeTimeOut (QSGNTimeOut* iTimeOut)
{
    if (!iTimeOut)
        return;

    iTimeOut->removeTimeOut();      // remove internal structures
    mTimeOuts.removeOne (iTimeOut); // remove from list
}

void QSGNSignalManager::removeSocket (QSGNSocket* iSocket)
{
    if (!iSocket)
        return;

    iSocket->removeSocket();       // remove internal structures
    mSockets.removeOne (iSocket);  // remove from list
}

/* ============================================================================= */
