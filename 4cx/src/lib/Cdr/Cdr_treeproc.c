#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include <math.h>

#include "misc_macros.h"
#include "misclib.h"

#include "cxlib.h" // for cx_strrflag_short()
#include "datatree.h"
#include "datatreeP.h"
#include "cda.h"
#include "Cdr.h"
#include "CdrP.h"
#include "Cdr_plugmgr.h"

#ifndef NAN
  #warning The NAN macro is undefined, using strtod("NAN") instead
  #define NAN strtod("NAN", NULL)
#endif


enum {NUMAVG = 30};
enum {MMXCAP = 30};


data_section_t *CdrCreateSection(DataSubsys sys,
                                 const char *type,
                                 char       *name, size_t namelen,
                                 void       *data, size_t datasize)
{
  data_section_t *nsp;
  data_section_t *new_sections;

    /* Grow the 'sections' array */
    new_sections = safe_realloc(sys->sections,
                                sizeof(data_section_t)
                                *
                                (sys->num_sections + 1));
    if (new_sections == NULL)
        return NULL;
    sys->sections = new_sections;

    /* Okay -- a new section slot, let's initialize it */
    nsp = new_sections + sys->num_sections;
    bzero(nsp, sizeof(*nsp));
    nsp->type = type;

    /* Allocate and fill the name */
    if (namelen != 0)
    {
        nsp->name = malloc(namelen + 1);
        if (nsp->name == NULL)
            return NULL;
        memcpy(nsp->name, name, namelen);
        nsp->name[namelen] = '\0';
    }

    /* Allocate data, if required */
    if (datasize != 0)
    {
        nsp->data = malloc(datasize);
        if (nsp->data == NULL)
            return NULL;

        if (data != NULL)
            memcpy(nsp->data, data, datasize);
        else
            bzero (nsp->data,       datasize);
    }
    else
        nsp->data = data;

    /* Reflect the existence of new section */
    sys->num_sections += 1;

    return sys->sections + sys->num_sections - 1;
}

DataSubsys  CdrLoadSubsystem(const char *def_scheme,
                             const char *reference,
                             const char *argv0)
{
  DataSubsys          ret;
  char                scheme[20];
  const char         *location;
  CdrSubsysLdrModRec *metric;
  
    CdrClearErr();

    if (def_scheme != NULL  &&  *def_scheme == '!')
    {
        strzcpy(scheme, def_scheme + 1, sizeof(scheme));
        location = reference;
    }
    else
        split_url(def_scheme, "::",
                  reference, scheme, sizeof(scheme),
                  &location);

    metric = CdrGetSubsysLdrByScheme(scheme);

    if (metric == NULL)
    {
        CdrSetErr("unknown scheme \"%s\"", scheme);
        return NULL;
    }

    ret = metric->ldr(location, argv0);
    if (ret == NULL) return NULL;

    if (CdrFindSection(ret, DSTN_SYSNAME, NULL) == NULL)
        CdrCreateSection(ret, DSTN_SYSNAME,
                         NULL, 0,
                         location, strlen(location) + 1);
    
    ret->sysname       = CdrFindSection(ret, DSTN_SYSNAME,   NULL);
    ret->main_grouping = CdrFindSection(ret, DSTN_GROUPING,  "main");
    ret->defserver     = CdrFindSection(ret, DSTN_DEFSERVER, NULL);
    ret->treeplace     = CdrFindSection(ret, DSTN_TREEPLACE, NULL);
    
    return ret;
}


void     *CdrFindSection    (DataSubsys subsys, const char *type, const char *name)
{
  int  n;
    
    for (n = 0;  n < subsys->num_sections;  n++)
        if (
            (strcasecmp(type, subsys->sections[n].type) == 0)
            &&
            (name == NULL  ||
             strcasecmp(name, subsys->sections[n].name) == 0)
           )
            return subsys->sections[n].data;

    return NULL;
}

DataKnob  CdrGetMainGrouping(DataSubsys subsys)
{
    if (subsys == NULL) return NULL;
    return subsys->main_grouping;
}

void  CdrDestroySubsystem(DataSubsys subsys)
{
  int             n;
  data_section_t *p;
    
    if (subsys == NULL) return;

    for (n = 0, p = subsys->sections;
         n < subsys->num_sections;
         n++,   p++)
    {
        if      (p->destroy != NULL)
            p->destroy(p);
        else if (strcasecmp(p->type, DSTN_GROUPING) == 0)
            CdrDestroyKnobs(p->data, 1), p->data = NULL;

        /* DSTN_DEFSERVER and DSTN_TREEPLACE are just free()'d */
        
        safe_free(p->name);  p->name = NULL;
        safe_free(p->data);  p->data = NULL;
    }

    /*!!! cda_del_context(subsys->cid); */

    free(subsys);
}

void  CdrDestroyKnobs(DataKnob list, int count)
{
  int       n;
  DataKnob  k;
  int       pn;
  int       rn;

  DataSubsys  subsys;
  DataKnob    p1;
  DataKnob    p2;
    
    if (list == NULL  ||  count <= 0) return;

    for (n = 0, k = list;
         n < count;
         n++,   k++)
    {
        /* First, do type-dependent things... */
        switch (k->type)
        {
            case DATAKNOB_GRPG:
                /* Fallthrough to CONT */
            case DATAKNOB_CONT:
                CdrDestroyKnobs(k->u.c.content, k->u.c.count);
                safe_free(k->u.c.content); k->u.c.content = NULL; k->u.c.count = 0;

                safe_free(k->u.c.refbase); k->u.c.refbase = NULL;
                safe_free(k->u.c.str1);    k->u.c.str1    = NULL;
                safe_free(k->u.c.str2);    k->u.c.str2    = NULL;
                safe_free(k->u.c.str3);    k->u.c.str3    = NULL;

                safe_free(k->u.c.at_init_src); k->u.c.at_init_src = NULL;
                safe_free(k->u.c.ap_src);  k->u.c.ap_src  = NULL;
                /*!!! cda_del_chan() references? W/prelim cda_del_chan_evproc()? */
                break;
                
            case DATAKNOB_NOOP:
                /* Nothing to do */
                break;
                
            case DATAKNOB_KNOB:
                /*!!!*/
                safe_free(k->u.k.units);   k->u.k.units   = NULL;
                safe_free(k->u.k.items);   k->u.k.items   = NULL;
                safe_free(k->u.k.rd_src);  k->u.k.rd_src  = NULL;
                safe_free(k->u.k.wr_src);  k->u.k.wr_src  = NULL;
                safe_free(k->u.k.cl_src);  k->u.k.cl_src  = NULL;

                for (pn = 0;  pn < k->u.k.num_params; pn++)
                {
                    safe_free(k->u.k.params[pn].ident);
                    safe_free(k->u.k.params[pn].label);
                }
                safe_free(k->u.k.params);  k->u.k.params  = NULL; k->u.k.num_params = 0;

                /* Do kind-dependent things */
                if      (k->u.k.kind == DATAKNOB_KIND_DEVN)
                {
                    /* Do-nothing */
                }
                else if (k->u.k.kind == DATAKNOB_KIND_MINMAX)
                {
                    safe_free(k->u.k.kind_var.minmax.buf);
                    k->u.k.kind_var.minmax.buf = NULL;
                }

                if (k->u.k.histring != NULL)
                {
                    safe_free(k->u.k.histring); k->u.k.histring = NULL;
                    k->u.k.histring_len = 0;
                    subsys = get_knob_subsys(k);
                    if (subsys != NULL)
                    {
                        p1 = k->u.k.hist_prev;
                        p2 = k->u.k.hist_next;

                        if (p1 == NULL) subsys->hist_first = p2; else p1->u.k.hist_next = p2;
                        if (p2 == NULL) subsys->hist_last  = p1; else p2->u.k.hist_prev = p1;
                    }
                    k->u.k.hist_prev = k->u.k.hist_next = NULL;
                }
                break;
                
            case DATAKNOB_BIGC:
                /*!!! cda_del_chan() references? W/prelim cda_del_chan_evproc()? */
                break;

            case DATAKNOB_TEXT:
                safe_free(k->u.t.src);
                break;

            case DATAKNOB_USER:
                for (rn = 0;  rn < k->u.u.num_datarecs;  rn++)
                {
                    if (cda_ref_is_sensible(k->u.u.datarecs[n].dr)) /*!!!*/;
                }
                safe_free(k->u.u.datarecs); k->u.u.datarecs = NULL; k->u.u.num_datarecs = 0;
                for (rn = 0;  rn < k->u.u.num_bigcrecs;  rn++)
                {
                    /*!!! cda_del_chan() references? W/prelim cda_del_chan_evproc()? */
                }
                safe_free(k->u.u.bigcrecs); k->u.u.bigcrecs = NULL; k->u.u.num_bigcrecs = 0;
                break;

            case DATAKNOB_PZFR:
                safe_free(k->u.z.tree_base);
                safe_free(k->u.z.src);
                /*!!! cda_del_chan(k->u.z._devstate_ref); */
                break;
        }

        /* ...and than common ones */
        safe_free(k->look);    k->look    = NULL;
        safe_free(k->options); k->options = NULL;
        safe_free(k->ident);   k->ident   = NULL;
        safe_free(k->label);   k->label   = NULL;
        safe_free(k->tip);     k->tip     = NULL;
        safe_free(k->comment); k->comment = NULL;
        safe_free(k->style);   k->style   = NULL;
        safe_free(k->layinfo); k->layinfo = NULL;
        safe_free(k->geoinfo); k->geoinfo = NULL;
        safe_free(k->conns_u); k->conns_u = NULL;
    }
}


