#include <dlfcn.h>

#include "misc_macros.h"
#include "misclib.h"

#include "cx_sysdeps.h"

#include "v2cxlib_renames.h"
#include "v2datatree_renames.h"
#include "v2Cdr_renames.h"
#include "Knobs_typesP.h"

#include "v2subsysaccess.h"


static char progname[40] = "";

static void reporterror(const char *format, ...)
    __attribute__ ((format (printf, 1, 2)));

static void reporterror(const char *format, ...)
{
  va_list ap;

    va_start(ap, format);
#if 1
    fprintf (stderr, "%s %s%s ",
             strcurtime(), progname, progname[0] != '\0' ? ": " : "");
    vfprintf(stderr, format, ap);
    fprintf (stderr, "\n");
#endif
    va_end(ap);
}

//////////////////////////////////////////////////////////////////////

typedef struct
{
    int             in_use;
    char            subsysname[200];
    void           *handle;
    subsysdescr_t  *info;
    cda_serverid_t  mainsid;
    groupelem_t    *grouplist;
} v2subsys_t;

enum
{
    SUBSYS_MAX       = 0,
    SUBSYS_ALLOC_INC = 2,     // Must be >1 (to provide growth from 0 to 2)
};

static v2subsys_t *subsys_list        = NULL;
static int         subsys_list_allocd = 0;

// GetSubsysSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(static, Subsys, v2subsys_t,
                                 subsys, in_use, 0, 1,
                                 1, SUBSYS_ALLOC_INC, SUBSYS_MAX,
                                 , , void)

static void RlsSubsysSlot(int yid)
{
  v2subsys_t *syp = AccessSubsysSlot(yid);
  int         err = errno;        // To preserve errno

    if (yid < 0  ||  yid >= subsys_list_allocd  ||  syp->in_use == 0) return;

    if (syp->grouplist != NULL)               CdrDestroyGrouplist(syp->grouplist);
    if (syp->mainsid   != CDA_SERVERID_ERROR) /*cda_del_server(syp->mainsid)*/;
    if (syp->handle    != NULL)               dlclose(syp->handle);

    syp->in_use = 0;

    errno = err;
}

//--------------------------------------------------------------------

static int subsysname_checker(v2subsys_t *syp, void *privptr)
{
  const char *subsysname = privptr;

    return strcasecmp(subsysname, syp->subsysname) == 0;
}
static int GetSubsysID(const char *argv0,
                       const char *caller,
                       const char *subsysname)
{
  int         yid;
  v2subsys_t *syp;
  char       *err;

  physinfodb_rec_t *rec;

#if OPTION_HAS_PROGRAM_INVOCATION_NAME /* With GNU libc+ld we can determine the true argv[0] */
    if (progname[0] == '\0') strzcpy(progname, program_invocation_short_name, sizeof(progname));
#endif /* OPTION_HAS_PROGRAM_INVOCATION_NAME */

    /* Check if this subsystem is already loaded */
    yid = ForeachSubsysSlot(subsysname_checker, subsysname);
    if (yid >= 0) return yid;

    /* No, should load... */

    /* First, allocate... */
    yid = GetSubsysSlot();
    if (yid < 0)
    {
        reporterror("%s: unable to allocate subsys-slot", caller);
        return -1;
    }
    syp = AccessSubsysSlot(yid);
    strzcpy(syp->subsysname, subsysname, sizeof(syp->subsysname));
    syp->mainsid = CDA_SERVERID_ERROR;

    /* ...than open... */
    if (CdrOpenDescription(subsysname, argv0, &(syp->handle), &(syp->info), &err) != 0)
    {
        reporterror("%s: OpenDescription(\"%s\"): %s",
                    caller, subsysname, err);
        goto ERREXIT;
    }
    /* ...and use */
    syp->mainsid = cda_new_server(syp->info->defserver,
                                  NULL, lint2ptr(yid),
                                  CDA_REGULAR);
    if (syp->mainsid == CDA_SERVERID_ERROR)
    {
        reporterror("%s: cda_new_server(\"%s\"): %s",
                    caller, syp->info->defserver, cx_strerror(errno));
        goto ERREXIT;
    }
    if (syp->info->phys_info_count < 0)
    {
        stripped_cda_register_physinfo_dbase(syp->mainsid,
                                             (physinfodb_rec_t *)(syp->info->phys_info));
        // ...
        for (rec = (physinfodb_rec_t *)(syp->info->phys_info);
             rec != NULL  &&  rec->srv != NULL;
             rec++)
            if (strcasecmp(syp->info->defserver, rec->srv) == 0)
            {
                cda_set_physinfo(syp->mainsid, rec->info, rec->count);
            }
    }
    else
    {
        cda_set_physinfo(syp->mainsid, syp->info->phys_info, syp->info->phys_info_count);
    }
    syp->grouplist = CdrCvtGroupunits2Grouplist(syp->mainsid, syp->info->grouping);
    if (syp->grouplist == NULL)
    {
        reporterror("%s: CdrCvtGroupunits2Grouplist(): %s",
                    caller, cx_strerror(errno));
        goto ERREXIT;
    }

    return yid;

 ERREXIT:
    RlsSubsysSlot(yid);

    return -1;
}

//--------------------------------------------------------------------

