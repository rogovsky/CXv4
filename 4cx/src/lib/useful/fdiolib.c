#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
/*!!!*/#include <netinet/tcp.h>

#include "cx_sysdeps.h"
#include "misc_macros.h"
#include "misclib.h"
#include "cxscheduler.h"

#include "fdiolib.h"


/* For some strange reason these GNU variables aren't defined in any .h
   unless you are using glibc... */
#if OPTION_HAS_PROGRAM_INVOCATION_NAME
  #define progname program_invocation_short_name
#else
  const char progname[] = "";
#endif /* OPTION_HAS_PROGRAM_INVOCATION_NAME */

static void reporterror(const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));

static void reporterror(const char *format, ...)
{
  va_list ap;

    va_start(ap, format);
#if 1
    fprintf (stderr, "%s %s%sfdiolib: ",
             strcurtime(), progname, progname[0] != '\0' ? ": " : "");
    vfprintf(stderr, format, ap);
#endif
    va_end(ap);
}


/*
  Note: for DGRAM connections a special layout of _sysbuf is used --
        each datagram is prefixed with dgraminfo_t header.
 */

typedef struct
{
    size_t           size;
    struct sockaddr  to;
    socklen_t        to_len;
    uint8            data[0];
} dgraminfo_t;

typedef struct
{
    int            fd;             // ==fd; '-1' denotes a free cell
    sl_fdh_t       fdhandle;       // Its handle in cxscheduler
    fdio_fdtype_t  fdtype;         // Its type
    
    /* Callback info */
    fdio_ntfr_t    notifier;       // Notifier to call upon event
    int            uniq;
    void          *privptr1;       // Private pointer to pass to notifier
    void          *privptr2;       // Private pointer to pass to notifier
    
    /* Protocol properties */
    size_t         maxinppktsize;  // Max size of input packet
    size_t         hdr_size;       // Size of fixed, always-present header
    size_t         len_offset;     // Offset of the 'length' field in header
    size_t         len_size;       // 1,2,3,4; 0 => fixed length packets
    char           len_endian;     // little- or big-endian 'length' field
    size_t         len_units;      //  - packet_length = len*len_units
    size_t         len_add;        //  -               + len_add

    size_t         maxoutbufsize;  // Maximum allowed size of output buffer (_sysbuf)

    int            maxrepcount;    // Maximum # of consecutive read() attempts upon SL_RD-readiness
    
    /* Data buffers */
    uint8         *_sysbuf;        // Background send buffer
    size_t         _sysbufsize;    // Its size
    size_t         _sysbufused;    // Amount of data in it (this does NOT include _sysbufoffset!)
    size_t         _sysbufoffset;  // Amount of already freed space at its beginning (for move-to-bottom-on-demand)
    int            send_locked;    // Lock count (!=0 -- locked)
    
    uint8         *reqbuf;         // Client's request buffer
    size_t         reqbufsize;     // Its size

    /* Current state */
    int            reqreadstate;   // =0 -- reading header, else -- data
    size_t         reqreadsize;    // Bytes currently in reqbuf
    size_t         thisreqpktsize; // Size of the packet currently being read

    struct sockaddr  last_addr;
    socklen_t        last_addr_len;

    int            last_err;       // Last errno
    int            is_defunct;     // !=0 => can only be closed and queried
    int            being_processed;
    int            being_destroyed;

    // For "advice" quasi-plugins API
    int            is_asking_plugin;
    size_t         hdr_size_add;
    size_t         plugin_pktlen;
} fdinfo_t;

enum {FDIO_MAX_SUPPORTED_FD = FD_SETSIZE - 1};
enum {WATCHED_ALLOC_INCREMENT = 10}; // Must be >1 (to provide growth from 0 to 2)

static fdinfo_t *watched        = NULL;
static int       watched_allocd = 0;

static fdio_uniq_checker_t  uniq_checker = NULL;


#define DECODE_AND_CHECK(errret)                           \
    do {                                                   \
        fr = watched + handle;                             \
        if (handle <= 0  ||  handle >= watched_allocd  ||  \
            fr->fd < 0  ||  fr->being_destroyed)           \
        {                                                  \
            errno = EINVAL;                                \
            return errret;                                 \
        }                                                  \
        if (fr->is_defunct)                                \
        {                                                  \
            errno = EBADF;                                 \
            return errret;                                 \
        }                                                  \
    } while (0)

/*
 *  The same as above, but allows defunct records -- for close() and lasterr()
 */

#define DECODE_AND_LAZYCHECK(errret)                       \
    do {                                                   \
        fr = watched + handle;                             \
        if (handle <= 0  ||  handle >= watched_allocd  ||  \
            fr->fd < 0  ||  fr->being_destroyed)           \
        {                                                  \
            errno = EINVAL;                                \
            return errret;                                 \
        }                                                  \
    } while (0)

static inline int fr2handle(fdinfo_t *fr)
{
    return fr - watched;
}

static void close_because(fdinfo_t *fr, int reason)
{
    fr->last_err   = errno;
    fr->is_defunct = 1;
    sl_set_fd_mask(fr->fdhandle, 0);
    fr->notifier(fr->uniq, fr->privptr1,
                 fr2handle(fr), reason, NULL, 0, fr->privptr2);
}

static int IsReadError(fdinfo_t *fr, int r)
{
    if (r > 0) return 0;
    
    if (r == 0)
    {
        errno = 0;
        close_because(fr, FDIO_R_CLOSED);
    }
    else /* r < 0 */
    {
        if (!ERRNO_WOULDBLOCK()  &&  !SHOULD_RESTART_SYSCALL())
            close_because(fr, FDIO_R_IOERR);
        return -1;
    }
    
    return 1;
}