void  CdrSetSubsystemRO  (DataSubsys  subsys, int ro)
{
    subsys->readonly = (ro != 0);
}


enum
{
    REALIZE_PASS_1 = 0,
    REALIZE_PASS_2 = 1,
};

static void FillConnsOfContainer(int        numsrvs,
                                 DataKnob   node,
                                 uint8     *upper_conns_u,
                                 int        pass_n)
{
  size_t    conns_u_size = numsrvs * sizeof(node->conns_u[0]);
  int       x;
  int       n;
  DataKnob  k;

    if (numsrvs <= 0) return;
    if (node->type != DATAKNOB_GRPG  &&  node->type != DATAKNOB_CONT) return;

    /* Allocate array at the start of pass 1 */
    if (pass_n == REALIZE_PASS_1)
    {
        if ((node->conns_u = malloc(conns_u_size)) == NULL) return;
        bzero(node->conns_u, conns_u_size);
    }
    else if (node->conns_u == NULL) return; /*!!!??? Sure? Not in v2*/

    /* Walk through content */
    for (k = node->u.c.content, n = 0;  n < node->u.c.count;  k++, n++)
    {
        if      (k->type == DATAKNOB_GRPG  ||
                 k->type == DATAKNOB_CONT)
        {
            FillConnsOfContainer(numsrvs, k, node->conns_u, pass_n);
        }
        else if (pass_n == REALIZE_PASS_2)
        {
            /* Don't gather at pass 2 */
        }
        else if (k->type == DATAKNOB_KNOB)
        {
            if (cda_ref_is_sensible(k->u.k.rd_ref))
                cda_srvs_of_ref    (k->u.k.rd_ref, node->conns_u, conns_u_size);
            if (cda_ref_is_sensible(k->u.k.cl_ref))
                cda_srvs_of_ref    (k->u.k.cl_ref, node->conns_u, conns_u_size);
        }
        else if (k->type == DATAKNOB_TEXT)
        {
            if (cda_ref_is_sensible(k->u.t.ref))
                cda_srvs_of_ref    (k->u.t.ref,    node->conns_u, conns_u_size);
        }
        else if (k->type == DATAKNOB_BIGC)
        {
            /*!!!???*/
        }
        else if (k->type == DATAKNOB_USER)
        {
            /*!!!???*/
        }
        else if (k->type == DATAKNOB_PZFR)
        {
            /* No need to do anything: PZFRs are on theirselves */
        }
    }
    /* ...and add our at-proc too */
    if (cda_ref_is_sensible(node->u.c.ap_ref))
        cda_srvs_of_ref    (node->u.c.ap_ref, node->conns_u, conns_u_size);

    /* Add our used-srvs upwards */
    if (upper_conns_u != NULL)
        for (x = 0;  x < numsrvs;  x++) upper_conns_u[x] |= node->conns_u[x];

    /* Perform checks at the end of pass 2 */
    if (pass_n == REALIZE_PASS_2)
    {
        for (x = 0;  x >= 0  &&  x < numsrvs;  x++)
            if (node->conns_u[x]) x = -999; /* Note: NOT -1, since (-1)+1 would result in 0 */

        if (x > 0 /* x>0 means that no sids-used-by this-hierarchy found */
            ||
            (
             node->uplink          != NULL  &&
             node->uplink->conns_u != NULL  &&
             memcmp(node->conns_u, node->uplink->conns_u, conns_u_size) == 0
            )
           )
        {
            safe_free(node->conns_u);
            node->conns_u = NULL;
        }
    }
}

static void ProcessContextEvent(int            uniq,
                                void          *privptr1,
                                cda_context_t  cid,
                                int            reason,
                                int            info_int,
                                void          *privptr2)
{
  DataSubsys      subsys = privptr1;

//fprintf(stderr, "%s()\n", __FUNCTION__);
    switch (reason)
    {
        case CDA_CTX_R_CYCLE:
            if (subsys->is_freezed == 0)
                CdrProcessSubsystem(subsys, info_int, 0, &(subsys->currflags));
            break;
    }
}

static void CallAtInitOfKnobs(DataKnob list, int count, int cda_opts)
{
  int       n;
  DataKnob  k;
    
    if (list == NULL  ||  count <= 0) return;

    for (n = 0, k = list;
         n < count;
         n++,   k++)
    {
        switch (k->type)
        {
            case DATAKNOB_GRPG:
                /* Fallthrough to CONT */
            case DATAKNOB_CONT:
                if (cda_ref_is_sensible(k->u.c.at_init_ref))
                    cda_process_ref(k->u.c.at_init_ref,
                                    CDA_OPT_WR_FLA | CDA_OPT_DO_EXEC | cda_opts,
                                    0.0,
                                    NULL, 0);
                CallAtInitOfKnobs(k->u.c.content, k->u.c.count, cda_opts);
                break;
        }
    }
}
int   CdrRealizeSubsystem(DataSubsys  subsys,
                          int         cda_ctx_options,
                          const char *defserver,
                          const char *argv0)
{
  int             n;
  data_section_t *p;
  int             r;
  int             cda_opts = (subsys->readonly)? CDA_OPT_READONLY : 0;
  int             numsrvs;

  static const char FROM_CMDLINE_STR[] = "from_cmdline";

    CdrClearErr();

    if (subsys == NULL) return -1;

    if (defserver != NULL  &&  *defserver != '\0')
    {
        p = CdrCreateSection(subsys,
                             DSTN_DEFSERVER,
                             FROM_CMDLINE_STR, strlen(FROM_CMDLINE_STR),
                             defserver, strlen(defserver) + 1);
        if (p == NULL) return -1;
        subsys->defserver = p->data;
    }

    /* 0. Create context */
    subsys->cid = cda_new_context(0, subsys,
                                  subsys->defserver, cda_ctx_options,
                                  argv0,
                                  CDA_CTX_EVMASK_CYCLE, ProcessContextEvent, NULL);
    if (subsys->cid < 0)
    {
        CdrSetErr("cda_new_context(): %s", cda_last_err());
        return -1;
    }

    /* 1st stage -- perform binding */
    for (n = 0, p = subsys->sections;
         n < subsys->num_sections;
         n++,   p++)
        if (strcasecmp(p->type, DSTN_GROUPING) == 0)
        {
            r = CdrRealizeKnobs(subsys,
                                "",
                                p->data, 1);
            if (r < 0) return r;
        }
    /* 2nd stage -- trigger all at_init_ref's */
    for (n = 0, p = subsys->sections;
         n < subsys->num_sections;
         n++,   p++)
        if (strcasecmp(p->type, DSTN_GROUPING) == 0)
            CallAtInitOfKnobs(p->data, 1, cda_opts);
    /* 3rd stage -- determine "change points" */
    numsrvs = cda_status_srvs_count(subsys->cid);
    for (n = 0, p = subsys->sections;
         n < subsys->num_sections;
         n++,   p++)
        if (strcasecmp(p->type, DSTN_GROUPING) == 0)
        {
            FillConnsOfContainer(numsrvs, p->data, NULL, REALIZE_PASS_1);
            FillConnsOfContainer(numsrvs, p->data, NULL, REALIZE_PASS_2);
        }

    return 0;
}

