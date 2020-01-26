#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <errno.h>

#include "misc_macros.h"
#include "misclib.h"
#include "timeval_utils.h"
#include "cxscheduler.h"
#include "fdiolib.h"

#include "cx.h"
#include "cda.h"

#include "console_cda_util.h"

#ifdef BUILTINS_DECLARATION_H_FILE
  #include BUILTINS_DECLARATION_H_FILE
#endif /* BUILTINS_DECLARATION_H_FILE */


//////////////////////////////////////////////////////////////////////
/* Note: the following refrec_t and RecSlot code
   is a copy from cdaclient.c */

typedef struct
{
    int            in_use;
    util_refrec_t  ur;
} refrec_t;

static refrec_t *refrecs_list        = NULL;
static int       refrecs_list_allocd = 0;

// GetRefRecSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, RefRec, refrec_t,
                                 refrecs, in_use, 0, 1,
                                 0, 10, 1000,
                                 , , void)

static void RlsRefRecSlot(int rn)
{
  refrec_t *rp = AccessRefRecSlot(rn);

    safe_free(rp->ur.spec);
    /*!!! Note: we do NOT safe_free(rp->ur.databuf), because it can be
      shared between multiple channels, and we do NOT duplicate it in 
      RememberOneChan() */
    rp->in_use = 0;
}

static int rp2rn(refrec_t *rp)
{
    return rp - refrecs_list;
}

//////////////////////////////////////////////////////////////////////

static const char    *global_argv0;

static char          *option_baseref   = NULL;
static char          *option_defpfx    = NULL;
static int            option_binary    = 0;
static const char    *option_bin_name  = NULL;
static int            option_cont_bin  = 0;
static int            option_help      = 0;
static double         option_timelim   = 0;
static int            option_w_unbuff  = 0;

static cda_context_t  the_cid;

static sl_fdh_t       binary_fdhandle;
static fdio_handle_t  string_fhandle;

static refrec_t       bin_rr;
static char           bin_chanref[CDA_PATH_MAX];
static size_t         bin_sp_datasize;      // dtype+nelems-SPecified datasize
static size_t         bin_reqreadsize = 0;  // Bytes currently read ("in reqbuf")

static void          *buf2        = NULL;
static size_t         buf2_allocd = 0;
static size_t         buf2_failed = 0;

static int            num2write = 0;
static int            EOF_rcvd  = 0;


static void ProcessContextEvent(int            uniq,
                                void          *privptr1,
                                cda_context_t  cid,
                                int            reason,
                                int            info_int,
                                void          *privptr2)
{
////fprintf(stderr, "%s  %s(cid=%d, reason=%d, info_int=%d)\n", strcurtime(), __FUNCTION__, cid, reason, info_int);
    switch (reason)
    {
    }
}

static void PerformWrite(refrec_t *rp)
{
  void          *buf;
  int            r;

////    fprintf(stderr, "W\n");
    buf = rp->ur.databuf;
    if (buf == NULL) buf = &(rp->ur.val2wr);
    if ((r = cda_snd_ref_data(rp->ur.ref, rp->ur.dtype, rp->ur.num2wr, buf)) >= 0)
    {
        rp->ur.wr_req = 0;
        rp->ur.wr_snt = 1;
    }
    else
        fprintf(stderr, "%s %s: fail to cda_snd_ref_data(\"%s\", nelems=%d): %s\n",
                strcurtime(), global_argv0, rp->ur.spec, rp->ur.num2wr, strerror(errno));
}

static void ProcessDatarefEvent(int            uniq,
                                void          *privptr1,
                                cda_dataref_t  ref,
                                int            reason,
                                void          *info_ptr,
                                void          *privptr2)
{
  int       rn = ptr2lint(privptr2);
  refrec_t *rp = option_binary? &bin_rr : AccessRefRecSlot(rn);

    if      (reason == CDA_REF_R_RSLVSTAT)
    {
        if      (ptr2lint(info_ptr) == CDA_RSLVSTAT_NOTFOUND)
        {
            fprintf(stderr, "%s %s: channel \"%s\" not found\n",
                    strcurtime(), global_argv0, rp->ur.spec);
            if (option_binary) exit(EC_ERR);
            else
            {
                //!!! manage wr_req/wr_snt
            }
        }
    }
    else if (reason == CDA_REF_R_UPDATE)
    {
        // Mark "update was received, may send further data immediately upon reading"
        rp->ur.rd_req = 0;
        // "Write was finished"
        if (rp->ur.wr_snt)
        {
            rp->ur.wr_snt = 0;
            // Do accounting: when (wr_req,wr_snt) go to (0,0) -- decrease num2wr
            if (rp->ur.wr_req == 0) num2write--;
        }
        // Send data if requested
        if (rp->ur.wr_req)
            PerformWrite(rp);

        // Is it time to finish (EOF and all requests had been executed)?
        if (EOF_rcvd  &&  num2write == 0)
            exit(EC_OK);
    }
}