int v2subsysaccess_resolve(const char *argv0,
                           const char *name,
                           char *srvnamebuf, size_t srvnamebufsize,
                           int *chan_n, int *color_p,
                           double *phys_r_p, double *phys_d_p, int *phys_q_p,
                           v2subsysaccess_strs_t *strs_p)
{
  const char     *dot_p;
  const char     *at_p;
  const char     *k_name;
  char            subsysname[200];
  size_t          subsysnamelen;

  int             yid;
  v2subsys_t     *syp;

  Knob            k;
  int             r;
  const char     *phys_srvname;

  int             r_type;
  int             r_nn;
  char           *err;
  char            namebuf[1000];
  size_t          namelen;
  int             physhandle;
  excmd_t        *cmdp;
  int             ref_n;

    /* Perform checks */
    if (name == NULL)
    {
        reporterror("%s: NULL channel request", __FUNCTION__);
        return -1;
    }
    if (*name == '\0')
    {
        reporterror("%s: empty channel request", __FUNCTION__);
        return -1;
    }
    dot_p = strchr(name, '.');
    if (dot_p == NULL)
    {
        reporterror("%s: '.'-less channel request \"%s\"", __FUNCTION__, name);
        return -1;
    }

    k_name = dot_p + 1;

    /* Obtain subsys name */
    subsysnamelen = dot_p - name;
    if (subsysnamelen > sizeof(subsysname) - 1)
        subsysnamelen = sizeof(subsysname) - 1;
    memcpy(subsysname, name, subsysnamelen); subsysname[subsysnamelen] = '\0';

    /* ...and reference */
    yid = GetSubsysID(argv0, __FUNCTION__, subsysname);
    if (yid < 0) return -1;
    syp = AccessSubsysSlot(yid);

    /*  */
    at_p = strchr(k_name, '@');
    if (at_p != NULL)
    {
        r_type = tolower(at_p[1]);
        if (r_type == 'r'  ||  r_type == 'w'  ||  r_type == 'c');
        else
        {
            reporterror("%s: invalid @-kind-specifier \"@%c\"",
                        __FUNCTION__, at_p[1]);
            return -1;
        }
        r_nn = strtol(at_p + 2, &err, 10);
        if (err == at_p + 2  ||  *err != '\0')
        {
            reporterror("%s: syntax error after \"@%c\" in \"%s\"",
                        __FUNCTION__, at_p[1], name);
            return -1;
        }

        namelen = at_p - k_name;
        if (namelen == 0)
        {
            reporterror("%s: empty knob name in \"%s\"",
                        __FUNCTION__, name);
            return -1;
        }
        if (namelen > sizeof(namebuf) - 1)
        {
            reporterror("%s: name \"%s\" too long",
                        __FUNCTION__, k_name);
            return -1;
        }
        memcpy(namebuf, k_name, namelen); namebuf[namelen] = '\0';
        k_name = namebuf;
    }
    else r_type = 0;

    k = datatree_FindNode(syp->grouplist, k_name);
    if (k == NULL)
    {
        reporterror("%s: node \"%s\" not found",
                    __FUNCTION__, name);
        return -1;
    }
    if (k->type == LOGT_SUBELEM)
    {
        reporterror("%s: attempt to use subelem (\"%s\")",
                    __FUNCTION__, name);
        return -1;
    }
#if 1
    if (r_type == 0)
    {
        if (k->kind != LOGK_DIRECT)
        {
            reporterror("%s: attempt to use non-LOGK_DIRECT knob (\"%s\")",
                        __FUNCTION__, name);
            return -1;
        }
        physhandle = k->physhandle;
    }
    else if (r_type == 'r'  ||  r_type == 'w'  ||  r_type == 'c')
    {
        if      (r_type == 'r') cmdp = k->formula;
        else if (r_type == 'w') cmdp = k->revformula;
        else                    cmdp = k->colformula;

        for (ref_n = 0, physhandle = -1;
             cmdp != NULL  &&  (cmdp->cmd & OP_code) != OP_RET;
             cmdp++)
            if (cmdp->cmd == OP_GETP_I  ||  cmdp->cmd == OP_SETP_I)
            {
                if (ref_n == r_nn)
                    physhandle = cmdp->arg.handle;
                ref_n++;
            }

        if (physhandle < 0)
        {
            reporterror("%s: chanref @%c%d not found in knob \"%s\"",
                        __FUNCTION__, r_type, r_nn, k_name);
            return -1;
        }
    }

    r = cda_srcof_physchan(physhandle, &phys_srvname, chan_n);
    if (r < 0) return r;
    if (srvnamebufsize > 0) strzcpy(srvnamebuf, phys_srvname, srvnamebufsize);
    if (color_p != NULL) *color_p = k->color;
    cda_getphyschan_rd(physhandle, phys_r_p, phys_d_p);
    stripped_cda_getphyschan_q_int(physhandle, phys_q_p);
    if (strs_p != NULL)
    {
        strs_p->ident   = k->ident;
        strs_p->label   = k->label;
        strs_p->tip     = k->comment;
        strs_p->comment = NULL;
        strs_p->geoinfo = NULL;
        strs_p->rsrvd6  = NULL;
        strs_p->units   = k->units;
        strs_p->dpyfmt  = k->dpyfmt;
    }
#else
    if (k->kind != LOGK_DIRECT)
    {
        reporterror("%s: attempt to use non-LOGK_DIRECT knob (\"%s\")",
                    __FUNCTION__, name);
        return -1;
    }

    r = cda_srcof_physchan(k->physhandle, &phys_srvname, chan_n);
    if (r < 0) return r;
    if (srvnamebufsize > 0) strzcpy(srvnamebuf, phys_srvname, srvnamebufsize);
    cda_getphyschan_rd(k->physhandle, phys_r_p, phys_d_p);
#endif

    return 0;
}