static cda_dataref_t cvt2ref(DataSubsys     subsys,
                             const char    *curbase,
                             const char    *src,
                             int            rw,
                             int            behaviour,
                             CxKnobParam_t *params,
                             int            num_params,
                             int                   evmask,
                             cda_dataref_evproc_t  evproc,
                             void                 *privptr2)
{
  enum
  {
      SRC_IS_CHN = 1,
      SRC_IS_REG = 2,
      SRC_IS_FLA = 3,
  }           srctype;
  const char *p;
  int         is_chanref_char;
  int         options = CDA_DATAREF_OPT_NONE;
  cxdtype_t   dtype   = CXDTYPE_DOUBLE;
  const char *chn;
  char        ut_char;

    if (src == NULL  ||  *src == '\0') return CDA_DATAREF_NONE;

    if      (*src == '%')
        srctype = SRC_IS_REG;
    else if (*src == '#')
        srctype = SRC_IS_FLA;
    else if (*src == '=')
        srctype = SRC_IS_CHN;
    else
    {
        for (p = src, srctype = SRC_IS_CHN;
             *p != '\0';
             p++)
        {
            is_chanref_char =
                isalnum(*p)  ||  *p == '_'  ||  *p == '-'  ||
                *p == '.'  ||  *p == ':'  ||  *p == '@';
            if (!is_chanref_char)
            {
                srctype = SRC_IS_FLA;
                break;
            }
        }
    }

    if      (srctype == SRC_IS_CHN)
    {
        chn = src;
        if (*chn == '=') chn++;
        if (*chn == '@')
        {
            chn++;

            while (1)
            {
                if      (*chn == '-') options |= CDA_DATAREF_OPT_PRIVATE;
                else if (*chn == '.') options |= CDA_DATAREF_OPT_NO_RD_CONV;
                else if (*chn == '/') options |= CDA_DATAREF_OPT_SHY;
                else if (*chn == ':') {chn++; goto END_FLAGS;}
                else break;
                chn++;
            }

            ut_char = tolower(*chn);
            if      (ut_char == 'b') dtype = CXDTYPE_INT8;
            else if (ut_char == 'h') dtype = CXDTYPE_INT16;
            else if (ut_char == 'i') dtype = CXDTYPE_INT32;
            else if (ut_char == 'q') dtype = CXDTYPE_INT64;
            else if (ut_char == 's') dtype = CXDTYPE_SINGLE;
            else if (ut_char == 'd') dtype = CXDTYPE_DOUBLE;
            else if (ut_char == '\0')
            {
                fprintf(stderr, "%s %s: data-type expected after '@' in \"%s\" spec\n",
                        strcurtime(), __FUNCTION__, src);
                return CDA_DATAREF_NONE;
            }
            else
            {
                fprintf(stderr, "%s %s: invalid channel-data-type after '@' in \"%s\" spec\n",
                        strcurtime(), __FUNCTION__, src);
                return CDA_DATAREF_NONE;
            }
            chn++;
            if (*chn != ':')
            {
                fprintf(stderr, "%s %s: ':' expected after \"@%c\" in \"%s\" spec\n",
                        strcurtime(), __FUNCTION__, ut_char, src);
                return CDA_DATAREF_NONE;
            }
            else
                chn++;
        }
 END_FLAGS:;

        if ((behaviour & DATAKNOB_B_ON_UPDATE) != 0)
            options |= CDA_DATAREF_OPT_ON_UPDATE;

        return cda_add_chan   (subsys->cid, curbase,
                               chn, options, dtype, 1,
                               evmask, evproc, privptr2);
    }
    else if (srctype == SRC_IS_REG)
        return cda_add_varchan(subsys->cid, src + 1);
    else if (srctype == SRC_IS_FLA)
        return cda_add_formula(subsys->cid, curbase,
                               src, rw? CDA_OPT_WR_FLA : CDA_OPT_RD_FLA,
                               params, num_params,
                               0, NULL, NULL);
    else return CDA_DATAREF_NONE;
}

static cda_dataref_t src2ref(DataKnob k,
                             const char    *src_name,
                             DataSubsys     subsys,
                             const char    *curbase,
                             const char    *src,
                             int            rw,
                             int            behaviour,
                             CxKnobParam_t *params,
                             int            num_params,
                             int                   evmask,
                             cda_dataref_evproc_t  evproc,
                             void                 *privptr2)
{
  cda_dataref_t  ref = cvt2ref(subsys, curbase,
                               src, rw, behaviour,
                               params, num_params,
                               evmask, evproc, privptr2);

    if (ref == CDA_DATAREF_ERROR)
        fprintf(stderr, "%s \"%s\".%s: %s\n",
                strcurtime(), k->ident, src_name, cda_last_err());

    /*!!! No, should also check somehow else for bad results --
     by CXCF_FLAG_NOTFOUND */

    return ref;
}

static cda_dataref_t txt2ref(DataKnob k,
                             const char    *src_name,
                             DataSubsys     subsys,
                             const char    *curbase,
                             const char    *src,
                             int            rw,
                             CxKnobParam_t *params,
                             int            num_params,
                             int                   evmask,
                             cda_dataref_evproc_t  evproc,
                             void                 *privptr2)
{
  cda_dataref_t  ref;
  int         options = CDA_DATAREF_OPT_NONE;
  cxdtype_t   dtype   = CXDTYPE_TEXT;
  int         nelems  = 10;
  const char *chn;
  char        ut_char;
  char       *endptr;

    chn = src;
    if (*chn == '=') chn++;
    if (*chn == '@')
    {
        chn++;

        while (1)
        {
            if      (*chn == '-') options |= CDA_DATAREF_OPT_PRIVATE;
            else if (*chn == '.') options |= CDA_DATAREF_OPT_NO_RD_CONV;
            else if (*chn == '/') options |= CDA_DATAREF_OPT_SHY;
            else if (*chn == ':') {chn++; goto END_FLAGS;}
            else break;
            chn++;
        }

        // Optional 't'
        ut_char = tolower(*chn);
        if      (ut_char == 't') chn++;
        else if (isdigit(ut_char)) /* Fallthrough to COUNT */;
        else if (ut_char == '\0')
        {
            fprintf(stderr, "%s %s: \"%s\": data-type expected after '@' in \"%s\" spec\n",
                    strcurtime(), __FUNCTION__, k->ident, src);
            return CDA_DATAREF_NONE;
        }
        else
        {
            fprintf(stderr, "%s %s: \"%s\": invalid channel-data-type after '@' in \"%s\" spec (only 't' is allowed for TEXT knobs)\n",
                    strcurtime(), __FUNCTION__, k->ident, src);
            return CDA_DATAREF_NONE;
        }
        // Optional COUNT
        if (isdigit(*chn))
        {
            nelems = strtol(chn, &endptr, 10);
            if (endptr == chn)
            {
                fprintf(stderr, "%s %s: \"%s\": invalid channel-nelems after '@' in \"%s\" spec\n",
                        strcurtime(), __FUNCTION__, k->ident, src);
                return CDA_DATAREF_NONE;
            }
            chn = endptr;
        }
        // Mandatory ':' separator
        if (*chn != ':')
        {
            fprintf(stderr, "%s %s: \"%s\": ':' expected after \"@%c\" in \"%s\" spec\n",
                    strcurtime(), __FUNCTION__, k->ident, ut_char, src);
            return CDA_DATAREF_NONE;
        }
        else
            chn++;
    }
 END_FLAGS:;

    ref = cda_add_chan(subsys->cid, curbase,
                       chn, options, dtype, nelems,
                       evmask, evproc, privptr2);

    if (ref == CDA_DATAREF_ERROR)
        fprintf(stderr, "%s \"%s\".%s: %s\n",
                strcurtime(), k->ident, src_name, cda_last_err());

    /*!!! No, should also check somehow else for bad results --
     by CXCF_FLAG_NOTFOUND */

    return ref;
}

static cda_dataref_t pzf2ref(DataKnob k,
                             DataSubsys     subsys,
                             const char    *curbase)
{
  char      pzfr_base[CDA_PATH_MAX];
  char     *baseptr;

    baseptr = cda_combine_base_and_spec(subsys->cid,
                                        curbase, k->u.z.src,
                                        pzfr_base, sizeof(pzfr_base));
    return cda_add_chan(subsys->cid, pzfr_base,
                        "_devstate", 0, CXDTYPE_INT32, 1,
                        0, NULL, NULL);
}