static void finish_proc(int uniq, void *privptr1,
                        sl_tid_t tid,                   void *privptr2)
{
    sl_break();
}
static void SetWaitLimit(void)
{
  struct timeval  now;
  struct timeval  timelim;
  double          dummy;

    if (option_timelim <= 0) return;

    timelim.tv_sec  = option_timelim;
    timelim.tv_usec = modf(option_timelim, &dummy) * 1000000;
    gettimeofday(&now, NULL);
    timeval_add(&timelim, &now, &timelim);
    sl_enq_tout_at(0, NULL, &timelim, finish_proc, NULL);
}

static void stdin_in_cb(int uniq, void *privptr1,
                        sl_fdh_t fdh, int fd, int mask, void *privptr2)
{
  void    *binbuf;  // ALWAYS points to the place from which writes are performed (either val2wr or databuf)
  uint8   *reqbuf;  // Points to the place where STDIN is being read
  size_t   count;
  ssize_t  r;

    binbuf = bin_rr.ur.databuf;
    if (binbuf == NULL) binbuf = &(bin_rr.ur.val2wr);

    /* Select a buffer where to read data to */
    if (bin_rr.ur.wr_req == 0)
        reqbuf = binbuf;
    /* ("-C" mode only): use another buffer if main one already holds data */
    else
    {
        if (buf2 == NULL)
        {
            buf2 = malloc(bin_sp_datasize);
            if (buf2 == NULL)
            {
                fprintf(stderr, "%s %s: failed to malloc(%zd) for shadow buffer: %s\n",
                        strcurtime(), global_argv0, bin_sp_datasize, strerror(errno));
                EOF_rcvd = 1;
                sl_del_fd(fdh);
                SetWaitLimit();
            }
            else
                buf2_allocd = bin_sp_datasize;
        }
        reqbuf = buf2;
    }

    count = bin_sp_datasize - bin_reqreadsize;

    if (count != 0)
    {
        r = uintr_read(fd, reqbuf + bin_reqreadsize, count);
        if      (r < 0)
        {
            if (ERRNO_WOULDBLOCK()) return;

            EOF_rcvd = 1; //!!!??? why this was NOT before 27-09-2019?
            sl_del_fd(fdh);
            SetWaitLimit();
            return;
        }
        else if (r == 0)
        {
            EOF_rcvd = 1;
            sl_del_fd(fdh);
            SetWaitLimit();
            if (option_cont_bin)
            {
                // No pending write request, and no pending "was sent, wait for result"?
                if (bin_rr.ur.wr_req == 0  &&  bin_rr.ur.wr_snt == 0)
                    exit(EC_OK);
                // There is/are -- okay, let's wait
                return;
            }
        }
        else
        {
            bin_reqreadsize += r;
            if (r < count) return;
            if (!option_cont_bin)
            {
                EOF_rcvd = 1;
                sl_del_fd(fdh);
                SetWaitLimit();
            }
        }
    }

////    fprintf(stderr, "A\n");
    // Do accounting: when (wr_req,wr_snt) go from (0,0) -- increase num2wr
    if (bin_rr.ur.wr_req + bin_rr.ur.wr_snt == 0) num2write++;
    // If intermediate read()s were to buf2, copy its content into main buffer (from which writes are performed)
    if (bin_rr.ur.wr_req != 0  &&  bin_reqreadsize != 0)
        memcpy(binbuf, reqbuf, bin_reqreadsize);
    /* Okay, the whole data had arrived;
       its size is current values of bin_reqreadsize */
    bin_rr.ur.num2wr = bin_reqreadsize / sizeof_cxdtype(bin_rr.ur.dtype);
    bin_rr.ur.wr_req = 1;
    if (bin_rr.ur.rd_req == 0 /* "update was received" */  &&
        bin_rr.ur.wr_snt == 0 /* no write in process */)
        PerformWrite(&bin_rr);

    // Re-initialize read state
    bin_reqreadsize = 0;
}