static int StreamReadyForWrite(fdinfo_t *fr)
{
  int  r;
    
    /* Just a safety net for (hope) an impossible case... */
    if (fr->_sysbufused == 0)
    {
        reporterror("%s(handle=%d,fd=%d): STREAM-READY-FOR_WRITE, but _sysbufused==0!\n",
                    __FUNCTION__, fr2handle(fr), fr->fd);
        sl_set_fd_mask(fr->fdhandle, SL_RD);
        return 0;
    }
    
    errno = 0;
    r = uintr_write(fr->fd, fr->_sysbuf + fr->_sysbufoffset, fr->_sysbufused);
    if (r < 0)
    {
        if (!ERRNO_WOULDBLOCK()  &&  !SHOULD_RESTART_SYSCALL())
        {
            close_because(fr, FDIO_R_IOERR);
            return -1;
        }
    }
    else
    {
        fr->_sysbufoffset += r;
        fr->_sysbufused   -= r;
        
        /* Have we completely flushed the buffer? */
        if (fr->_sysbufused == 0)
        {
            sl_set_fd_mask(fr->fdhandle, SL_RD);
            fr->_sysbufoffset = 0;
        }
    }

    return 0;
}

static int StreamReadyForRead (fdinfo_t *fr)
{
  int     handle = fr2handle(fr);

  int     r;
  int     count;
  int     errs;
  
  size_t  pktlen;

  int     repcount = 0;

 REPEAT:
    /* Is it still a header? */
    if (fr->reqreadstate == 0)
    {
        /* Try to read the whole header */
        count = fr->hdr_size + fr->hdr_size_add - fr->reqreadsize;
        r = uintr_read(fr->fd, fr->reqbuf + fr->reqreadsize, count);
        errs = IsReadError(fr, r);
        ////if (errs != 0) fprintf(stderr, "0{%d} errs=%d/%d repcount=%d\n", handle, errs, errno, repcount);
        if (errs < 0) return -1;                    // Unrecoverable errors -- return -1
        if (errs > 0) return repcount == 0? -1 : 0; // Recoverable errors -- return -1 only if 1st iteration, since on subsequent attempts empty input is ok

        fr->reqreadsize += r;
        if (r < count) return 0;

        /* Obtain the packet length */
        pktlen = 0; // To make gcc happy
        if      (fr->fdtype == FDIO_STREAM_PLUG)
        {
            fr->being_processed  = 1;
            fr->is_asking_plugin = 1;
            fr->plugin_pktlen = 0;
            fr->notifier(fr->uniq, fr->privptr1,
                         handle, FDIO_R_HEADER_PART, fr->reqbuf, fr->reqreadsize, fr->privptr2);
            fr = watched + handle;
            fr->is_asking_plugin = 0;
            fr->being_processed  = 0;
            if (fr->being_destroyed)
            {
                fr->fd = -1;
                return 0;
            }

            // Should read more data to obtain packet length?
            if (fr->reqreadsize < fr->hdr_size + fr->hdr_size_add)
                goto NEXT_REPEAT;

            // Is an error?
            if (fr->plugin_pktlen < fr->reqreadsize)
            {
                close_because(fr, FDIO_R_PROTOERR);
                return -1;
            }

            // Everything is OK -- length is obtained, just proceed
            pktlen = fr->plugin_pktlen;
        }
        else if (fr->len_size == 0)
        {
            pktlen = fr->hdr_size;
        }
        else
        {
            if (fr->len_endian == FDIO_LEN_BIG_ENDIAN)
            {
                switch (fr->len_size)
                {
                    case 1:
                        pktlen =  fr->reqbuf[fr->len_offset];
                        break;
                    case 2:
                        pktlen = (fr->reqbuf[fr->len_offset]   << 8)
                               +  fr->reqbuf[fr->len_offset+1];
                        break;
                    case 3:
                        pktlen = (fr->reqbuf[fr->len_offset]   << 16)
                               + (fr->reqbuf[fr->len_offset+1] << 8)
                               +  fr->reqbuf[fr->len_offset+2];
                        break;
                    case 4:
                        pktlen = (fr->reqbuf[fr->len_offset]   << 24)
                               + (fr->reqbuf[fr->len_offset+1] << 16)
                               + (fr->reqbuf[fr->len_offset+2] << 8)
                               +  fr->reqbuf[fr->len_offset+3];
                        break;
                }
            }
            else /* FDIO_LEN_LITTLE_ENDIAN */
            {
                switch (fr->len_size)
                {
                    case 1:
                        pktlen =  fr->reqbuf[fr->len_offset];
                        break;
                    case 2:
                        pktlen =  fr->reqbuf[fr->len_offset]
                               + (fr->reqbuf[fr->len_offset+1] << 8);
                        break;
                    case 3:
                        pktlen =  fr->reqbuf[fr->len_offset]
                               + (fr->reqbuf[fr->len_offset+1] << 8)
                               + (fr->reqbuf[fr->len_offset+2] << 16);
                        break;
                    case 4:
                        pktlen =  fr->reqbuf[fr->len_offset]
                               + (fr->reqbuf[fr->len_offset+1] << 8)
                               + (fr->reqbuf[fr->len_offset+2] << 16)
                               + (fr->reqbuf[fr->len_offset+3] << 24);
                        break;
                }
            }

            pktlen = pktlen * fr->len_units + fr->len_add;
        }
        fr->thisreqpktsize = pktlen;

        /* Check packet length */
        if (pktlen > fr->maxinppktsize)
        {
            close_because(fr, FDIO_R_INPKT2BIG);
            return -1;
        }
        if (pktlen < fr->hdr_size)
        {
            close_because(fr, FDIO_R_INPKT2BIG/*!!!FDIO_R_PROTOERR?*/);
            return -1;
        }

        r = GrowBuf((void*) &(fr->reqbuf), &(fr->reqbufsize), pktlen);
        if (r < 0)
        {
            close_because(fr, FDIO_R_ENOMEM);
            return -1;
        }

        /* Change the state */
        fr->reqreadstate = 1;

#if 0
        /* Should we try to read the following data (if any)? */
        if (fr->thisreqpktsize > fr->hdr_size)
        {
            r = check_fd_state(fr->fd, O_RDONLY);
            if (r < 0  &&  !SHOULD_RESTART_SYSCALL())
            {
                close_because(fr, FDIO_R_IOERR);
                return -1;
            }

            if (r > 0) goto FALLTHROUGH;

            /* No, nothing more yet... */
            return 0;
        }
        else
#endif
        {
            goto FALLTHROUGH;
        }
    }
    /* No, this is packet data... */
    else
    {
 FALLTHROUGH:
        /* Try to read the whole remainder of the packet */
        count = fr->thisreqpktsize - fr->reqreadsize;
        
        /* count==0 on fallthrough from header when pktsize==hdr_size */
        if (count != 0)
        {
            r = uintr_read(fr->fd, fr->reqbuf + fr->reqreadsize, count);
            errs = IsReadError(fr, r);
            ////if (errs != 0) fprintf(stderr, "1{%d} errs=%d/%d repcount=%d count=%d thisreqpktsize=%zd pktlen=%zd\n", handle, errs, errno, repcount, count, fr->thisreqpktsize, pktlen);
            if (errs < 0) return -1; // Unrecoverable errors -- return -1
            if (errs > 0) return  0; // Recoverable errors -- should return 0 even on 1st iteration, since we can be here because of fallthrough

            fr->reqreadsize += r;
            if (r < count) return 0;
        }

        /* Okay, the whole packet has arrived */

        /* Reset the read state */
        fr->reqreadstate = 0;
        fr->reqreadsize  = 0;
        fr->hdr_size_add = 0;

        /* And notify the host program */
        fr->being_processed = 1;
        fr->notifier(fr->uniq, fr->privptr1,
                     handle, FDIO_R_DATA, fr->reqbuf, fr->thisreqpktsize, fr->privptr2);
        fr = watched + handle;
        fr->being_processed = 0;
        if (fr->being_destroyed)
        {
            fr->fd = -1;
            return 0;
        }

 NEXT_REPEAT:;
        repcount++;
        if (repcount < fr->maxrepcount) goto REPEAT;
    }

    return 0;
}