static inline int isempty(const char *s)
{
    return s == NULL  ||  *s == '\0';
}

static void UpdateStr(char **pp, const char *new_value)
{
  char *new_ptr;

    if (new_value != NULL  &&  *new_value != '\0')
    {
        new_ptr = safe_realloc(*pp, strlen(new_value) + 1);
        if (new_ptr == NULL) return;
        strcpy(new_ptr, new_value);
        *pp = new_ptr;
    }
    else
    {
        /* Non-NULL?  Just set to epmty */
        if (*pp != NULL) *pp = '\0';
        /* Otherwise do-nothing */
    }
}
static void RefStrsChgEvproc(int            uniq,
                             void          *privptr1,
                             cda_dataref_t  ref,
                             int            reason,
                             void          *info_ptr,
                             void          *privptr2)
{
  DataKnob     k     = privptr2;
  data_knob_t  old_k = *k;
  char        *ident;
  char        *label;
  char        *tip;
  char        *comment;
  char        *geoinfo;
  char        *rsrvd6;
  char        *units;
  char        *dpyfmt;

  int     flags;
  int     width;
  int     precision;
  char    conv_c;

//fprintf(stderr, "\a%s()\n", __FUNCTION__);
    /* 1. Get info */
    if (cda_strings_of_ref(ref,
                           &ident, &label,
                           &tip, &comment, &geoinfo, &rsrvd6,
                           &units, &dpyfmt) < 0) return;
    /* 2. Update "unspecified" fields */
    if (k->strsbhvr & DATAKNOB_STRSBHVR_IDENT)   UpdateStr(&(k->ident),     ident);
    if (k->strsbhvr & DATAKNOB_STRSBHVR_LABEL)   UpdateStr(&(k->label),     label);
    if (k->strsbhvr & DATAKNOB_STRSBHVR_TIP)     UpdateStr(&(k->tip),       tip);
    if (k->strsbhvr & DATAKNOB_STRSBHVR_COMMENT) UpdateStr(&(k->comment),   comment);
    if (k->strsbhvr & DATAKNOB_STRSBHVR_GEOINFO) UpdateStr(&(k->geoinfo),   geoinfo);
    if (k->strsbhvr & DATAKNOB_STRSBHVR_UNITS)   UpdateStr(&(k->u.k.units), units);
    if (k->strsbhvr & DATAKNOB_STRSBHVR_DPYFMT)
    {
        /* Clever "duplication" of dpyfmt */
        if (ParseDoubleFormat(GetTextFormat(dpyfmt),
                              &flags, &width, &precision, &conv_c) >= 0)
            CreateDoubleFormat(&(k->u.k.dpyfmt), sizeof(k->u.k.dpyfmt), 20, 10,
                               flags, width, precision, conv_c);
    }
                      
    /* 3. Notify knobplugin... */
    if (k->vmtlink != NULL                 &&
        k->vmtlink->type == DATAKNOB_KNOB  &&
        ((dataknob_knob_vmt_t *)(k->vmtlink))->PropsChg != NULL)
        ((dataknob_knob_vmt_t *)(k->vmtlink))->PropsChg(k, &old_k);
    /* 4. ...and parent */
    if (k->uplink != NULL                     &&
        (k->uplink->type == DATAKNOB_GRPG  ||
         k->uplink->type == DATAKNOB_CONT)    &&
        k->uplink->vmtlink != NULL            &&
        ((dataknob_cont_vmt_t *)(k->uplink->vmtlink))->ChildPropsChg != NULL)
        ((dataknob_cont_vmt_t *)(k->uplink->vmtlink))->ChildPropsChg(k->uplink,
                                                                     k - k->uplink->u.c.content,
                                                                     &old_k);
    
}

int   CdrRealizeKnobs    (DataSubsys  subsys,
                          const char *baseref,
                          DataKnob list, int count)
{
  int       n;
  DataKnob  k;
  char      curbase[CDA_PATH_MAX];
  char     *curbptr;
  rflags_t  rflags;
    
    if (list == NULL  ||  count <= 0) return -1;

    for (n = 0, k = list;
         n < count;
         n++,   k++)
    {
        if (isempty(k->ident))   k->strsbhvr |= DATAKNOB_STRSBHVR_IDENT;
        if (isempty(k->label))   k->strsbhvr |= DATAKNOB_STRSBHVR_LABEL;
        if (isempty(k->tip))     k->strsbhvr |= DATAKNOB_STRSBHVR_TIP;
        if (isempty(k->comment)) k->strsbhvr |= DATAKNOB_STRSBHVR_COMMENT;
        if (isempty(k->geoinfo)) k->strsbhvr |= DATAKNOB_STRSBHVR_GEOINFO;

        /* First, do type-dependent things... */
        switch (k->type)
        {
            case DATAKNOB_GRPG:
                /* Fallthrough to CONT */
            case DATAKNOB_CONT:
                /*!!! baseref+k->u.c.refbase wizardry?*/
#if 1
                if (k->u.c.refbase != NULL  &&  k->u.c.refbase[0] != '\0')
                    curbptr = cda_combine_base_and_spec(subsys->cid,
                                                        baseref, k->u.c.refbase,
                                                        curbase, sizeof(curbase));
                else
                    curbptr = baseref;

                CdrRealizeKnobs(subsys,
                                curbptr,
                                k->u.c.content, k->u.c.count);
#else
                curbase[0] = '\0';
                if      (k->u.c.refbase != NULL  &&  k->u.c.refbase[0] != '\0')
                    strzcpy(curbase, k->u.c.refbase, sizeof(curbase));
                else if (baseref != NULL)
                    strzcpy(curbase, baseref, sizeof(curbase));
                //curbase[0] = '\0';

                CdrRealizeKnobs(subsys,
                                curbase,
                                k->u.c.content, k->u.c.count);
#endif

                k->u.c.at_init_ref = src2ref(k, "at_init",
                                             subsys, baseref,
                                             k->u.c.at_init_src, 0, 0,
                                             NULL, 0,
                                             0, NULL, NULL);
                k->u.c.ap_ref      = src2ref(k, "ap",
                                             subsys, baseref,
                                             k->u.c.ap_src     , 0, 0,
                                             NULL, 0,
                                             0, NULL, NULL);
                break;
                
            case DATAKNOB_NOOP:
                /* Nothing to do */
                break;
                
            case DATAKNOB_KNOB:
                k->timestamp = (cx_time_t){CX_TIME_SEC_NEVER_READ,0};

                if (isempty(k->u.k.units))    k->strsbhvr |= DATAKNOB_STRSBHVR_UNITS;
                if (k->u.k.dpyfmt[0] == '\0') k->strsbhvr |= DATAKNOB_STRSBHVR_DPYFMT;

                k->u.k.rd_ref      = src2ref(k, "rd",
                                             subsys, baseref,
                                             k->u.k.rd_src    , 1, k->behaviour,
                                             k->u.k.params, k->u.k.num_params,
                                             k->strsbhvr == 0? 0 : CDA_REF_EVMASK_STRSCHG,
                                             RefStrsChgEvproc, k);
                k->u.k.wr_ref      = src2ref(k, "wr",
                                             subsys, baseref,
                                             k->u.k.wr_src    , 1, 0,
                                             k->u.k.params, k->u.k.num_params,
                                             0, NULL, NULL);
                k->u.k.cl_ref      = src2ref(k, "cl",
                                             subsys, baseref,
                                             k->u.k.cl_src    , 0, 0,
                                             k->u.k.params, k->u.k.num_params,
                                             0, NULL, NULL);
                if ( k->u.k.rd_ref == CDA_DATAREF_ERROR  /*&& (fprintf(stderr, "<%s>:ERROR\n", k->u.k.rd_src))*/  ||
                    (k->u.k.rd_ref != CDA_DATAREF_NONE     &&
                     cda_get_ref_dval(k->u.k.rd_ref,
                                      NULL,
                                      NULL, NULL,
                                      &rflags, NULL) >= 0  &&
                    (rflags & CXCF_FLAG_NOTFOUND) != 0))
                    k->curstate = KNOBSTATE_NOTFOUND;

                /* Perform kind-dependent preparations */
                if      (k->u.k.kind == DATAKNOB_KIND_DEVN)
                {
                    /* Do-nothing */
                }
                else if (k->u.k.kind == DATAKNOB_KIND_MINMAX)
                {
                    k->u.k.kind_var.minmax.bufcap = MMXCAP;
                    k->u.k.kind_var.minmax.buf    = malloc(k->u.k.kind_var.minmax.bufcap * sizeof(k->u.k.kind_var.minmax.buf[0]));
                    if (k->u.k.kind_var.minmax.buf == NULL) return -1;
                }
                break;
                
            case DATAKNOB_BIGC:
                /*!!!*/
                break;

            case DATAKNOB_TEXT:
                k->timestamp = (cx_time_t){CX_TIME_SEC_NEVER_READ,0};
                k->u.t.ref   = txt2ref(k, "src",
                                       subsys, baseref,
                                       k->u.t.src, k->is_rw,
                                       NULL, 0,
                                       0, NULL, NULL);
                if ( k->u.t.ref == CDA_DATAREF_ERROR   ||
                    (k->u.t.ref != CDA_DATAREF_NONE        &&
                     cda_get_ref_stat(k->u.t.ref,
                                      &rflags, NULL) >= 0  &&
                    (rflags & CXCF_FLAG_NOTFOUND) != 0))
                    k->curstate = KNOBSTATE_NOTFOUND;
                break;
                
            case DATAKNOB_USER:
                /*!!!*/
                break;

            case DATAKNOB_PZFR:
                if (baseref != NULL  &&  baseref[0] != '\0')
                    k->u.z.tree_base = strdup(baseref);
                k->u.z._devstate_ref = pzf2ref(k, subsys, baseref);
                break;
        }
    }

    return 0;
}


