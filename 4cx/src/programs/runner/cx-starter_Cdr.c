#include <errno.h>

#include "misc_macros.h"

#include "datatreeP.h"
#include "CdrP.h"

#include "cx-starter_msg.h"
#include "cx-starter_Cdr.h"


// GetSubsysSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(, Subsys, subsys_t,
                                 sys, in_use, 0, 1,
				 0, 10, 0,
				 sl->, sl,
				 subsyslist_t *sl, subsyslist_t *sl)

void RlsSubsysSlot(int id, subsyslist_t *sl)
{
  subsys_t *sr = AccessSubsysSlot(id, sl);

    CdrDestroySubsystem(sr->ds);
    sr->in_use = 0;
}

#define CLEVER_STRDUP(dst, src) do {if(dst == NULL  &&  src != NULL) dst = strdup(src);} while (0)
static int SubsysAdder(cfgline_t *lp, void *privptr)
{
  subsyslist_t *sl = privptr;
  int           id;
  subsys_t     *sr;

  const char   *cp;
  const char   *src;
  char         *dp;
  int           ci_count;
  size_t        fla_size;

  data_section_t *nsp;
  DataKnob        grp;
  DataKnob        k;

  static const char *fla_beg_s = "#!fla\n_all_code;\n";
  static const char *ref_pfx_s = "getchan \"";
  static const char *ref_sfx_s = "\"; pop;";
  static const char *fla_end_s = "ret 0;\n";

    id = GetSubsysSlot(sl);
    if (id < 0) return -1;
    sr = AccessSubsysSlot(id, sl);

    if (lp->kind == SUBSYS_CX)
    {
        if (lp->chaninfo == NULL)
        {
            sr->ds = CdrLoadSubsystem("file", lp->name, NULL);
            if (sr->ds == NULL)
            {
                reportinfo("unable to CdrLoadSubsystem(\"%s\"): %s",
                           lp->name, CdrLastErr());
                goto FINISH; /* Loading failure is NOT fatal, just the button will be grayed out */
            }
            CLEVER_STRDUP(lp->app_name, CdrFindSection(sr->ds, DSTN_WINNAME,  "main"));
            CLEVER_STRDUP(lp->comment,  CdrFindSection(sr->ds, DSTN_WINTITLE, "main"));
        }
        else /*!!! chaninfo=... */
        {
            sr->ds = malloc(sizeof(*(sr->ds)));
            if (sr->ds == NULL)
            {
                reportinfo("unable to malloc(subsystem) for \"%s\": %s",
                           lp->name, strerror(errno));
                goto FINISH; /* Failure is NOT fatal, just the button will be grayed out */
            }
            bzero(sr->ds, sizeof(*(sr->ds)));

            /* Count requested channels */
            for (ci_count = 1, cp = lp->chaninfo;  *cp != '\0';  cp++)
                if (*cp == ',') ci_count++;
            /* ...and size of formula */
            fla_size = strlen(fla_beg_s) +
                       (strlen(ref_pfx_s) + strlen(ref_sfx_s)) * ci_count +
                       strlen(lp->chaninfo) +
                       strlen(fla_end_s) + 1;

            /* Allocate subsystem */
            CdrCreateSection(sr->ds, DSTN_SYSNAME,
                             NULL, 0,
                             lp->name, strlen(lp->name) + 1);
            nsp = CdrCreateSection(sr->ds,
                                   DSTN_GROUPING,
                                   "main", strlen("main"),
                                   NULL, sizeof(data_knob_t));
            sr->ds->sysname       = CdrFindSection(sr->ds, DSTN_SYSNAME,   NULL);
            sr->ds->main_grouping = CdrFindSection(sr->ds, DSTN_GROUPING,  "main");
            grp                   = nsp->data;
            grp->type             = DATAKNOB_GRPG;
            grp->u.c.content = k  = malloc(sizeof(data_knob_t));
            if (grp->u.c.content == NULL)
            {
                reportinfo("unable to malloc(content) for \"%s\": %s",
                           lp->name, strerror(errno));
                CdrDestroySubsystem(sr->ds); sr->ds = NULL;
                goto FINISH; /* Failure is NOT fatal, just the button will be grayed out */
            }
            bzero(k, sizeof(*k));
            grp->u.c.count        = 1;
            k->type   = DATAKNOB_KNOB;
            k->uplink = grp;

            /* Allocate formula... */
            if ((k->u.k.rd_src = dp = malloc(fla_size)) == NULL)
            {
                reportinfo("unable to malloc(rd_src) for \"%s\": %s",
                           lp->name, strerror(errno));
                CdrDestroySubsystem(sr->ds); sr->ds = NULL;
                goto FINISH; /* Failure is NOT fatal, just the button will be grayed out */
            }
            /* ...and fill it */
            strcpy(dp, fla_beg_s); dp += strlen(fla_beg_s);
            for (src = lp->chaninfo;  *src != '\0'; /*NO-OP*/)
            {
                while (*src != '\0'  &&  isspace(*src)) src++;
                cp = strchr(src, ',');
                if (cp == NULL) cp = src + strlen(src);

                strcpy(dp, ref_pfx_s); dp += strlen(ref_pfx_s);
                if (cp > src)
                {
                    memcpy(dp, src, cp - src);  dp += cp - src;
                }
                strcpy(dp, ref_sfx_s); dp += strlen(ref_sfx_s);

                src = cp;
                if (*src == ',') src++;
            }
            strcpy(dp, fla_end_s); dp += strlen(fla_end_s);
        }
        sr->ds->is_freezed = lp->noprocess;
        CdrSetSubsystemRO(sr->ds, 1);
    }

 FINISH:;
    sr->cfg = *lp;

    return 0;
}
subsyslist_t *CreateSubsysListFromCfgBase(cfgbase_t *cfg)
{
  subsyslist_t *sl;
  int           r;
  int           saved_errno;

    sl = malloc(sizeof(*sl));
    if (sl == NULL) return NULL;
    bzero(sl, sizeof(*sl));

    r = ForeachCfglSlot(SubsysAdder, sl, cfg);
    if (r >= 0)
    {
        saved_errno = errno;
        DestroySubsysList(sl);
	errno = saved_errno;
        return NULL;
    }

    return sl;
}