static int StreamSend        (fdinfo_t *fr, const uint8 *buf, size_t size)
{
  int        r;
  size_t     newbufsize;

    if (size == 0) return 0;
    
    /* Check if the buffer is already non-empty */
    if (fr->send_locked == 0  &&  fr->_sysbufused != 0)
    {
        r = check_fd_state(fr->fd, O_WRONLY);
        if (r < 0) return -1;
        if (r > 0)
            if (StreamReadyForWrite(fr) < 0) return -1;
    }

    /* Now, if the buffer is empty, we can try to send data right now... */
    if (fr->send_locked == 0  &&  fr->_sysbufused == 0)
    {
        r = uintr_write(fr->fd, buf, size);
        if (r < 0)
        {
            /* What is it -- SNDBUF full or a real error? */
            if (!ERRNO_WOULDBLOCK()) return -1;
        }
        else
        {
            /* Advance the pointer */
            buf  += r;
            size -= r;
        }
    }

    /* Do we still have anything to send later? */
    if (size != 0)
    {
        /* Do we have enough space at the end of _sysbuf? */
        if (fr->_sysbufsize < fr->_sysbufoffset + fr->_sysbufused + size)
        /* ...no */
        {
            /* Should do on-demand move to the bottom */
            if (fr->_sysbufoffset != 0)
            {
                /* Move remaining data to beginning (note memmove(), not memcpy()!) */
                fast_memmove(fr->_sysbuf,
                             fr->_sysbuf + fr->_sysbufoffset,
                             fr->_sysbufused);
                fr->_sysbufoffset = 0;
            }

            newbufsize = fr->_sysbufused + size;
            /* Are we still within legal limits? */
            if (fr->maxoutbufsize != 0  &&  newbufsize > fr->maxoutbufsize)
            {
                errno = EOVERFLOW;
                return -1;
            }

            r = GrowBuf((void *) &(fr->_sysbuf), &(fr->_sysbufsize), newbufsize);
            /*!!! Partial refuse?! */
            if (r < 0) return -1;
        }

        /* Append these to the buffer... */
        fast_memcpy(fr->_sysbuf + fr->_sysbufoffset + fr->_sysbufused, buf, size);
        fr->_sysbufused += size;

        /* And mark descriptor as w-pending */
        sl_set_fd_mask(fr->fdhandle, SL_RD|SL_WR);
    }

    return 0;
}

static int StreamLockSend    (fdinfo_t *fr)
{
    fr->send_locked++;

    return 0;
}

static int StreamUnlockSend  (fdinfo_t *fr)
{
  int  r;

    fr->send_locked--;

    if (fr->send_locked == 0)
    {
        /* This is a copy from StreamSend() -- "Check if the buffer is already non-empty" */
        if (fr->_sysbufused != 0)
        {
            r = check_fd_state(fr->fd, O_WRONLY);
            if (r < 0) return -1;
            if (r > 0)
                if (StreamReadyForWrite(fr) < 0) return -1;
        }
    }

    return 0;
}