void  CdrProcessSubsystem(DataSubsys subsys, int cause_conn_n,  int options,
                                                     rflags_t *rflags_p)
{
  int             n;
  data_section_t *p;

  rflags_t        rflags;
  rflags_t        cml_rflags = 0;

    for (n = 0, p = subsys->sections;
         n < subsys->num_sections;
         n++,   p++)
        if (strcasecmp(p->type, DSTN_GROUPING) == 0)
        {
            CdrProcessKnobs(subsys, cause_conn_n, options,
                            p->data, 1, &rflags);
            cml_rflags   |= rflags;
        }

    if (rflags_p != NULL) *rflags_p = cml_rflags;
}

void  CdrProcessKnobs    (DataSubsys subsys, int cause_conn_n, int options,
                          DataKnob list, int count, rflags_t *rflags_p)
{
  int         n;
  DataKnob    k;

  int         cda_opts = ((options & CDR_OPT_READONLY) ||
                          subsys->readonly)? CDA_OPT_READONLY : 0;

  rflags_t    rflags;
  rflags_t    rflags2;
  rflags_t    cml_rflags = 0;

  double      v;
  double      cl_v;
  CxAnyVal_t  raw;
  cxdtype_t   raw_dtype;
  time_t      timenow = time(NULL);

  int         rn;
  
  /* For LOGT_DEVN */
  double      newA;
  double      newA2;
  double      divider;

  /* For LOGT_MINMAX */
  double      min, max;
  double     *dp;
  int         i;

  /* For DATAKNOB_TEXT */
  void       *text_buf;

    for (n = 0, k = list;
         n < count;
         n++,   k++)
    {
        if (cause_conn_n >= 0              &&
            k->conns_u != NULL             &&
            k->conns_u[cause_conn_n] == 0)
        {
            rflags = k->currflags;
            goto ACCUMULATE_RFLAGS;
        }

        rflags = 0;
        switch (k->type)
        {
            case DATAKNOB_GRPG:
                /* Fallthrough to CONT */
            case DATAKNOB_CONT:
                if (cda_ref_is_sensible(k->u.c.ap_ref))
                {
                    cda_process_ref (k->u.c.ap_ref,
                                     CDA_OPT_RD_FLA | CDA_OPT_DO_EXEC | cda_opts,
                                     NAN,
                                     k->u.k.params, k->u.k.num_params);
                    cda_get_ref_dval(k->u.c.ap_ref,
                                     NULL,
                                     NULL, NULL,
                                     &rflags2, &(k->timestamp));
                }
                else
                    rflags2 = 0;

                CdrProcessKnobs(subsys, cause_conn_n, options,
                                k->u.c.content, k->u.c.count, &rflags);
                rflags |= rflags2;

                /* For CONTainers: store rflags right now for NewData() to be able to use them immediately instead of a cycle later */
                /*     (For some strange reason, this works for normal->alarm and relax->normal only, but NOT for alarm->relax, which shows as alarm->normal->relax) */
                k->currflags = rflags;

                if (k->vmtlink != NULL  &&
                    ((dataknob_cont_vmt_t*)(k->vmtlink))->NewData != NULL)
                    ((dataknob_cont_vmt_t*)(k->vmtlink))->NewData(k,
                                                                  (options & CDR_OPT_SYNTHETIC) != 0);
                break;
                
            case DATAKNOB_NOOP:
                /* Nothing to do */
                break;
                
            case DATAKNOB_KNOB:
                /* Obtain a value... */
                if (cda_ref_is_sensible(k->u.k.rd_ref))
                {
                    cda_process_ref (k->u.k.rd_ref,
                                     CDA_OPT_RD_FLA | CDA_OPT_DO_EXEC | cda_opts,
                                     NAN,
                                     k->u.k.params, k->u.k.num_params);
                    cda_get_ref_dval(k->u.k.rd_ref,
                                     &v,
                                     &raw, &raw_dtype,
                                     &rflags, &(k->timestamp));
                }
                else
                {
                    v         = NAN;
                    rflags    = 0;
                    raw_dtype = CXDTYPE_UNKNOWN;
                }
                rflags &= (CXRF_SERVER_MASK | CXCF_FLAG_CDA_MASK);

                /* Perform kind-dependent operations with a value */
                if      (k->u.k.kind == DATAKNOB_KIND_DEVN)
                {
                    /*!!! Should check for synthetic update! */
    
                    /* Initialize average value with current */
                    if (k->u.k.kind_var.devn.notnew == 0)
                    {
                        k->u.k.kind_var.devn.avg  = v;
                        k->u.k.kind_var.devn.avg2 = v*v;
    
                        k->u.k.kind_var.devn.notnew = 1;
                    }
    
                    /* Recursively calculate next avg and avg2 */
                    newA  = (k->u.k.kind_var.devn.avg * NUMAVG + v)
                        /
                        (NUMAVG + 1);
                    
                    newA2 = (k->u.k.kind_var.devn.avg2 * NUMAVG + v*v)
                        /
                        (NUMAVG + 1);
                    
                    /* Store them */
                    k->u.k.kind_var.devn.avg  = newA;
                    k->u.k.kind_var.devn.avg2 = newA2; 
                    
                    /* And substitute the value of 'v' with the deviation */
                    divider = fabs(newA2);
                    if (divider < 0.00001) divider = 0.00001;
                    
                    v =  (
                          sqrt(fabs(newA2 - newA*newA))
                          /
                          divider
                         ) * 100;
                }
                else if (k->u.k.kind == DATAKNOB_KIND_MINMAX  &&
                         k->u.k.kind_var.minmax.bufcap != 0)  /* Guard against processed-before-realized problem */
                {
                    /* Store current value */
                    if ((options & CDR_OPT_SYNTHETIC) == 0)  /* On real, non-synthetic updates only */
                    {
                        if (k->u.k.kind_var.minmax.bufused == k->u.k.kind_var.minmax.bufcap)
                        {
                        ////fprintf(stderr, "%s used=%d cap=%d \n", k->ident, k->u.k.kind_var.minmax.bufused, k->u.k.kind_var.minmax.bufcap);
                            k->u.k.kind_var.minmax.bufused--;
                            memmove(&(k->u.k.kind_var.minmax.buf[0]),  &(k->u.k.kind_var.minmax.buf[1]),
                                    k->u.k.kind_var.minmax.bufused * sizeof(k->u.k.kind_var.minmax.buf[0]));
                        }
                        k->u.k.kind_var.minmax.buf[k->u.k.kind_var.minmax.bufused++] = v;
                    }
                    else
                    {
                        /* Here we handle a special case -- when synthetic update occurs before
                         any real data arrival, so we must use one minmax cell, otherwise we
                         just replace the last value */
                        if (k->u.k.kind_var.minmax.bufused == 0)
                            k->u.k.kind_var.minmax.buf[k->u.k.kind_var.minmax.bufused++] = v;
                        else
                            k->u.k.kind_var.minmax.buf[k->u.k.kind_var.minmax.bufused-1] = v;
                    }
                    
                    /* Find min & max */
                    for (i = 0,  dp = k->u.k.kind_var.minmax.buf,  min = max = *dp;
                         i < k->u.k.kind_var.minmax.bufused;
                         i++, dp++)
                    {
                        if (*dp < min) min = *dp;
                        if (*dp > max) max = *dp;
                    }
                    
                    v = fabs(max - min);
                }

                /* A separate colorization? */
                if (cda_ref_is_sensible(k->u.k.cl_ref))
                {
                    cda_process_ref (k->u.k.cl_ref,
                                     CDA_OPT_RD_FLA | CDA_OPT_DO_EXEC | cda_opts,
                                     NAN,
                                     k->u.k.params, k->u.k.num_params);
                    cda_get_ref_dval(k->u.k.cl_ref,
                                     &cl_v,
                                     NULL, NULL,
                                     &rflags2, NULL);
                    rflags |= rflags2;

                    k->u.k.curcolv = cl_v;
                }
                
                /* Store new values... */
                k->u.k.curv           = v;
                k->u.k.curv_raw       = raw;
                k->u.k.curv_raw_dtype = raw_dtype;

                /* ...and store in history buffer, if present and if required */

                /*!!! Make decisions about behaviour/colorization/etc.*/
                if (k->attn_endtime != 0  &&
                    difftime(timenow, k->attn_endtime) >= 0)
                    k->attn_endtime = 0;
                rflags = choose_knob_rflags(k, rflags,
                                            cda_ref_is_sensible(k->u.k.cl_ref)? cl_v : v);

                if (rflags & CXCF_FLAG_OTHEROP) set_knob_otherop(k, 1);

                //!!! Here is a place for alarm detection
                if ((rflags ^ k->currflags) & CXCF_FLAG_ALARM_ALARM)
                    show_knob_alarm(k, (rflags & CXCF_FLAG_ALARM_ALARM) != 0);

                k->currflags           = rflags;

                /* Display the result -- ... */
                /* ...colorize... */
                set_knobstate(k, choose_knobstate(k, rflags));
                /* ...and finally show the value */
                if (
                    !k->is_rw  ||
                    (
                     difftime(timenow, k->usertime) > KNOBS_USERINACTIVITYLIMIT  &&
                     !k->wasjustset
                    )
                   )
                    set_knob_controlvalue(k, v, 0);
                
                if ((options & CDR_OPT_SYNTHETIC) == 0) k->wasjustset = 0;
                
                break;
                
            case DATAKNOB_BIGC:
                /*!!!*/
                break;

            case DATAKNOB_TEXT:
                if (cda_ref_is_sensible(k->u.t.ref)  &&
                    cda_acc_ref_data   (k->u.t.ref, &text_buf, NULL) == 0)
                {
                    cda_get_ref_stat(k->u.t.ref,
                                     &(k->currflags),
                                     &(k->timestamp));
                    /* Display the result -- ... */
                    /* ...colorize... */
                    set_knobstate(k, choose_knobstate(k, k->currflags));
                    /* ...and finally show the value */
                    if (
                        !k->is_rw  ||
                        (
                         difftime(timenow, k->usertime) > KNOBS_USERINACTIVITYLIMIT  &&
                         !k->wasjustset
                        )
                       )
                        set_knob_textvalue(k, text_buf, 0);

                    if ((options & CDR_OPT_SYNTHETIC) == 0) k->wasjustset = 0;
                }
                break;
           
            case DATAKNOB_USER:
                for (rn = 0;  rn < k->u.u.num_datarecs;  rn++)
                {
                    cda_process_ref (k->u.u.datarecs[rn].dr,
                                     CDA_OPT_RD_FLA | CDA_OPT_DO_EXEC | cda_opts,
                                     NAN,
                                     NULL, 0);
                    cda_get_ref_dval(k->u.u.datarecs[rn].dr,
                                     &v,
                                     NULL, NULL,
                                     &rflags2, NULL);
                    if (!(k->u.u.datarecs[rn].is_rw))
                        rflags2 &=~ CXCF_FLAG_4WRONLY_MASK;

                    rflags |= rflags2;
                }
                for (rn = 0;  rn < k->u.u.num_bigcrecs;  rn++)
                {
                }
                /*!!!*/
                break;
        }
        k->currflags  = rflags;
        
 ACCUMULATE_RFLAGS:
        cml_rflags   |= rflags;
    }

    if (rflags_p != NULL) *rflags_p = cml_rflags;
}


