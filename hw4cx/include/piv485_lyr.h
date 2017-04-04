#ifndef __PIV485_LYR_H
#define __PIV485_LYR_H


#include "sendqlib.h"


#define PIV485_LYR_NAME "piv485"
enum
{
    PIV485_LYR_VERSION_MAJOR = 2,
    PIV485_LYR_VERSION_MINOR = 0,
    PIV485_LYR_VERSION = CX_ENCODE_VERSION(PIV485_LYR_VERSION_MAJOR,
                                           PIV485_LYR_VERSION_MINOR)
};


/* Driver-provided callbacks */

typedef void (*Piv485PktinProc) (int devid, void *devptr,
                                 int cmd, int dlc, uint8 *data);
typedef void (*Piv485OnSendProc)(int devid, void *devptr,
                                 sq_try_n_t try_n, void *privptr);
typedef void (*Piv485SndMdProc) (int devid, void *devptr,
                                 int *dlc_p, uint8 *data);
typedef int  (*Piv485PktCmpFunc)(int dlc, uint8 *data, void *privptr);

/* Layer API for drivers */

typedef int  (*Piv485AddDevice)(int devid, void *devptr,
                                int businfocount, int businfo[],
                                Piv485PktinProc inproc,
                                Piv485SndMdProc mdproc,
                                int queue_size);

typedef void        (*Piv485QClear)     (int handle);

typedef sq_status_t (*Piv485QEnqTotal)  (int handle, sq_enqcond_t how,
                                         int tries, int timeout_us,
                                         Piv485OnSendProc on_send, void *privptr,
                                         int model_dlc, int dlc, uint8 *data);
typedef sq_status_t (*Piv485QEnqTotal_v)(int handle, sq_enqcond_t how,
                                         int tries, int timeout_us,
                                         Piv485OnSendProc on_send, void *privptr,
                                         int model_dlc, int dlc, ...);
typedef sq_status_t (*Piv485QEnq)       (int handle, sq_enqcond_t how,
                                         int dlc, uint8 *data);
typedef sq_status_t (*Piv485QEnq_v)     (int handle, sq_enqcond_t how,
                                         int dlc, ...);

typedef sq_status_t (*Piv485QErAndSNx)  (int handle,
                                         int dlc, uint8 *data);
typedef sq_status_t (*Piv485QErAndSNx_v)(int handle,
                                         int dlc, ...);

typedef sq_status_t (*Piv485Foreach)    (int handle,
                                         Piv485PktCmpFunc do_cmp, void *privptr,
                                         sq_action_t action);

/* Piv485 interface itself */

typedef struct
{
    /* General device management */
    Piv485AddDevice            add;

    /* Queue management */
    Piv485QClear               q_clear;

    Piv485QEnqTotal            q_enqueue;
    Piv485QEnqTotal_v          q_enqueue_v;
    Piv485QEnq                 q_enq;
    Piv485QEnq_v               q_enq_v;

    Piv485QErAndSNx            q_erase_and_send_next;
    Piv485QErAndSNx_v          q_erase_and_send_next_v;

    Piv485Foreach              q_foreach;
} piv485_vmt_t;


#endif /* __PIV485_LYR_H */