static int DgramReadyForWrite(fdinfo_t *fr)
{
  int          r;
  dgraminfo_t *mp;
  size_t       total;

    /* Just a safety net for (hope) an impossible case... */
    if (fr->_sysbufused == 0)
    {
        reporterror("%s(handle=%d,fd=%d): DGRAM-READY-FOR_WRITE, but _sysbufused==0!\n",
                    __FUNCTION__, fr2handle(fr), fr->fd);
        sl_set_fd_mask(fr->fdhandle, SL_RD);
        return 0;
    }

    mp = fr->_sysbuf + fr->_sysbufoffset;
    errno = 0;
    if (mp->to_len != 0)
        r = sendto(fr->fd, mp->data, mp->size, 0, &(mp->to), mp->to_len);
    else
        r = send  (fr->fd, mp->data, mp->size, 0);

    if (r < 0)
    {
        /*!!!???*/
    }

    total = sizeof(*mp) + mp->size;
    fr->_sysbufoffset += total;
    fr->_sysbufused   -= total;
    
    /* Have we completely flushed the buffer? */
    if (fr->_sysbufused == 0)
    {
        sl_set_fd_mask(fr->fdhandle, SL_RD);
        fr->_sysbufoffset = 0;
    }

    return 0;
}

static int DgramReadyForRead (fdinfo_t *fr)
{
  int      handle = fr2handle(fr);

  ssize_t  r;

  int      repcount = 0;

 REPEAT:
    /* As in uintr_read() */
    do {
        errno = 0; /*???Is useful for anything?*/
        fr->last_addr_len = sizeof(fr->last_addr);
        r = recvfrom(fr->fd, fr->reqbuf, fr->maxinppktsize, 0,
                     &(fr->last_addr), &(fr->last_addr_len));
    } while (r < 0  &&  SHOULD_RESTART_SYSCALL());

    /* A part of IsReadError() */
    if (r < 0)
    {
        /*!!! Any other NON-fatal conditions for DGRAM sockets? */
        if (!ERRNO_WOULDBLOCK()  &&  !SHOULD_RESTART_SYSCALL())
        {
            close_because(fr, FDIO_R_IOERR);
            return -1;
        }
        return 0;
    }

    /* Check packet length */
    /* 1. MAX: No reason to check for "r > fr->maxinppktsize",
          because we request recv() of no more than that amount. */
    /* 2. MIN */
    if (r < fr->hdr_size)
    {
        /* Note: we do NOT "close_because(,FDIO_R_INPKT2BIG)",
                 since a connectionless socket which can receive
                 any garbage (and treating that condition as fatal
                 would result in a nice remote DoS possibility). */
        return 0;
    }

    /* Notify the host program */
    fr->being_processed = 1;
    fr->notifier(fr->uniq, fr->privptr1,
                 handle, FDIO_R_DATA, fr->reqbuf, r, fr->privptr2);
    fr = watched + handle;
    fr->being_processed = 0;
    if (fr->being_destroyed)
    {
        fr->fd = -1;
        return 0;
    }
    repcount++;
    if (repcount < fr->maxrepcount) goto REPEAT;

    return 0;
}

static int DgramSendTo       (fdinfo_t *fr, const uint8 *buf, size_t size,
                              struct sockaddr *to, socklen_t to_len)
{
  int          r;
  dgraminfo_t *mp;
  size_t       total;
  size_t       newbufsize;

    /* Check if the buffer is already non-empty */
    if (fr->_sysbufused != 0)
    {
        r = check_fd_state(fr->fd, O_WRONLY);
        if (r < 0) return -1;
        if (r > 0)
            if (DgramReadyForWrite(fr) < 0) return -1;
    }

    /* Now, if the buffer is empty, we can try to send data right now... */
    if (fr->_sysbufused == 0)
    {
        errno = 0;
        if (to_len != 0)
            r = sendto(fr->fd, buf,  size, 0, to, to_len);
        else
            r = send  (fr->fd, buf,  size, 0);

        if (r >= 0) return 0;
    }

    /* Should send later... */

    /* Should do on-demand move to the bottom */
    if (fr->_sysbufoffset != 0)
    {
        /* Move remaining data to beginning (note memmove(), not memcpy()!) */
        fast_memmove(fr->_sysbuf,
                     fr->_sysbuf + fr->_sysbufoffset,
                     fr->_sysbufused);
        fr->_sysbufoffset = 0;
    }

    total = sizeof(*mp) + size;
    newbufsize = fr->_sysbufused + total;
    /* Are we still within legal limits? */
    if (fr->maxoutbufsize != 0  &&  newbufsize > fr->maxoutbufsize)
    {
        errno = EOVERFLOW;
        return -1;
    }
    
    r = GrowBuf((void *) &(fr->_sysbuf), &(fr->_sysbufsize), newbufsize);
    if (r < 0) return -1;
    
    /* Append to the buffer... */
    mp = fr->_sysbuf + fr->_sysbufused;
    mp->size   = size;
    if (to_len != 0) memcpy(&(mp->to), to, to_len);
    mp->to_len = to_len;
    if (size != 0)   fast_memcpy(mp->data, buf, size);
    fr->_sysbufused = newbufsize;
    
    /* And mark descriptor as w-pending */
    sl_set_fd_mask(fr->fdhandle, SL_RD|SL_WR);

    return 0;
}

static int DgramSend         (fdinfo_t *fr, const uint8 *buf, size_t size)
{
    return DgramSendTo(fr, buf, size, NULL, 0);
}