int   CdrSetKnobValue(DataSubsys subsys, DataKnob  k, double      v, int options)
{
  CxDataRef_t  wr;
  int          ret;
  int          cda_opts = ((options & CDR_OPT_READONLY) ||
                           subsys->readonly)? CDA_OPT_READONLY : 0;

    /*!!! Should also check being_modified */
    /*!!! Allow CONTainers to be "writable" -- to set state/subelement
      (see bigfile-0002.html 30.03.2011) */
    if (k->type != DATAKNOB_KNOB  ||  !k->is_rw)
    {
        errno = EINVAL;
        return CDA_PROCESS_ERR;
    }

    /*!!! some status mangling... */

    /**/
    wr                               = k->u.k.wr_ref;
    if (!cda_ref_is_sensible(wr)) wr = k->u.k.rd_ref;
    if (!cda_ref_is_sensible(wr)) return CDA_PROCESS_DONE;

    /*!!! Should set and later drop being_modified */
    if (k->being_modified)
    {
        errno = EINPROGRESS;
        return CDA_PROCESS_ERR;
    }
    k->being_modified = 1;

    ret = cda_process_ref(wr, CDA_OPT_WR_FLA | CDA_OPT_DO_EXEC | cda_opts,
                          v,
                          k->u.k.params, k->u.k.num_params);
    if (ret != CDA_PROCESS_ERR  &&  (ret & CDA_PROCESS_FLAG_REFRESH) != 0)
        CdrProcessSubsystem(subsys, -1, CDR_OPT_SYNTHETIC | cda_opts, NULL);

    k->being_modified = 0;

    return ret;
}

int   CdrSetKnobText (DataSubsys subsys, DataKnob  k, const char *s, int options)
{
  CxDataRef_t  wr;
  int          ret;
  int          cda_opts = ((options & CDR_OPT_READONLY) ||
                           subsys->readonly)? CDA_OPT_READONLY : 0;

    if (k->type != DATAKNOB_TEXT  ||  !k->is_rw)
    {
        errno = EINVAL;
        return CDA_PROCESS_ERR;
    }

    wr                               = k->u.t.ref;
    if (!cda_ref_is_sensible(wr)) return CDA_PROCESS_DONE;

    /*!!! Should set and later drop being_modified */
    if (k->being_modified)
    {
        errno = EINPROGRESS;
        return CDA_PROCESS_ERR;
    }
    k->being_modified = 1;

    if (s == NULL) s = "";
    ret = cda_snd_ref_data(wr, CXDTYPE_TEXT, strlen(s), s);
    if (ret != CDA_PROCESS_ERR  &&  (ret & CDA_PROCESS_FLAG_REFRESH) != 0)
        CdrProcessSubsystem(subsys, -1, CDR_OPT_SYNTHETIC | cda_opts, NULL);

    k->being_modified = 0;

    return ret;
}

int   CdrAddHistory  (DataKnob  k, int histring_len)
{
  DataSubsys subsys;

    if (k->type != DATAKNOB_KNOB)
    {
        errno = EINVAL;
        return -1;
    }

    if (k->u.k.histring != NULL) return 0;

    subsys = get_knob_subsys(k);
    if (subsys == NULL)
    {
        errno = EINVAL;
        return -1;
    }

    if (histring_len <= 0) histring_len = 86400;
    k->u.k.histring = malloc(histring_len * sizeof(*(k->u.k.histring)));
    if (k->u.k.histring == NULL) return -1;

    k->u.k.histring_len   = histring_len;
    k->u.k.histring_start = 0;
    k->u.k.histring_used  = 0;

    k->u.k.hist_prev = subsys->hist_last;
    k->u.k.hist_next = NULL;

    if (subsys->hist_last != NULL)
        subsys->hist_last->u.k.hist_next = k;
    else
        subsys->hist_first           = k;
    subsys->hist_last = k;

    return 0;
}

