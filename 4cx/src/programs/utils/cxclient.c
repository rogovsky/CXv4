#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "misc_macros.h"
#include "misclib.h"

#include "cx.h"
#include "cxlib.h"


//////////////////////////////////////////////////////////////////////

typedef struct
{
    int            in_use;
    int            hwid;
    cxdtype_t      dtype;
    int            n_items;
    char           dpyfmt[20];
} hwcrec_t;

static hwcrec_t *hwcrecs_list        = NULL;
static int       hwcrecs_list_allocd = 0;

// GetHwcRecSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, HwcRec, hwcrec_t,
                                 hwcrecs, in_use, 0, 1,
                                 0, 10, 1000,
                                 , , void)

static void RlsHwcRecSlot(int rn)
{
  hwcrec_t *rp = AccessHwcRecSlot(rn);

    rp->in_use = 0;
}

//////////////////////////////////////////////////////////////////////

enum
{
    EC_OK      = 0,
    EC_HELP    = 1,
    EC_USR_ERR = 2,
    EC_ERR     = 3,
};

static int   option_help = 0;

static char *srvref;
static int   conn;


static void evproc(int uniq, void *privptr1,
                   int cd, int reason, const void *info,
                   void *privptr2)
{
  cx_newval_info_t *nvp = info;

//fprintf(stderr, "reason=%d\n", reason);
    if      (reason == CAR_CYCLE)
    {
        if (0) fprintf(stdout, "CYCLE\n");
    }
    else if (reason == CAR_NEWDATA)
    {
        fprintf(stdout, "[%d] v=%d\n", nvp->hwid, *((int32*)(nvp->data)));
    }
}

int main(int argc, char *argv[])
{
  int   c;
  int   err = 0;
  int   saved_errno;

  int            hwid;

  int            nrefs;

  int            rn;
  hwcrec_t      *rp;


    /* Make stdout ALWAYS line-buffered */
    setvbuf(stdout, NULL, _IOLBF, 0);

    set_signal(SIGPIPE, SIG_IGN);

    while ((c = getopt(argc, argv, "h")) != EOF)
        switch (c)
        {
            case 'h':
                option_help = 1;
                break;

            case ':':
            case '?':
                err++;
        }

    if (option_help)
    {
        printf("Usage: %s [OPTIONS] SERVER {COMMANDS}\n"
               "    -h  show this help\n",
               argv[0]);
        exit(EC_HELP);
    }

    if (err)
    {
        fprintf(stderr, "Try '%s -h' for help.\n", argv[0]);
        exit(EC_USR_ERR);
    }

    if (optind >= argc)
    {
        fprintf(stderr, "%s: missing SERVER reference.\n", argv[0]);
        exit(EC_USR_ERR);
    }

    srvref = argv[optind++];

    //////////////////////////////////////////////////////////////////

    conn = cx_open(0, NULL, srvref, 0, argv[0], NULL, evproc, NULL);
    if (conn < 0)
    {
        saved_errno = errno;
        fprintf(stderr, "%s %s: cx_open(\"%s\"): %s\n",
                strcurtime(), argv[0], srvref, cx_strerror(saved_errno));
        exit(EC_ERR);
    }

    cx_begin(conn);
    for (nrefs = 0;  optind < argc;  optind++)
    {
        hwid = atoi(argv[optind]);

        rn = GetHwcRecSlot();
        if (rn >= 0)
        {
            rp = AccessHwcRecSlot(rn);
            
            rp->hwid    = hwid;
            //rp->dtype   = dtype;
            //rp->n_items = n_items;
            //strzcpy(rp->dpyfmt,  dpyfmt,  sizeof(rp->dpyfmt));
            
            //cda_add_dataref_evproc(ref, CDA_REF_EVMASK_UPDATE, ProcessDatarefEvent, lint2ptr(rn));
            cx_rd_cur(conn, 1, &hwid, &rn, NULL);

            nrefs++;
        }
    }
    cx_run(conn);

    cx_close(conn);
    
    return EC_OK;
}