static int StringReadyForWrite(fdinfo_t *fr)
{
    return StreamReadyForWrite(fr);
}

static int StringReadyForRead (fdinfo_t *fr)
{
  int     handle = fr2handle(fr);

  int     r;
  int     count;
  int     errs;

  int     repcount = 0;

  int     reason;

    // Are we in string mode?
    if (fr->reqreadstate == 0)
    {
        while (1)
        {
            r = uintr_read(fr->fd, fr->reqbuf + fr->reqreadsize, 1);
            /* Handle EOF with non-empty line as EOL first */
            if (r == 0  &&  fr->reqreadsize > 0) goto END_OF_LINE;
            /* Check for errors... */
            errs = IsReadError(fr, r);
            ////if (errs != 0) fprintf(stderr, "0{%d} errs=%d/%d repcount=%d\n", handle, errs, errno, repcount);
            if (errs < 0) return -1;                    // Unrecoverable errors -- return -1
            if (errs > 0) return repcount == 0? -1 : 0; // Recoverable errors -- return -1 only if 1st iteration, since on subsequent attempts empty input is ok

            /* Okay, what character is this? */
            if (fr->reqbuf[fr->reqreadsize] == '\n'  ||
                fr->reqbuf[fr->reqreadsize] == '\r'  ||
                fr->reqbuf[fr->reqreadsize] == '\0') goto END_OF_LINE;

            fr->reqreadsize++;
            /* Buffer overflow? */
            if (fr->reqreadsize >= fr->maxinppktsize)
            {
                close_because(fr, FDIO_R_INPKT2BIG);
                return -1;
            }

            repcount++;
        }
 END_OF_LINE:
        fr->reqbuf[fr->reqreadsize] = '\0';
        fr->thisreqpktsize = fr->reqreadsize;

        reason = FDIO_R_DATA;
    }
    else
    // Binary mode: code is copy from StreamReadyForRead()::!(reqreadstate==0)
    {
        /* Try to read the whole remainder of the packet */
        count = fr->thisreqpktsize - fr->reqreadsize;
        
        r = uintr_read(fr->fd, fr->reqbuf + fr->reqreadsize, count);
        errs = IsReadError(fr, r);
        ////if (errs != 0) fprintf(stderr, "1{%d} errs=%d/%d repcount=%d count=%d thisreqpktsize=%zd pktlen=%zd\n", handle, errs, errno, repcount, count, fr->thisreqpktsize, pktlen);
        if (errs < 0) return -1; // Unrecoverable errors -- return -1
        if (errs > 0) return  0; // Recoverable errors -- should return 0 even on 1st iteration, since we can be here because of fallthrough

        fr->reqreadsize += r;
        if (r < count) return 0;

        /* Okay, the whole packet has arrived */

        reason = FDIO_R_BIN_DATA_IN_STRING;
    }

    /* Reset the read state */
    fr->reqreadstate = 0;
    fr->reqreadsize  = 0;
    
    /* And notify the host program */
    fr->being_processed = 1;
    fr->notifier(fr->uniq, fr->privptr1,
                 handle, reason, fr->reqbuf, fr->thisreqpktsize, fr->privptr2);
    fr = watched + handle;
    fr->being_processed = 0;
    if (fr->being_destroyed)
    {
        fr->fd = -1;
        return 0;
    }

    return 0;
}

static int StringSend         (fdinfo_t *fr, const uint8 *buf, size_t size)
{
    return StreamSend(fr, buf, size);
}

static int StringLockSend    (fdinfo_t *fr)
{
    return StreamLockSend  (fr);
}

static int StringUnlockSend  (fdinfo_t *fr)
{
    return StreamUnlockSend(fr);
}

/*
 *  fdio_io_cb()
 *      The I/O meat of fdiolib.
 *
 *  Note:
 *      All calls to notifiers/close_because() must be LAST statements,
 *      since client program may issue fdio_close() or register a new
 *      fd (which will invalidate 'fr').
 */

static void fdio_io_cb(int uniq, void *privptr1,
                       sl_fdh_t fdh, int fd, int mask, void *privptr2)
{
  int        handle = ptr2lint(privptr2);
  fdinfo_t  *fr     = watched + handle;

  int        sock_err;
  socklen_t  errlen;

////fprintf(stderr, "%s(%d, %d, %p)\n", __FUNCTION__, fdh, mask, privptr2);
    switch (fr->fdtype)
    {
        case FDIO_CONNECTING:
        case FDIO_STRING_CONN:
        case FDIO_STREAM_PLUG_CONN:
            /* Okay, what's there? */
            sock_err = errno = 0;
            errlen = sizeof(sock_err);
            if (getsockopt(fr->fd, SOL_SOCKET, SO_ERROR, &sock_err, &errlen) >= 0)
                errno = sock_err;
#ifdef OS_SOLARIS
            if (errno == EPIPE) errno = ETIMEDOUT;
#endif

            if (errno == 0)
            {
                /* Convert from CONN to respective regular type */
                if      (fr->fdtype == FDIO_CONNECTING)       fr->fdtype = FDIO_STREAM;
                else if (fr->fdtype == FDIO_STRING_CONN)      fr->fdtype = FDIO_STRING;
                else if (fr->fdtype == FDIO_STREAM_PLUG_CONN) fr->fdtype = FDIO_STREAM_PLUG;
                sl_set_fd_mask(fr->fdhandle, SL_RD);
                fr->notifier(fr->uniq, fr->privptr1,
                             handle, FDIO_R_CONNECTED, NULL, 0, fr->privptr2);
            }
            else
            {
                close_because(fr, FDIO_R_CONNERR);
            }
            break;

        case FDIO_STREAM:
        case FDIO_STREAM_PLUG:
            /* Handle "ready-for-write" first... */
            if (mask & SL_WR) if (StreamReadyForWrite(fr) < 0) return;

            /* ...and then, if we are still here, "ready-for-read" */
            if (mask & SL_RD)     StreamReadyForRead (fr);
            break;

        case FDIO_LISTEN:
            fr->notifier(fr->uniq, fr->privptr1,
                         handle, FDIO_R_ACCEPT, NULL, 0, fr->privptr2);
            break;

        case FDIO_DGRAM:
            /* Handle "ready-for-write" first... */
            if (mask & SL_WR) if (DgramReadyForWrite(fr) < 0) return;
            
            /* ...and then, if we are still here, "ready-for-read" */
            if (mask & SL_RD)     DgramReadyForRead (fr);
            break;

        case FDIO_STRING:
            /* Handle "ready-for-write" first... */
            if (mask & SL_WR) if (StringReadyForWrite(fr) < 0) return;

            /* ...and then, if we are still here, "ready-for-read" */
            if (mask & SL_RD)     StringReadyForRead (fr);
            break;

        default:
            reporterror("%s(handle=%d,fd=%d): unknown value of .fdtype=%d\n",
                        __FUNCTION__, handle, fr->fd, fr->fdtype);
    }
}