typedef struct
{
    subsyslist_t         *sl;
    cda_context_evproc_t  evproc;
} realize_info_t;
static int SubsysRealizer(subsys_t *sr, void *privptr)
{
  realize_info_t *info_p = privptr;

    if (sr->ds != NULL)
    {
        if (CdrRealizeSubsystem(sr->ds,
                                CDA_CONTEXT_OPT_NO_OTHEROP | 
                                ((sr->cfg.noprocess)? CDA_CONTEXT_OPT_IGN_UPDATE
                                                    : 0),
                                NULL, NULL) < 0)
        {
            sr->ds = NULL;
            CdrDestroySubsystem(sr->ds);
        }
        else if (info_p->evproc != NULL)
            cda_add_context_evproc(sr->ds->cid, 
                                   ((sr->cfg.noprocess)? 0
                                                       : CDA_CTX_EVMASK_CYCLE) |
                                       CDA_CTX_EVMASK_NEWSRV,
                                   info_p->evproc,
                                   lint2ptr(sr - info_p->sl->sys_list/*!!! HACK!!! */));
    }

    return 0;
}
int           RealizeSubsysList          (subsyslist_t *sl,
                                          cda_context_evproc_t evproc)
{
  realize_info_t  info;

    info.sl     = sl;
    info.evproc = evproc;

    ForeachSubsysSlot(SubsysRealizer, &info, sl);
    return 0;
}

static int TermIterator(subsys_t *sr, void *privptr)
{
  subsyslist_t *sl = privptr;
  int           id = sr - sl->sys_list;

    RlsSubsysSlot(id, sl);
    return 0;
}
void          DestroySubsysList          (subsyslist_t *sl)
{
    if (sl == NULL) return;

    ForeachSubsysSlot(TermIterator, sl, sl);
    safe_free(sl);
}
