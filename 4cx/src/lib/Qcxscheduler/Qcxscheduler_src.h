#ifndef _QCX_SCHEDULER
#define _QCX_SCHEDULER

enum QSGNSocketType
{
    QSGN_SOCKET_READ,
    QSGN_SOCKET_WRITE,
    QSGN_SOCKET_EXCEPTION
};

typedef void*         QSGNContext;
typedef unsigned long QSGNIntervalId;
typedef unsigned long QSGNInputId;

typedef void*         QSGNPointer;
typedef void          (*QSGNTimerCallbackProc)  (QSGNPointer, QSGNIntervalId);
typedef void          (*QSGNSocketCallbackProc) (QSGNPointer, int, QSGNInputId);

/* ============================================================================= */

#include <qsocketnotifier.h>

class QSGNSocket : public QObject
{
    Q_OBJECT

public:
    QSGNSocket (int iSocket, QSGNSocketType iType, QSGNSocketCallbackProc iProc, QSGNPointer iData);
    ~QSGNSocket() {}

    void callProc (int iSource) const;
    void removeSocket();

    bool isEnabled() const;

private:
    QSGNSocketCallbackProc mProc;   // user callback
    QSGNPointer mData;              // user data

    QSocketNotifier mNotifier;

private slots:
    void triggered (int iSource);

private:
    QSGNSocket (const QSGNSocket&);
    QSGNSocket& operator= (const QSGNSocket&);
};

/* ============================================================================= */

#include <qobject.h>
#include <qbasictimer.h>
#include <qcoreevent.h>

class QSGNTimeOut : public QObject
{
    Q_OBJECT

public:
    QSGNTimeOut (unsigned int iInterval, QSGNTimerCallbackProc iProc, QSGNPointer iData);
    ~QSGNTimeOut() {}

    void callProc() const;
    void removeTimeOut();

    inline bool isActive() const { return mIsActive; }

protected:
    void timerEvent(QTimerEvent *event);

private:
    void timeOut();

private:
    QSGNTimerCallbackProc mProc;    // user callback
    QSGNPointer mData;              // user data

    bool mIsActive;                 // active timeout or not: non-active timeouts were already triggered.
    QBasicTimer mLocalTimer;        // lightweight timer for single triggering

private:
    QSGNTimeOut (const QSGNTimeOut&);
    QSGNTimeOut& operator= (const QSGNTimeOut&);
};

/* ============================================================================= */

#include <qlinkedlist.h>
#define QSGNList QLinkedList

class QSGNSignalManager
{
public:
    QSGNSignalManager();
    virtual ~QSGNSignalManager() {}

    QSGNTimeOut* registerTimeOut (unsigned long iInterval,
                                  QSGNTimerCallbackProc iProc, QSGNPointer iData);

    QSGNSocket* registerSocket (int iSocket, QSGNSocketType iType, QSGNSocketCallbackProc iProc, QSGNPointer iData);

    void removeTimeOut (QSGNTimeOut* iTimeOut);
    void removeSocket (QSGNSocket* iSocket);

private:
    QSGNList <QSGNTimeOut*> mTimeOuts;  // list of all registered timeouts
    QSGNList <QSGNSocket*>  mSockets;   // list of all registered sockets

private:
    QSGNSignalManager (const QSGNSignalManager&);
    QSGNSignalManager& operator= (const QSGNSignalManager&);
};

#endif // _QCX_SCHEDULER