static int find_free_watched_slot(void)
{
  int        handle;

    for (handle = 1;  handle < watched_allocd;  handle++)
        if (watched[handle].fd < 0) return handle;

    return -1;
}

fdio_handle_t fdio_register_fd(int uniq, void *privptr1,
                               int            fd,        
                               fdio_fdtype_t  fdtype,    
                               fdio_ntfr_t    notifier,  
                               void          *privptr2,
                               size_t         maxpktsize,
                               size_t         hdr_size,  
                               size_t         len_offset,
                               size_t         len_size,  
                               char           len_endian,
                               size_t         len_units, 
                               size_t         len_add)
{
  int        handle;
  fdinfo_t  *fr = NULL;
  fdinfo_t  *new_watched;

  int        iomask;

  size_t     minbufsize;
  void      *_sysbuf;
  void      *reqbuf;

  int        saved_errno;

  struct  sigaction  act;

    /* Check if SIGPIPE handling is still DFL */
    if (sigaction(SIGPIPE, NULL, &act) == 0            &&
        (void*)(act.sa_handler)   == (void*)(SIG_DFL)  &&
        (void*)(act.sa_sigaction) == (void*)(SIG_DFL))
    {
        ////fprintf(stderr, "IGNORING SIGPIPE!!!\n");
        set_signal(SIGPIPE, SIG_IGN);
    }

    if (uniq_checker != NULL  &&  uniq_checker(__FUNCTION__, uniq)) return -1;

    /* Check validity of parameters */
    switch (fdtype)
    {
        case FDIO_CONNECTING:
        case FDIO_STRING_CONN:
        case FDIO_STREAM_PLUG_CONN:
            iomask = SL_WR | SL_CE;
            break;
            
        case FDIO_STREAM:
        case FDIO_LISTEN:
        case FDIO_DGRAM:
        case FDIO_STRING:
        case FDIO_STREAM_PLUG:
            iomask = SL_RD;
            break;
            
        default:
            errno = EINVAL;
            return -1;
    }
    if (
        len_size != 0  &&
        len_size != 1  &&  len_size != 2  &&  len_size != 3  &&  len_size != 4
       )
    {
        errno = EINVAL;
        return -1;
    }
    if (
        (len_endian != FDIO_LEN_LITTLE_ENDIAN  &&  len_endian != FDIO_LEN_BIG_ENDIAN)
       )
    {
        errno = EINVAL;
        return -1;
    }

    minbufsize = hdr_size;
    if (minbufsize < 16) minbufsize = 16;
    if (fdtype == FDIO_DGRAM   ||
        fdtype == FDIO_STRING  || fdtype == FDIO_STRING_CONN)
        /* For DGRAM allocate space for max.possible packet,
           for STRING allocate space for whole line */
        if (minbufsize < maxpktsize) minbufsize = maxpktsize;
    
    /* 1. Check if descriptor looks valid... */
    if (fd < 0)
    {
        errno = EBADF;
        return -1;
    }
    if (fd > FDIO_MAX_SUPPORTED_FD)
    {
        errno = EMFILE;
        return -1;
    }

    /* 1.5. Try to switch it to non-blocking mode... */
    /*      (this also tests that the descriptor is real) */
    if (set_fd_flags(fd, O_NONBLOCK, 1) < 0) return -1;

    /* 2. Check if this fd was already registered */
    for (handle = 1;  handle < watched_allocd;  handle++)
        if (watched[handle].fd == fd)
        {
            errno = EBUSY;
            return -1;
        }
    
    /* 3. Find a free cell... */
    handle = find_free_watched_slot();
    if (handle > 0) goto CELL_FOUND;

    /* 3.5. None found?  Okay, let's allocate some more */
    new_watched = safe_realloc(watched,
                               sizeof(fdinfo_t) * (watched_allocd + WATCHED_ALLOC_INCREMENT));
    if (new_watched == NULL) return -1;
    watched = new_watched;

    for (handle = watched_allocd;
         handle < watched_allocd + WATCHED_ALLOC_INCREMENT;
         handle ++)
        watched[handle].fd = -1;

    watched_allocd += WATCHED_ALLOC_INCREMENT;

    /* Re-do search */
    handle = find_free_watched_slot();
    
 CELL_FOUND:
    /* Allocate minimal buffers first */
    _sysbuf  = malloc(minbufsize);
    reqbuf   = malloc(minbufsize);
    if (_sysbuf == NULL  ||  reqbuf == NULL)
    {
        errno = ENOMEM;
        goto ERREXIT;
    }
    bzero(_sysbuf, minbufsize);
    bzero(reqbuf, minbufsize);
     
    /* Okay, let's fill the cell */
    fr = watched + handle;
    bzero(fr, sizeof(*fr));

    fr->fd            = fd;
    fr->fdtype        = fdtype;

    fr->notifier      = notifier;
    fr->uniq          = uniq;
    fr->privptr1      = privptr1;
    fr->privptr2      = privptr2;

    fr->maxinppktsize = maxpktsize;
    fr->hdr_size      = hdr_size;
    fr->len_offset    = len_offset;
    fr->len_size      = len_size;
    fr->len_endian    = len_endian;
    fr->len_units     = len_units;
    fr->len_add       = len_add;

    fr->maxoutbufsize = 0;
    fr->maxrepcount   = 30;

    fr->_sysbuf       = _sysbuf;
    fr->_sysbufsize   = minbufsize;
    fr->reqbuf        = reqbuf;
    fr->reqbufsize    = minbufsize;

    /* Register with scheduler */
    fr->fdhandle = sl_add_fd(uniq, NULL,
                             fd, iomask, fdio_io_cb, lint2ptr(handle));
    if (fr->fdhandle < 0) goto ERREXIT;

    return handle;
    
 ERREXIT:
    saved_errno = errno;
    safe_free(_sysbuf);
    safe_free(reqbuf);
    if (fr != NULL) fr->fd = -1;
    errno = saved_errno;
    return -1;
}

