
#include "misc_macros.h"
#include "misclib.h"
#include "memcasecmp.h"
#include "paramstr_parser.h"

#include "cx-starter_cfg.h"


//--------------------------------------------------------------------

// GetCfglSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(, Cfgl, cfgline_t,
                                 clns, in_use, 0, 1,
				 0, 10, 0,
				 cfg->, cfg,
				 cfgbase_t *cfg, cfgbase_t *cfg)

void RlsCfglSlot(int id, cfgbase_t *cfg)
{
  cfgline_t *p = AccessCfglSlot(id, cfg);

    p->in_use = 0;
}

//--------------------------------------------------------------------

// GetSrvpSlot()
GENERIC_SLOTARRAY_DEFINE_GROWING(, Srvp, srvparams_t,
                                 srvs, in_use, 0, 1,
				 0, 10, 0,
				 cfg->, cfg,
				 cfgbase_t *cfg, cfgbase_t *cfg)

void RlsSrvpSlot(int id, cfgbase_t *cfg)
{
  srvparams_t *p = AccessSrvpSlot(id, cfg);

    p->in_use = 0;
}

//--------------------------------------------------------------------




enum {MAXCMDLEN = 1024};
static psp_paramdescr_t text2cfgline[] =
{
    PSP_P_STRING ("label",         cfgline_t, label,         NULL),
    PSP_P_FLAG   ("cx",            cfgline_t, kind,          SUBSYS_CX,   1),
    PSP_P_FLAG   ("foreign",       cfgline_t, kind,          SUBSYS_FRGN, 0),
    PSP_P_FLAG   ("-process-",     cfgline_t, noprocess,    -1,           1),
    PSP_P_FLAG   ("noprocess",     cfgline_t, noprocess,     1,           0),
    PSP_P_FLAG   ("doprocess",     cfgline_t, noprocess,     0,           0),
    PSP_P_FLAG   ("ignorevals",    cfgline_t, ignorevals,    1,           0),
    PSP_P_MSTRING("app_name",      cfgline_t, app_name,      NULL, 200),
    PSP_P_MSTRING("title",         cfgline_t, title,         NULL, 200),
    PSP_P_MSTRING("comment",       cfgline_t, comment,       NULL, 200),
    PSP_P_MSTRING("chaninfo",      cfgline_t, chaninfo,      NULL, 1000),
    PSP_P_MSTRING("start",         cfgline_t, start_cmd,     NULL, MAXCMDLEN),
    PSP_P_MSTRING("stop",          cfgline_t, stop_cmd,      NULL, MAXCMDLEN),
    PSP_P_MSTRING("params",        cfgline_t, params,        NULL, MAXCMDLEN),
    PSP_P_END()
};

static psp_paramdescr_t text2srvparams[] =
{
    PSP_P_MSTRING("start",  srvparams_t, start_cmd,     NULL, MAXCMDLEN),
    PSP_P_MSTRING("stop",   srvparams_t, stop_cmd,      NULL, MAXCMDLEN),
    PSP_P_MSTRING("user",   srvparams_t, user,          NULL, 32),
    PSP_P_MSTRING("params", srvparams_t, params,        NULL, MAXCMDLEN),
    PSP_P_END()
};


cfgbase_t  * CfgBaseCreate (int option_noprocess)
{
  cfgbase_t *cfg;

    cfg = malloc(sizeof(*cfg));
    if (cfg == NULL) return NULL;

    bzero(cfg, sizeof(*cfg));
    cfg->option_noprocess = option_noprocess;

    return cfg;
}

void         CfgBaseClear  (cfgbase_t *cfg)
{
}

void         CfgBaseDestroy(cfgbase_t *cfg)
{
}

