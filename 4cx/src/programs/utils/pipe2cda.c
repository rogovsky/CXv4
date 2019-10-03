#include <stdio.h>
#include <unistd.h>
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

//////////////////////////////////////////////////////////////////////

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

static util_refrec_t  bin_ur;
static char           bin_chanref[CDA_PATH_MAX];


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

static void stdin_in_cb(int uniq, void *privptr1,
                        sl_fdh_t fdh, int fd, int mask, void *privptr2)
{
}

static void ProcessFdioEvent(int uniq, void *unsdptr,
                             fdio_handle_t handle, int reason,
                             void *inpkt, size_t inpktsize,
                             void *privptr)
{
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

int main(int argc, char *argv[])
{
  int            c;
  int            err = 0;

    set_signal(SIGPIPE, SIG_IGN);

#ifdef BUILTINS_REGISTRATION_CODE
    BUILTINS_REGISTRATION_CODE
#endif /* BUILTINS_REGISTRATION_CODE */

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
               "   -- read binary data once (any size from 0 to max)\n"
               "\n"
               "c. %s -C CHANNEL-TO-WRITE\n"
               "   -- read binary data continuously till EOF\n"
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
        if (option_cont_bin)
        {
            fprintf(stderr, "%s: sorry, \"-C\" operation isn't supported yet\n", argv[0]);
            exit(EC_ERR);
        }
        ParseDatarefSpec(argv[0], option_bin_name,
                         NULL, bin_chanref, sizeof(bin_chanref), &bin_ur);
        bin_ur.spec = bin_chanref;
        bin_ur.ref = cda_add_chan(the_cid,
                                  option_baseref,
                                  bin_ur.spec, bin_ur.options, bin_ur.dtype, bin_ur.n_items,
                                  0, NULL, NULL);
        if (bin_ur.ref == CDA_DATAREF_ERROR)
        {
            fprintf(stderr, "%s %s: cda_add_chan(\"%s\"): %s\n",
                    strcurtime(), argv[0], bin_ur.spec, cda_last_err());
            exit(EC_ERR);
        }

        binary_fdhandle = sl_add_fd(0, NULL,
                                    STDIN_FILENO, SL_RD,
                                    stdin_in_cb, NULL);
    }
    else
    {
        fprintf(stderr, "%s: sorry, non-\"-B\"/\"-C\" operation isn't supported yet\n", argv[0]);
        exit(EC_ERR);
        /////////////////////////
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