void  CdrShiftHistory(DataSubsys subsys)
{
  DataKnob  k;

    for (k = subsys->hist_first;  k != NULL;  k = k->u.k.hist_next)
    {
        if (k->u.k.histring_used == k->u.k.histring_len)
            k->u.k.histring_start = (k->u.k.histring_start + 1) % k->u.k.histring_len;
        else
            k->u.k.histring_used++;

        k->u.k.histring[(k->u.k.histring_start + k->u.k.histring_used - 1) % k->u.k.histring_len] = k->u.k.curv;
    }
}

int   CdrBroadcastCmd(DataKnob  k, const char *cmd, int info_int)
{
  int  r = 0;
  int  n;
    
    if (k == NULL) return -1;
  
    if (k->vmtlink            != NULL  &&
        k->vmtlink->HandleCmd != NULL)
        r = k->vmtlink->HandleCmd(k, cmd, info_int);
    
    if (r != 0) return r;

    if (k->type == DATAKNOB_GRPG  ||  k->type == DATAKNOB_CONT)
        for (n = 0;
             n < k->u.c.count  &&  r == 0;
             n++)
            r = CdrBroadcastCmd(k->u.c.content + n, cmd, info_int);

    return r;
}

static const char *mode_lp_s    = "#=Mode";
static const char *subsys_lp_s  = "#!SUBSYSTEM:";
static const char *crtime_lp_s  = "#!CREATE-TIME:";
static const char *comment_lp_s = "#!COMMENT:";

/* Copy from console_cda_util.c */
static void PrintChar(FILE *fp, char32 c32)
{
    if      ((c32 & 0x000000FF) == c32)
    {
        if      (c32 == '\\')
            fprintf(fp, "\\\\");
        else if (c32 == '\'')
            fprintf(fp, "\\'");
        else if (c32 == '\"')
            fprintf(fp, "\\\"");
        else if (isprint(c32))
            fprintf(fp, "%c", c32);
        else if (c32 == '\a')
            fprintf(fp, "\\a");
        else if (c32 == '\b')
            fprintf(fp, "\\b");
        else if (c32 == '\f')
            fprintf(fp, "\\f");
        else if (c32 == '\n')
            fprintf(fp, "\\n");
        else if (c32 == '\r')
            fprintf(fp, "\\r");
        else if (c32 == '\t')
            fprintf(fp, "\\t");
        else if (c32 == '\v')
            fprintf(fp, "\\v");
        else
            fprintf(fp, "\\x%02x", c32);
    }
    else if ((c32 & 0x0000FFFF) == c32)
    {
        fprintf(fp, "\\u%04x", c32);
    }
    else
    {
        fprintf(fp, "\\U%08x", c32);
    }
}

static void FprintfKnobPathPart(FILE *fp, DataKnob k)
{
    if (k->uplink != NULL)
    {
        FprintfKnobPathPart(fp, k->uplink);
        fprintf(fp, ".");
    }
    fprintf(fp, "%s",
            k->ident != NULL  &&  k->ident[0] != '\0'? k->ident : "-");
}
static void FprintfKnobSrc(FILE *fp, CxDataRef_t rd_ref, CxDataRef_t wr_ref)
{
  const char *src;
  const char *p;
  int         is_chanref_char;
  int         is_sensible;

    src = NULL;
    if (cda_src_of_ref(wr_ref, &src) <= 0) 
        cda_src_of_ref(rd_ref, &src);
    if (src == NULL  ||  *src == '\0') is_sensible = 0;
    else
    {
        for ( p = src,       is_sensible = 1;  
             *p != '\0'  &&  is_sensible;
              p++)
        {
            /* Logic from cvt2ref() */
            is_chanref_char =
                isalnum(*p)  ||  *p == '_'  ||  *p == '-'  ||
                *p == '.'  ||  *p == ':'  ||  *p == '@';
            if (!is_chanref_char) is_sensible = 0;
        }
    }
    fprintf(fp, "%s", is_sensible? src : "-");
}
static void FprintfKnobStats(FILE *fp, DataKnob k)
{
  int            shift;
  int            notfirst;

    fprintf(fp, " @%ld.%06ld", k->timestamp.sec, k->timestamp.nsec/1000);
    fprintf(fp, " <");
    for (shift = 31, notfirst = 0;
         shift >= 0;
         shift--)
        if ((k->currflags & (1 << shift)) != 0)
        {
            if (notfirst) fprintf(fp, ",");
            fprintf(fp, "%s", cx_strrflag_short(shift));
            notfirst = 1;
        }
    fprintf(fp, ">");
}
static void SaveKnobsMode(FILE *fp, DataSubsys subsys, DataKnob list, int count)
{
  int          n;
  DataKnob     k;

  int          nelems;
  const char8 *p;

    for (n = 0, k = list;
         n < count;
         n++,   k++)
    {
        if ((k->behaviour & DATAKNOB_B_UNSAVABLE) != 0) continue;
        if      (k->type == DATAKNOB_KNOB)
        {
            if (k->is_rw == 0) fprintf(fp, "#");
            /* dtype */
            fprintf(fp, "@d:");
            /* Knob reference */
            fprintf(fp, "(");
            FprintfKnobPathPart(fp, k);
            fprintf(fp, ")");
            /* Channel reference */
            FprintfKnobSrc(fp, k->u.k.wr_ref, k->u.k.rd_ref);
            /* Value */
            fprintf(fp, " ");
            fprintf(fp, k->u.k.dpyfmt, k->u.k.curv);
            FprintfKnobStats(fp, k);
            fprintf(fp, "\n");
        }
        else if (k->type == DATAKNOB_TEXT)
        {
            if (k->is_rw == 0) fprintf(fp, "#");
            /* dtype */
            nelems = cda_max_nelems_of_ref(k->u.t.ref);
            fprintf(stderr, "@t%d:", nelems > 0? nelems : 1);
            /* Knob reference */
            fprintf(fp, "(");
            FprintfKnobPathPart(fp, k);
            fprintf(fp, ")");
            /* Channel reference */
            FprintfKnobSrc(fp, k->u.t.ref, CDA_DATAREF_NONE);

            /* Value */
            fprintf(fp, " \"");
            if (cda_ref_is_sensible(k->u.t.ref)  &&
                cda_acc_ref_data   (k->u.t.ref, &p, NULL) == 0)
                for (;  *p != '\0';  p++) PrintChar(fp, *p);
            else
                fprintf(fp, "%s", "-"); /*!!! Should do the same as in CdrProcessKnobs() -- cda_acc_ref_data() & Co.; but with escaping controls, quotes etc. */
            fprintf(fp, "\"");
            FprintfKnobStats(fp, k);
            fprintf(fp, "\n");
        }
        else if (k->type == DATAKNOB_GRPG  ||
                 k->type == DATAKNOB_CONT)
        {
            SaveKnobsMode(fp, subsys, k->u.c.content, k->u.c.count);
        }
    }
}
int   CdrSaveSubsystemMode(DataSubsys subsys,
                           const char *filespec, int         options,
                           const char *comment)
{
  FILE           *fp;
  time_t          timenow = time(NULL);
  const char     *cp;

  int             n;
  data_section_t *p;

    if ((fp = fopen(filespec, "w")) == NULL) return -1;

    fprintf(fp, "##########\n%s %s\n", mode_lp_s, ctime(&timenow));
    fprintf(fp, "%s %s\n",  subsys_lp_s, subsys->sysname);
    fprintf(fp, "%s %lu %s", crtime_lp_s, (unsigned long)(timenow), ctime(&timenow)); /*!: No '\n' because ctime includes a trailing one. */
    fprintf(fp, "%s ", comment_lp_s);
    if (comment != NULL)
        for (cp = comment;  *cp != '\0';  cp++)
            fprintf(fp, "%c", !iscntrl(*cp)? *cp : ' ');
    fprintf(fp, "\n\n");

    for (n = 0, p = subsys->sections;
         n < subsys->num_sections;
         n++,   p++)
        if (strcasecmp(p->type, DSTN_GROUPING) == 0)
            SaveKnobsMode(fp, subsys, p->data, 1);

    fclose(fp);
    
    return 0;
}