typedef struct
{
    const char *s;
    size_t      len;
} duplet_t;
static int cfgl_name_eq_checker(cfgline_t *p, void *privptr)
{
  duplet_t *dpl = privptr;

    return cx_strmemcasecmp(p->name, dpl->s, dpl->len) == 0;
}
static int srvp_name_eq_checker(srvparams_t *p, void *privptr)
{
  duplet_t *dpl = privptr;

    return cx_strmemcasecmp(p->name, dpl->s, dpl->len) == 0;
}
int          CfgBaseMerge  (cfgbase_t *cfg, const char *argv0, const char *path)
{
#define CFG_SEPARATORS " \t"
#define CFG_COMMENT    "#"
    
  FILE        *fp;
  char         linebuf[2000];
  const char  *linep;
  int          lineno = 0;
  const char  *s_p;
  int          namelen;

  duplet_t     dpl;

  int          id;
  cfgline_t   *cp;

  int          srv;
  srvparams_t *srvp;

  int          usecs;

    fp = fopen(path, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "%s %s: FATAL ERROR: unable to open requested config \"%s\": %s\n",
	        strcurtime(), argv0, path, strerror(errno));
        return -1;
    }

    while (fgets(linebuf, sizeof(linebuf), fp) != NULL)
    {
        /* Cut newline and check for '\' at end-of-line */
        do
        {
            lineno++;
            
            if (linebuf[0] == '\0') break;
            if (linebuf[strlen(linebuf) - 1] == '\n') linebuf[strlen(linebuf) - 1] = '\0';
            if (linebuf[strlen(linebuf) - 1] != '\\') break;

            linebuf[strlen(linebuf) - 1] = ' ';
            
            if (strlen(linebuf) >= sizeof(linebuf) - 2)
            {
                fprintf(stderr, "%s: %s:%d: FATAL ERROR: input buffer already full\n",
                        argv0, path, lineno);
                exit(1);
            }

            if (fgets(linebuf+strlen(linebuf),
                      sizeof(linebuf)-strlen(linebuf),
                      fp) == NULL)
            {
                fprintf(stderr, "%s: %s:%d: END-OF-FILE encountered after '\\'-line-continuation\n",
                        argv0, path, lineno);
                break;
            }
        } while (1);

        /* Skip leading whitespace */
        linep = linebuf;
        linep += strspn(linep, CFG_SEPARATORS);

        /* An empty/full-comment line? */
        if (*linep == '\0'  ||  *linep == '#') continue;

        if (*linep == '.')
        {
            linep++;

            /* Extract the keyword... */
            s_p = linep;
            while (*linep == '_'  ||  (linep != s_p  &&  *linep == '-')  ||
                   isalnum(*linep))
                linep++;
            if (s_p == linep)
            {
                fprintf(stderr, "%s %s: %s:%d@%d: missing .-keyword\n",
                        strcurtime(), argv0, path, lineno, (int)(s_p - linebuf + 1));
                return -1;
            }
            
            /* What a .-directive can it be? */
            if      (cx_strmemcasecmp("srvparams", s_p, linep - s_p) == 0)
            {
                linep += strspn(linep, CFG_SEPARATORS);

                s_p = linep;
                linep += strcspn(linep, CFG_SEPARATORS);
                if (s_p == linep)
                {
                    fprintf(stderr, "%s: %s:%d@%d: missing server spec for .srvparams\n",
                            argv0, path, lineno, (int)(s_p - linebuf + 1));
                    exit(1);
                }

                dpl.s = s_p; dpl.len = linep - s_p;
                srv = ForeachSrvpSlot(srvp_name_eq_checker, &dpl, cfg);
		if (srv < 0)
		{
		    srv = GetSrvpSlot(cfg);
		    if (srv < 0) /*!!! "out of memory", FATAL */
		    {
                        fprintf(stderr, "%s: %s:%d@%d: too many .srvparams\n",
                                argv0, path, lineno, (int)(s_p - linebuf + 1));
                        goto NEXT_LINE;
		    }
		    srvp = AccessSrvpSlot(srv, cfg);
		    srvp->name = malloc((linep - s_p) + 1);
                    memcpy(srvp->name, s_p, linep - s_p);
                    srvp->name[linep - s_p] = '\0';
		}
                srvp = AccessSrvpSlot(srv, cfg);

                if (psp_parse(linep, NULL,
                              srvp,
                              '=', CFG_SEPARATORS, CFG_COMMENT,
                              text2srvparams) != PSP_R_OK)
                {
                    fprintf(stderr, "%s: %s:%d: %s\n",
                            argv0, path, lineno, psp_error());
                    exit(1);
                }
            }
            else if (cx_strmemcasecmp("noprocess", s_p, linep - s_p) == 0)
                cfg->cfg_noprocess = 1;
            else if (cx_strmemcasecmp("doprocess", s_p, linep - s_p) == 0)
                cfg->cfg_noprocess = 0;
            else
            {
                fprintf(stderr, "%s: %s:%d@%d: unknown .-directive\n",
                        argv0, path, lineno, (int)(s_p - linebuf + 1));
                goto NEXT_LINE;
            }
        }
        else
        {
	    id = GetCfglSlot(cfg);
	    if (id < 0) /*!!! "out of memory", FATAL */
            {
                fprintf(stderr, "%s: sorry, too many systems upon reading \"%s\"\n",
                        argv0, path);
                goto END_PARSE;
            }

            cp = AccessCfglSlot(id, cfg);

            /* Extract the keyword... */
            s_p = linep;
            while (*linep == '_'  ||  (/*linep != s_p  &&*/  *linep == '-')  ||
                   isalnum(*linep))
                linep++;

            if (s_p == linep)
            {
                fprintf(stderr, "%s: %s:%d@%d: missing system name\n",
                        argv0, path, lineno, (int)(s_p - linebuf + 1));
                exit(1);
            }

            namelen = linep - s_p;
            if (namelen > sizeof(cp->name) - 1) namelen = sizeof(cp->name) - 1;
            memcpy(cp->name, s_p, namelen);
            cp->name[namelen] = '\0';

            if (cp->name[0] == '-')
            {
                cp->kind = SUBSYS_SEPR;
                if (cp->name[1] == '-')
                {
                    cp->kind = SUBSYS_SEPR2;
                    if (cp->name[2] == '-')
                    {
                        cp->kind = SUBSYS_NEWCOL;
                    }
                }

                goto NEXT_LINE;
            }

            /* Okay, what's than? */
            linep += strspn(linep, CFG_SEPARATORS);
            if (*linep == '\0'  ||  *linep == '#')
                psp_parse(NULL, NULL,
                          cp,
                          '=', "", CFG_COMMENT,
                          text2cfgline);
            else
            {
                if (psp_parse(linep, NULL,
                              cp,
                              '=', CFG_SEPARATORS, CFG_COMMENT,
                              text2cfgline) != PSP_R_OK)
                {
                    fprintf(stderr, "%s: %s:%d: %s\n",
                            argv0, path, lineno, psp_error());
                    exit(1);
                }

                /* Check validity... */
                if (cp->kind != SUBSYS_CX)
                {
                    if (cp->start_cmd == NULL)
                        fprintf(stderr, "%s: %s:%d: warning: no 'start' command specified for '%s'\n",
                                argv0, path, lineno, cp->name);
                }
            }

            /* Handle noprocess/doprocess flags */
            if (cp->noprocess < 0) cp->noprocess = cfg->cfg_noprocess;
            if (cfg->option_noprocess) cp->noprocess = 1;
        }
 NEXT_LINE:;
    }
 END_PARSE:;

    fclose(fp);

    return 0;
}