static int  RecSlotNameCompare(refrec_t *rp, void *privptr)
{
    // "name == ur.spec"
    return strcmp(privptr, rp->ur.spec) == 0;
}
static void ProcessFdioEvent(int uniq, void *unsdptr,
                             fdio_handle_t handle, int reason,
                             void *inpkt, size_t inpktsize,
                             void *privptr)
{
  const char    *p = inpkt;
  char           chanref[CDA_PATH_MAX];
  char          *endptr;

  int            rn;
  refrec_t      *rp;

  util_refrec_t  ur;

  size_t         buf2_size_rqd;

fprintf(stderr, "\treason=%d <%s>\n", reason, fdio_strreason(reason));
    /* 0. Check reason */
    if (reason != FDIO_R_DATA)
    {
        if (reason != FDIO_R_CLOSED)
            fprintf(stderr, "%s %s: %s\n",
                    strcurtime(), global_argv0, fdio_strreason(reason));
        goto STOP_READING_STDIN;
    }

    /* I. Parse name */
    // Ltrim
    while (*p != '\0'  &&  isspace(*p)) p++;
    // Skip "@TIME" prefix (for compatibility with cdaclient-created files)
    if (*p == '@'  &&  isdigit(p[1]))
    {
        // Timestamp itself...
        while (*p != '\0'  &&  !isspace(*p)) p++;
        // ...spaces
        while (*p != '\0'  &&   isspace(*p)) p++;
    }
    // An empty or comment line?
    if (*p == '\0'  ||  *p == '#') return;

    /* Channel name */
    bzero(&ur, sizeof(ur));
    if (ParseDatarefSpec(global_argv0, p,
                         &endptr, chanref, sizeof(chanref), &ur) < 0) return;
    if (strcmp(chanref, "-") == 0) return;

    /* Skip spaces or '=' sign */
    while (*endptr != '\0'  &&
           (isspace(*endptr)  /*||  *endptr == '='*/))
           endptr++;

    /* No value?! */
    if (*endptr == '\0') return;

    /* II. Okay, we've got a channel name */
    rn = ForeachRefRecSlot(RecSlotNameCompare, chanref);
    // If not found -- try to allocate a new slot
    if (rn < 0)
    {
        // Allocate
        rn = GetRefRecSlot();
        if (rn < 0)
        {
            fprintf(stderr, "%s %s: GetRefRecSlot(): %s\n---- STOPPING READING ----\n",
                    strcurtime(), global_argv0, strerror(errno));
            goto STOP_READING_STDIN;
        }

        // Fill with data
        rp = AccessRefRecSlot(rn);
        rp->ur = ur;
        rp->ur.spec = strdup(chanref);
        if (rp->ur.spec == NULL)
        {
            rp->ur.ref = CDA_DATAREF_ERROR; // Precaution just in case
            fprintf(stderr, "%s %s: strdup(chanref): %s\n---- STOPPING READING ----\n",
                    strcurtime(), global_argv0, strerror(errno));
            goto STOP_READING_STDIN;
        }

        // Allocate a buffer, if needed
        if (rp->ur.n_items > 1)
        {
            rp->ur.databuf_allocd = sizeof_cxdtype(rp->ur.dtype) * rp->ur.n_items;
            rp->ur.databuf = malloc(rp->ur.databuf_allocd);
            if (rp->ur.databuf == NULL)
            {
                fprintf(stderr, "%s %s: failed to malloc(%zd bytes) buffer for %s value: %s\n",
                        strcurtime(), global_argv0, rp->ur.databuf_allocd, rp->ur.spec, strerror(errno));
                rp->ur.ref = CDA_DATAREF_ERROR;
                return;
            }
        }

        // Activate the channel
fprintf(stderr, "spec=<%s>\n", rp->ur.spec);
        rp->ur.ref = cda_add_chan(the_cid,
                                  option_baseref,
                                  rp->ur.spec, rp->ur.options|CDA_DATAREF_OPT_PRIVATE, rp->ur.dtype, rp->ur.n_items,
                                  0, NULL, NULL);
        if (rp->ur.ref == CDA_DATAREF_ERROR)
            fprintf(stderr, "%s %s: cda_add_chan(\"%s\"): %s\n",
                    strcurtime(), global_argv0, rp->ur.spec, cda_last_err());
        else
            cda_add_dataref_evproc(rp->ur.ref,
                                   CDA_REF_EVMASK_UPDATE | CDA_REF_EVMASK_RSLVSTAT,
                                   ProcessDatarefEvent, lint2ptr(rn));
    }

    rp = AccessRefRecSlot(rn);
    // A non-functioning placeholder?
    if (rp->ur.ref == CDA_DATAREF_ERROR) return;

    /* III. Now we can parse the value */
    ur = rp->ur; // Make a copy
    if (rp->ur.wr_req == 0  ||  rp->ur.n_items <= 1)
    {
        if (ParseDatarefVal(global_argv0, endptr, &endptr, &ur) < 0) return;
        rp->ur.val2wr = ur.val2wr;
        rp->ur.num2wr = ur.num2wr;
    }
    else
    {
        // MUST use buf2 for currently-wr_req-pending-channels
        // a. Make it sufficiently sized
        buf2_size_rqd = sizeof_cxdtype(rp->ur.dtype) * rp->ur.n_items;
        if (GrowBuf(&buf2, &buf2_allocd, buf2_size_rqd) < 0)
        {
            if (buf2_failed == 0  ||  buf2_failed > buf2_size_rqd)
                fprintf(stderr, "%s %s: failed to malloc(%zd bytes) shadow buffer for %s value: %s\n",
                        strcurtime(), global_argv0, buf2_size_rqd, rp->ur.spec, strerror(errno));
            buf2_failed = buf2_size_rqd;
            return;
        }
        // b. Use
        ur.databuf = buf2;
        if (ParseDatarefVal(global_argv0, endptr, &endptr, &ur) < 0) return;
        // c. Copy the value into regular write-buffer
        if (ur.num2wr > 0)
            memcpy(rp->ur.databuf, buf2, sizeof_cxdtype(ur.dtype) * ur.num2wr);
        rp->ur.num2wr = ur.num2wr;
    }

    /* IV. Perform write, if possible */
    // Do accounting: when (wr_req,wr_snt) go from (0,0) -- increase num2wr
    if (bin_rr.ur.wr_req + bin_rr.ur.wr_snt == 0) num2write++;
    // Proceed with writing
    rp->ur.wr_req = 1;
    if (rp->ur.rd_req == 0 /* "update was received" */  &&
        rp->ur.wr_snt == 0 /* no write in process */)
        PerformWrite(rp);

    return;

 STOP_READING_STDIN:
    EOF_rcvd = 1;
    fdio_deregister(handle);
    SetWaitLimit();
    if (num2write == 0) exit(EC_OK);
}