int  fdio_deregister          (fdio_handle_t handle)
{
  fdinfo_t  *fr;

    DECODE_AND_LAZYCHECK(-1);

    fr->being_destroyed = 1;

    /* Deregister with scheduler */
    sl_del_fd(fr->fdhandle);

    /* Free buffers */
    free(fr->_sysbuf);
    free(fr->reqbuf);

    /* Forget it */
    if (!fr->being_processed) fr->fd = -1;

    return 0;
}

int  fdio_deregister_flush    (fdio_handle_t handle, int max_wait_secs)
{
  fdinfo_t  *fr;
  int        fd;
  int        r;

    DECODE_AND_LAZYCHECK(-1);

    if (fr->is_defunct || 1)
    {
        fd = fr->fd;
        r  = fdio_deregister(handle);
        close(fd);
        return r;
    }

    /* Here do tricks with deferred-closing-preparation... */
}

int  fdio_lasterr             (fdio_handle_t handle)
{
  fdinfo_t  *fr;
  
    DECODE_AND_LAZYCHECK(-1);

    return fr->last_err;
}

int  fdio_set_maxsbuf         (fdio_handle_t handle, size_t maxoutbufsize)
{
  fdinfo_t  *fr;
  
    DECODE_AND_CHECK(-1);

    fr->maxoutbufsize = maxoutbufsize;
    
    return 0;
}

int  fdio_set_maxpktsize      (fdio_handle_t handle, size_t maxpktsize)
{
  fdinfo_t  *fr;
  
    DECODE_AND_CHECK(-1);

    fr->maxinppktsize = maxpktsize;
    
    return 0;
}

int  fdio_set_len_endian      (fdio_handle_t handle, char  len_endian)
{
  fdinfo_t  *fr;
  
    DECODE_AND_CHECK(-1);

    if (
        (len_endian != FDIO_LEN_LITTLE_ENDIAN  &&  len_endian != FDIO_LEN_BIG_ENDIAN)
       )
    {
        errno = EINVAL;
        return -1;
    }
    
    fr->len_endian = len_endian;
    
    return 0;
}

int  fdio_set_maxreadrepcount (fdio_handle_t handle, int    maxrepcount)
{
  fdinfo_t  *fr;
  
    DECODE_AND_CHECK(-1);

    fr->maxrepcount = maxrepcount;
    
    return 0;
}

int  fdio_advice_hdr_size_add (fdio_handle_t handle, size_t hdr_size_add)
{
  fdinfo_t  *fr;
  
    DECODE_AND_CHECK(-1);

    if (fr->is_asking_plugin == 0)
    {
        errno = EINVAL;
        return -1;
    }
    if (hdr_size_add < fr->hdr_size_add)
    {
        errno = EINVAL;
        return -1;
    }

    fr->hdr_size_add  = hdr_size_add;
    
    return 0;
}

int  fdio_advice_pktlen       (fdio_handle_t handle, size_t pktlen)
{
  fdinfo_t  *fr;
  
    DECODE_AND_CHECK(-1);

    if (fr->is_asking_plugin == 0)
    {
        errno = EINVAL;
        return -1;
    }

    fr->plugin_pktlen = pktlen;
    
    return 0;
}

int  fdio_string_req_binary   (fdio_handle_t handle, size_t datasize)
{
  fdinfo_t  *fr;
  
    DECODE_AND_CHECK(-1);

    if (fr->fdtype != FDIO_STRING)
    {
        errno = EINVAL;
        return -1;
    }

    // No change while in a process of reading
    if (fr->reqreadsize != 0)
    {
        errno = EINVAL;
        return -1;
    }

    if      (datasize > fr->maxinppktsize)
    {
        errno = EOVERFLOW;
        return -1;
    }
    else
    {
        fr->thisreqpktsize = datasize;
        fr->reqreadstate   = (datasize == 0)? 0 : 1;  // datasize==0 means "drop previous binary request"
    }

    return 0;
}