int   CdrLoadSubsystemMode(DataSubsys subsys,
                           const char *filespec, int         options,
                           char   *commentbuf, size_t commentbuf_size)
{
  FILE           *fp;
  int             lineno;
  char            line     [1000];
  char           *cp;
  char            knob_name[1000];
  char            chan_name[1000];
  size_t          name_len;
  int             was_dot;

  DataKnob        root = CdrGetMainGrouping(subsys);

  char            ut_char;
  cxdtype_t       dtype;
  int             n_items;
  double          val;
  char           *endptr;
  char            str      [1000];

  cda_dataref_t   ref;
  DataKnob        k;

    if ((fp = fopen(filespec, "r")) == NULL) return -1;
    lineno = 0;

    if (commentbuf_size > 0) *commentbuf = '\0';

    while (fgets(line, sizeof(line) -1, fp) != NULL)
    {
        lineno++;

        // Rtrim
        for (cp = line + strlen(line) - 1;
             cp > line  &&  (*cp == '\n'  ||  *cp == '\r'  ||  isspace(*cp));
             *cp-- = '\0');
        // Ltrim
        for (cp = line; *cp != '\0'  &&  isspace(*cp);  cp++);
        // Skip "@TIME" prefix
        if (*cp == '@'  &&  isdigit(cp[1]))
        {
            // Timestamp itself...
            while (*cp != '\0'  &&  !isspace(*cp)) cp++;
            // ...spaces
            while (*cp != '\0'  &&   isspace(*cp)) cp++;
        }

        if (memcmp(cp, comment_lp_s, strlen(comment_lp_s)) == 0  &&
            commentbuf_size > 0)
        {
            cp += strlen(comment_lp_s);
            while (*cp != '\0'  &&  isspace(*cp)) cp++;

            strzcpy(commentbuf, cp, commentbuf_size);

            goto NEXT_LINE;
        }
        else if (*cp == '\0'  ||  *cp == '#') goto NEXT_LINE;

        dtype   = CXDTYPE_DOUBLE;
        n_items = 1;
        if (*cp == '@')
        {
            cp++;

            ut_char = tolower(*cp);
            if      (*cp == '\0')
            {
                fprintf(stderr, "%s %s(\"%s\") line %d: unexpected EOL after '@'\n",
                        strcurtime(), __FUNCTION__, filespec, lineno);
                goto NEXT_LINE;
            }
            else if (ut_char == 'd')
                dtype = CXDTYPE_DOUBLE;
            else if (ut_char == 't')
            {
                dtype = CXDTYPE_TEXT;
            }
            cp++;

            while (1)
            {
                if (*cp == '\0')
                {
                    fprintf(stderr, "%s %s(\"%s\") line %d: unexpected EOL after '@%c'\n",
                            strcurtime(), __FUNCTION__, filespec, lineno, ut_char);
                    goto NEXT_LINE;
                }
                if (*cp == ':')
                {
                    cp++;
                    goto END_PARSE_DTYPE;
                }
            }
        }
 END_PARSE_DTYPE:;

        knob_name[0] = chan_name[0] = '\0';

        if (*cp == '(')
        {
            cp++;
            for (name_len = 0, was_dot = 0;  ;  cp++)
            {
                if (*cp == '\0')
                {
                    fprintf(stderr, "%s %s(\"%s\") line %d: unexpected EOL while parsing (KNOB_NAME)\n",
                            strcurtime(), __FUNCTION__, filespec, lineno);
                    goto NEXT_LINE;
                }
                if (*cp == ')')
                {
                    knob_name[name_len] = '\0';
                    goto END_PARSE_KNOB_NAME;
                }
                if (name_len >= sizeof(knob_name) - 1)
                {
                    fprintf(stderr, "%s %s(\"%s\") line %d: knob name too long\n",
                            strcurtime(), __FUNCTION__, filespec, lineno);
                    goto NEXT_LINE;
                }
                if      (was_dot)
                    knob_name[name_len++] = *cp;
                else if (*cp == '.')
                    was_dot = 1;
            }
        }
 END_PARSE_KNOB_NAME:;

        for (name_len = 0;  ;  cp++)
        {
            if (*cp == '\0')
            {
                fprintf(stderr, "%s %s(\"%s\") line %d: unexpected EOL while parsing CHANNEL_NAME\n",
                        strcurtime(), __FUNCTION__, filespec, lineno);
                goto NEXT_LINE;
            }
            if (isspace(*cp))
            {
                chan_name[name_len] = '\0';
                goto END_PARSE_CHAN_NAME;
            }
            if (name_len >= sizeof(chan_name) - 1)
            {
                fprintf(stderr, "%s %s(\"%s\") line %d: chan name too long\n",
                        strcurtime(), __FUNCTION__, filespec, lineno);
                goto NEXT_LINE;
            }
            chan_name[name_len++] = *cp;
        }
 END_PARSE_CHAN_NAME:;

        while (*cp != '\0'  &&  (isspace(*cp) ||  *cp == '=' )) cp++;

        if (dtype == CXDTYPE_DOUBLE)
        {
            val = strtod(cp, &endptr);
////fprintf(stderr, "%s\n%s\n%d\n", cp, endptr, !isspace(*cp));
            if (endptr == cp  ||  (*endptr != '\0'  &&  !isspace(*endptr)))
            {
                fprintf(stderr, "%s %s(\"%s\") line %d: error parsing value\n",
                        strcurtime(), __FUNCTION__, filespec, lineno);
                goto NEXT_LINE;
            }
        }
        else goto NEXT_LINE;

        if (knob_name[0] != '\0')
        {
            k = datatree_find_node(root, knob_name, -1);
////fprintf(stderr, "find(%s)=%p\n", knob_name, k);
            if (k == NULL)
            {
                goto NEXT_LINE;
            }

            if      (k->type == DATAKNOB_KNOB)
            {
                if (dtype == CXDTYPE_DOUBLE)
                    set_knob_controlvalue(k, val, 1);
                else
                {
                }
            }
            else if (k->type == DATAKNOB_TEXT)
            {
                if (dtype == CXDTYPE_TEXT)
                    set_knob_textvalue   (k, str, 1);
                else
                {
                }
            }
        }
        else if (0)
        {
            ref = cda_add_chan(subsys->cid, NULL,
                               chan_name, CDA_DATAREF_OPT_FIND_ONLY,
                               dtype, n_items,
                               0, NULL, NULL);
            if (ref == CDA_DATAREF_ERROR)
            {
                goto NEXT_LINE;
            }

            if      (dtype == CXDTYPE_DOUBLE)
                cda_set_dcval   (ref, val);
            else if (dtype == DATAKNOB_TEXT)
                cda_snd_ref_data(ref, CXDTYPE_TEXT, strlen(str), str);
        }

 NEXT_LINE:;
    }

    fclose(fp);

    return 0;
}

int   CdrStatSubsystemMode(const char *filespec,
                           time_t *cr_time,
                           char   *commentbuf, size_t commentbuf_size)
{
  FILE        *fp;
  char         line[1000];
  int          lineno = 0;
  char        *cp;
  char        *err;
  struct tm    brk_time;
  time_t       sng_time;

    if ((fp = fopen(filespec, "r")) == NULL) return -1;

    if (commentbuf_size > 0) *commentbuf = '\0';

    while (lineno < 10  &&  fgets(line, sizeof(line) - 1, fp) != NULL)
    {
        lineno++;
    
        if      (memcmp(line, mode_lp_s,    strlen(mode_lp_s)) == 0)
        {
            cp = line + strlen(mode_lp_s);
            while (*cp != '\0'  &&  isspace(*cp)) cp++;
            
            bzero(&brk_time, sizeof(brk_time));
            if (strptime(cp, "%a %b %d %H:%M:%S %Y", &brk_time) != NULL)
            {
                sng_time = mktime(&brk_time);
                if (sng_time != (time_t)-1)
                    *cr_time = sng_time;
            }
        }
        else if (memcmp(line, crtime_lp_s,  strlen(crtime_lp_s)) == 0)
        {
            cp = line + strlen(crtime_lp_s);
            while (*cp != '\0'  &&  isspace(*cp)) cp++;

            sng_time = (time_t)strtol(cp, &err, 0);
            if (err != cp  &&  (*err == '\0'  ||  isspace(*err)))
                *cr_time = sng_time;
        }
        else if (memcmp(line, comment_lp_s, strlen(comment_lp_s)) == 0  &&
                 commentbuf_size > 0)
        {
            cp = line + strlen(comment_lp_s);
            while (*cp != '\0'  &&  isspace(*cp)) cp++;

            strzcpy(commentbuf, cp, commentbuf_size);
        }
    }

    fclose(fp);

    return 0;
}
