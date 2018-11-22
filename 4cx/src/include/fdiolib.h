#ifndef __FDIOLIB_H
#define __FDIOLIB_H


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


#include <sys/types.h>
#include <sys/socket.h>


#define FDIO_OFFSET_OF(struct_type,field_name) \
    ((int) ( ((char *)(&(((struct_type *)NULL)->field_name))) - ((char *)NULL) ))

#define FDIO_SIZEOF(struct_type,field_name) sizeof(((struct_type *)NULL)->field_name)


typedef int fdio_handle_t;

typedef enum
{
    FDIO_UNUSED = 0,  // Value isn't used
    FDIO_CONNECTING,  // Stream socket waiting for connect() to complete
    FDIO_STREAM,      // Connected stream socket
    FDIO_LISTEN,      // Listening stream socket
    FDIO_DGRAM,       // Datagram socket
    FDIO_STRING,      // String socket (connected) -- data is '\n'/'\r'/'\0'-terminated
    FDIO_STRING_CONN, // String socket waiting for connect() to complete
    FDIO_STREAM_PLUG,
    FDIO_STREAM_PLUG_CONN,
} fdio_fdtype_t;

/*
 *  Note: when adding/changing reason codes, the
 *  lib/useful/fdiolib.c::_fdio_rsnlist[] must be updated coherently.
 */
enum
{
    FDIO_R_DATA = 0,
    FDIO_R_CONNECTED,
    FDIO_R_ACCEPT,
    FDIO_R_FIRST_ERROR,
    FDIO_R_CLOSED,
    FDIO_R_CONNERR,
    FDIO_R_IOERR,
    FDIO_R_PROTOERR,
    FDIO_R_INPKT2BIG,
    FDIO_R_ENOMEM,
    FDIO_R_HEADER_PART,
    FDIO_R_BIN_DATA_IN_STRING,
};

enum
{
    FDIO_LEN_LITTLE_ENDIAN = 'l',
    FDIO_LEN_BIG_ENDIAN    = 'b'
};

typedef void (*fdio_ntfr_t)(int uniq, void *privptr1,
                            fdio_handle_t handle, int reason,
                            void *inpkt, size_t inpktsize,
                            void *privptr2);


fdio_handle_t fdio_register_fd(int uniq, void *privptr1,
                               int            fd,         // FD to look after
                               fdio_fdtype_t  fdtype,     // Its type
                               fdio_ntfr_t    notifier,   // Notifier to call upon event
                               void          *privptr2,   // Private pointer to pass to notifier
                               size_t         maxpktsize, // Maximum size of input packets
                               size_t         hdr_size,   // Size of header; must be >= len_size
                               size_t         len_offset, // Offset in header
                               size_t         len_size,   // 1,2,3,4; 0 => fixed size packets
                               char           len_endian, // 'l' or 'b'
                               size_t         len_units,  // 1/2/4/... -- len*=len_units
                               size_t         len_add);   // Packet_length=hdr.len+len_add
int  fdio_deregister          (fdio_handle_t handle);
int  fdio_deregister_flush    (fdio_handle_t handle, int max_wait_secs);
int  fdio_lasterr             (fdio_handle_t handle);

int  fdio_set_maxsbuf         (fdio_handle_t handle, size_t maxoutbufsize);
int  fdio_set_maxpktsize      (fdio_handle_t handle, size_t maxpktsize);
int  fdio_set_len_endian      (fdio_handle_t handle, char   len_endian);
int  fdio_set_maxreadrepcount (fdio_handle_t handle, int    maxrepcount);

int  fdio_advice_hdr_size_add (fdio_handle_t handle, size_t hdr_size_add);
int  fdio_advice_pktlen       (fdio_handle_t handle, size_t pktlen);

int  fdio_string_req_binary   (fdio_handle_t handle, size_t datasize);

int  fdio_fd_of               (fdio_handle_t handle);
int  fdio_accept              (fdio_handle_t handle,
                               struct sockaddr *addr, socklen_t *addrlen);
int  fdio_last_src_addr       (fdio_handle_t handle,
                               struct sockaddr *addr, socklen_t *addrlen);

int  fdio_send                (fdio_handle_t handle, const void *buf, size_t size);
int  fdio_lock_send           (fdio_handle_t handle);
int  fdio_unlock_send         (fdio_handle_t handle);
int  fdio_reply               (fdio_handle_t handle, const void *buf, size_t size);
int  fdio_send_to             (fdio_handle_t handle, const void *buf, size_t size,
                               struct sockaddr *to, socklen_t to_len);

char *fdio_strreason          (int reason);

//////////////////////////////////////////////////////////////////////
typedef int (*fdio_uniq_checker_t)(const char *func_name, int uniq);

void fdio_set_uniq_checker    (fdio_uniq_checker_t checker);
void fdio_do_cleanup          (int uniq);


#ifdef __cplusplus
}
#endif /* __cplusplus */


#endif /* __FDIOLIB_H */