int  fdio_fd_of               (fdio_handle_t handle)
{
  fdinfo_t  *fr;
  
    DECODE_AND_CHECK(-1);

    return fr->fd;
}

int  fdio_accept              (fdio_handle_t handle,
                               struct sockaddr *addr, socklen_t *addrlen)
{
  fdinfo_t  *fr;
  
    DECODE_AND_CHECK(-1);

    if (fr->fdtype != FDIO_LISTEN)
    {
        errno = EINVAL;
        return -1;
    }

    return accept(fr->fd, addr, addrlen);
}

int  fdio_last_src_addr       (fdio_handle_t handle,
                               struct sockaddr *addr, socklen_t *addrlen)
{
  fdinfo_t  *fr;
  socklen_t  len;
  
    DECODE_AND_CHECK(-1);

    if (fr->fdtype != FDIO_DGRAM)
    {
        errno = EINVAL;
        return -1;
    }

    if (addr == NULL) return 0;
    len = *addrlen;
    if (len < 0)
    {
        errno = EINVAL;
        return -1;
    }

    if (len > fr->last_addr_len)
        len = fr->last_addr_len;

    if (len != 0)
        memcpy(addr, &(fr->last_addr), len);

    *addrlen = len;
    return 0;
}


/*
 *  Note:
 *      Each packet will be *completely* accepted or rejected --
 *      the situation when the first half is accepted and sent, and the second
 *      half doesn't fit into buffer and is rejected will never happen.
 *
 *      This is because "reject" can happen in two cases:
 *      1.
 *      2. "_sysbuf full" (newbufsize>maxbufsize) -- but this
 *         can happen when something is already present in _sysbuf,
 *         in which case there would be NO "direct write", so,
 *         a *complete* packet would be put or rejected.
 */

int  fdio_send                (fdio_handle_t handle, const void *buf, size_t size)
{
  fdinfo_t  *fr;
  
    DECODE_AND_LAZYCHECK(-1); // "LAZY" is to be able to send reply on *protocol* errors (see bigfile-0002.html@15.01.2007)

    switch (fr->fdtype)
    {
        case FDIO_STREAM:
            return StreamSend(fr, buf, size);
            break;

        case FDIO_DGRAM:
            return DgramSend (fr, buf, size);
            break;
        
        case FDIO_STRING:
            return StringSend(fr, buf, size);
            break;

        default:
            errno = EINVAL;
            return -1;
    }
}

int  fdio_lock_send           (fdio_handle_t handle)
{
  fdinfo_t  *fr;
  
    DECODE_AND_LAZYCHECK(-1); // "LAZY" as in fdio_send()

    switch (fr->fdtype)
    {
        case FDIO_STREAM:
            return StreamLockSend  (fr);
            break;

        case FDIO_STRING:
            return StringLockSend  (fr);
            break;

        default:
            errno = EINVAL;
            return -1;
    }
}

int  fdio_unlock_send         (fdio_handle_t handle)
{
  fdinfo_t  *fr;
  
    DECODE_AND_LAZYCHECK(-1); // "LAZY" as in fdio_send()

    switch (fr->fdtype)
    {
        case FDIO_STREAM:
            return StreamUnlockSend(fr);
            break;

        case FDIO_STRING:
            return StringUnlockSend(fr);
            break;

        default:
            errno = EINVAL;
            return -1;
    }
}

int  fdio_reply               (fdio_handle_t handle, const void *buf, size_t size)
{
  fdinfo_t  *fr;
  
    DECODE_AND_LAZYCHECK(-1); // "LAZY" as in fdio_send()

    if (fr->last_addr_len != 0)
        return fdio_send_to(handle, buf, size,
                            &(fr->last_addr), fr->last_addr_len);
    else
        return fdio_send   (handle, buf, size);
                            
}

int  fdio_send_to             (fdio_handle_t handle, const void *buf, size_t size,
                               struct sockaddr *to, socklen_t tolen)
{
  fdinfo_t  *fr;
  
    DECODE_AND_LAZYCHECK(-1); // "LAZY" as in fdio_send()

    switch (fr->fdtype)
    {
        case FDIO_DGRAM:
            return DgramSendTo(fr, buf, size, to, tolen);
            break;

        default:
            errno = EINVAL;
            return -1;
    }
}


/*
 *  Note: this list should be kept in sync with
 *  include/fdiolib.h::FDIO_R_XXX enum.
 */
static char *_fdio_rsnlist[] =
{
    "Data arrived",
    "Connection established",
    "Incoming connection",
    "-FIRST-ERROR-CODE-",
    "Closed at other side",
    "Connection failure",
    "I/O error",
    "Protocol error",
    "Input packet is too big",
    "Out of memory"
};

char *fdio_strreason          (int reason)
{
  static char buf[100];
  
    if (reason < 0  ||  reason >= (signed)(sizeof(_fdio_rsnlist) / sizeof(_fdio_rsnlist[0])))
    {
        sprintf(buf, "Unknown reason code %d", reason);
        return buf;
    }

    return _fdio_rsnlist[reason];
}


void fdio_set_uniq_checker(fdio_uniq_checker_t checker)
{
    uniq_checker = checker;
}

void fdio_do_cleanup          (int uniq)
{
  fdio_handle_t  handle;
  int            fd;

    for (handle = 1;  handle < watched_allocd;  handle++)
        if (watched[handle].fd >= 0  &&  watched[handle].uniq == uniq)
        {
            fd = watched[handle].fd;
            fdio_deregister(handle);
            close          (fd);
        }
}