int main(int argc, char *argv[])
{
  int            c;
  int            err = 0;

    set_signal(SIGPIPE, SIG_IGN);

#ifdef BUILTINS_REGISTRATION_CODE
    BUILTINS_REGISTRATION_CODE
#endif /* BUILTINS_REGISTRATION_CODE */

    global_argv0 = argv[0];

    while ((c = getopt(argc, argv, "b:B:C:d:hW:w")) != EOF)
        switch (c)
        {
            case 'b':
                option_baseref = optarg;
                break;

            case 'B':
            case 'C':
                if (option_binary)
                {
                    fprintf(stderr, "%s: multiple -B/-C options are forbidden\n", argv[0]);
                    err++;
                }
                option_binary  = 1;
                option_bin_name= optarg;
                option_cont_bin= (c == 'C');
                break;

            case 'd':
                option_defpfx  = optarg;
                break;

            case 'h':
                option_help++;
                break;

            case 'W':
                option_timelim  = atof(optarg);
                break;

            case 'w':
                option_w_unbuff = 1;
                break;

            case ':':
            case '?':
                err++;
        }

    if (option_help)
    {
        printf("Usage (stdin is used for reading):\n"
               "\n"
               "a. %s [OPTIONS]\n"
               "   -- read CHANNEL-TO-WRITE=VALUE commands in text format (one per line)\n"
               "\n"
               "b. %s -B CHANNEL-TO-WRITE\n"
               "   -- read Binary data once (any size from 0 to max)\n"
               "\n"
               "c. %s -C CHANNEL-TO-WRITE\n"
               "   -- read binary data Continuously till EOF\n"
               "      (each packet size MUST EXACTLY correspond to CHANNEL spec)\n"
               "\n"
               "(for full CHANNEL syntax see 'cdaclient -hh')\n"
               "\n"
               "Options:\n"
               "  -b BASEREF\n"
               "  -W DURATION -- how many seconds to Wait after EOF (time limit)\n"
               "  -w          -- send writes to cda immediately (w/o builtin buffering)\n"
               "  -h  show this help\n",
               argv[0], argv[0], argv[0]);
        exit(EC_HELP);
    }

    if (err)
    {
        fprintf(stderr, "Try '%s -h' for help.\n", argv[0]);
        exit(EC_USR_ERR);
    }

    //////////////////////////////////////////////////////////////////

    if (set_fd_flags(STDIN_FILENO, O_NONBLOCK, 1) < 0)
    {
        fprintf(stderr, "%s %s: error switching STDIN to non-blocking mode: %s\n",
                strcurtime(), argv[0], strerror(errno));
        exit(EC_ERR);
    }

    the_cid = cda_new_context(0, NULL, option_defpfx, 0, argv[0],
                              CDA_CTX_EVMASK_CYCLE, ProcessContextEvent, NULL);
    if (the_cid == CDA_CONTEXT_ERROR)
    {
        fprintf(stderr, "%s %s: cda_new_context(\"%s\"): %s\n",
                strcurtime(), argv[0],
                option_defpfx != NULL? option_defpfx : "",
                cda_last_err());
        exit(EC_ERR);
    }

    if (option_binary)
    {
        // Get channel spec
        ParseDatarefSpec(argv[0], option_bin_name,
                         NULL, bin_chanref, sizeof(bin_chanref), &bin_rr.ur);
        bin_rr.ur.spec = bin_chanref;

        // Allocate buffer
        bin_sp_datasize = sizeof_cxdtype(bin_rr.ur.dtype) * bin_rr.ur.n_items;
        if (bin_sp_datasize > sizeof(bin_rr.ur.val2wr))
        {
            if ((bin_rr.ur.databuf = malloc(bin_sp_datasize)) == NULL)
            {
                fprintf(stderr, "%s %s: failed to malloc(%zd bytes): %s\n",
                        strcurtime(), argv[0], bin_sp_datasize,
                        strerror(errno));
                exit(EC_ERR);
            }
        }

        // Activate channel
        bin_rr.ur.ref = cda_add_chan(the_cid,
                                     option_baseref,
                                     bin_rr.ur.spec, bin_rr.ur.options, bin_rr.ur.dtype, bin_rr.ur.n_items,
                                     0, NULL, NULL);
        if (bin_rr.ur.ref == CDA_DATAREF_ERROR)
        {
            fprintf(stderr, "%s %s: cda_add_chan(\"%s\"): %s\n",
                    strcurtime(), argv[0], bin_rr.ur.spec, cda_last_err());
            exit(EC_ERR);
        }
        bin_rr.ur.rd_req = 1;
        cda_add_dataref_evproc(bin_rr.ur.ref,
                               CDA_REF_EVMASK_UPDATE | CDA_REF_EVMASK_RSLVSTAT,
                               ProcessDatarefEvent, lint2ptr(0)/*!!!*/);

        binary_fdhandle = sl_add_fd(0, NULL,
                                    STDIN_FILENO, SL_RD,
                                    stdin_in_cb, NULL);
    }
    else
    {
        console_cda_util_EC_USR_ERR_mode(0);
        string_fhandle = fdio_register_fd(0, NULL,
                                          STDIN_FILENO, FDIO_STRING,
                                          ProcessFdioEvent, NULL,
                                          1024*1024*64 /*=64MB max string length*/,
                                          0,
                                          0,
                                          0,
                                          FDIO_LEN_LITTLE_ENDIAN,
                                          1, 0);
    }
    
    sl_main_loop();
    
    return EC_OK;
}
