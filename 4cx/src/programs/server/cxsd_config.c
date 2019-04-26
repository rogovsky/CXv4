#define __CXSD_CONFIG_C
#include "cxsd_includes.h"

#include "ppf4td.h"


static const char *dash_e_params[100];
static int         dash_e_params_used = 0;


/* ==== Command line parsing ====================================== */

/*
 *  ParseCommandLine
 *      Parses the command line options.
 *
 *  Effect:
 *      Changes the 'option_*' variables according to the -?s.
 *      Prints the help message and exits if '-h' encountered.
 *      Prints an error message to stderr and exits if illegal
 *      option is encountered.
 */

void ParseCommandLine(int argc, char *argv[])
{
  __label__  ADVICE_HELP, PRINT_HELP;

  int            c;              /* Option character */
  int            err       = 0;  /* ++'ed on errors */
  unsigned long  instance;
  char          *endptr;

  int            log_set_mask;
  int            log_clr_mask;
  char          *log_parse_r;

    while ((c = getopt(argc, argv, "b:c:dDe:f:hl:nsSv:w:")) != EOF)
    {
        switch (c)
        {
            case 'b':
                option_basecyclesize = strtoul(optarg, &endptr, 10);
                if (endptr == optarg  ||  *endptr != '\0')
                {
                    fprintf(stderr,
                            "%s: '-b' argument should be an integer (in us)\n",
                            argv[0]);
                    err++;
                }
                break;
                
            case 'c':
                option_configfile = optarg;
                break;
                
            case 'd':
                option_dontdaemonize = 1;
                break;
                
            case 'D':
                option_donttrapsignals = 1;
                break;

            case 'e':
                if (dash_e_params_used >= countof(dash_e_params))
                {
                    fprintf(stderr,
                            "%s: too many '-e's\n", argv[0]);
                    err++;
                }
                else
                {
                    dash_e_params[dash_e_params_used++] = optarg;
                }
                break;

            case 'f':
                option_dbref = optarg;
                break;

            case 'h':
                goto PRINT_HELP;

            case 'l':
                log_parse_r = ParseDrvlogCategories(optarg, NULL,
                                                    &log_set_mask, &log_clr_mask);
                if (log_parse_r != NULL)
                {
                    fprintf(stderr,
                            "%s: error parsing -l switch: %s\n",
                            argv[0], log_parse_r);
                    err++;
                }
                option_defdrvlog_mask =
                    (option_defdrvlog_mask &~ log_clr_mask) | log_set_mask;
                break;

            case 'n':
                option_norun = 1;
                break;

            case 's':
                option_simulate = CXSD_SIMULATE_YES;
                break;

            case 'S':
                option_simulate = CXSD_SIMULATE_SUP;
                break;

            case 'v':
                if (*optarg >= '0'  &&  *optarg <= '9'  &&
                    optarg[1] == '\0')
                    option_verbosity = *optarg - '0';
                else
                {
                    fprintf(stderr,
                            "%s: argument of `-v' should be between 0 and 9\n",
                            argv[0]);
                    err++;
                }
                break;

            case 'w':
                option_cacherfw = 1;
                if (tolower(*optarg) == 'n'  ||  tolower(*optarg) == 'f'  ||
                    *optarg == '0')
                    option_cacherfw = 0;
                break;
                
            case '?':
            default:
                err++;
        }
    }
    
    if (err) goto ADVICE_HELP;

    if (optind < argc  &&  argv[optind] != NULL)
    {
        if (argv[optind][0] == ':')
        {
            instance = strtoul(&(argv[optind][1]), &endptr, 10);
            if (endptr == &(argv[optind][1])  ||  *endptr != '\0')
            {
                fprintf(stderr, "%s: invalid <server> specification\n", argv[0]);
                goto ADVICE_HELP;
            }
            if (instance > CX_MAX_SERVER)
            {
                fprintf(stderr, "%s: server specification out of range 0-%d\n",
                        argv[0], CX_MAX_SERVER);
                normal_exit = 1;
                exit(1);
            }

            option_instance = instance;
            optind++;
        }
    }
    
    if (optind < argc)
    {
        fprintf(stderr, "%s: unexpected argument(s)\n", argv[0]);
        goto ADVICE_HELP;
    }

    return;

 ADVICE_HELP:
    fprintf(stderr, "Try `%s -h' for more information.\n", argv[0]);
    normal_exit = 1;
    exit(1);

 PRINT_HELP:
    printf("Usage: %s [options] [:<server>]\n"
           "\n"
           "Options:\n"
           "  -b TIME   server base cycle size, in microseconds (def=%d)\n"
           "  -c FILE   use FILE instead of " DEF_FILE_CXSD_CONF "\n"
           "  -d        don't daemonize, i.e. keep console and don't fork()\n"
           "  -D        don't trap signals\n"
           "  -e CFG_L  execute CFG_L as if read from config file (AFTER cfg.file)\n"
           "  -f DBREF  obtain HW config from DBREF instead of " DEF_FILE_DEVLIST "\n"
           "  -h        display this help and exit\n"
           "  -n        don't actually run, just parse config and devlist\n"
           "  -l LIST   default drvlog categories\n"
           "  -s        simulate hardware; for debugging only\n"
           "  -S        super-simulate (as -s, plus ro-channels become rw)\n"
           "  -v N      verbosity level: 0 - lowest, 9 - highest (def=%d)\n"
           "  -w Y/N    cache reads from write channels (def=%s)\n"
           "<server> is an integer number of server instance\n"
           , argv[0],
           DEF_OPT_BASECYCLESIZE,
           DEF_OPT_VERBOSITY,
           DEF_OPT_CACHERFW? "yes" : "no");
    normal_exit = 1;
    exit(0);
}

//////////////////////////////////////////////////////////////////////

#define BARK(format, params...)         \
    (fprintf(stderr, "%s %s: %s:%d: ",  \
             strcurtime(), argv0, ppf4td_cur_ref(ctx), ppf4td_cur_line(ctx)), \
     fprintf(stderr, format, ##params), \
     fprintf(stderr, "\n"),             \
     -1)

static int ParseXName(const char *argv0, ppf4td_ctx_t *ctx,
                      int flags,
                      const char *name, char *buf, size_t bufsize)
{
  int  r;

    r = ppf4td_get_ident(ctx, flags, buf, bufsize);
    if (r < 0)
        return BARK("%s expected; %s\n", name, ppf4td_strerror(errno));

    return 0;
}

static int ParseAName(const char *argv0, ppf4td_ctx_t *ctx,
                      const char *name, char *buf, size_t bufsize)
{
    return ParseXName(argv0, ctx, PPF4TD_FLAG_NONE, name, buf, bufsize);
}

static int ParseDName(const char *argv0, ppf4td_ctx_t *ctx,
                      const char *name, char *buf, size_t bufsize)
{
    return ParseXName(argv0, ctx, PPF4TD_FLAG_DASH, name, buf, bufsize);
}

static int ParseAPath(const char *argv0, ppf4td_ctx_t *ctx,
                      const char *name, char *buf, size_t bufsize)
{
  int  r;

    r = ppf4td_get_string(ctx, PPF4TD_FLAG_HSHTERM | PPF4TD_FLAG_SPCTERM,
                          buf, bufsize);
    if (r < 0)
        return BARK("%s expected; %s\n", name, ppf4td_strerror(errno));
    ////fprintf(stderr, "%s:=<%s>\n", name, buf);
    if (strlen(buf) == 0)
        return BARK("non-empty %s expected\n", name);

    return 0;
}

typedef struct
{
    const char  *name;
    int        (*parser)(const char *argv0, ppf4td_ctx_t *ctx);
} keyworddef_t;

//////////////////////////////////////////////////////////////////////

static int setenv_parser(const char *argv0, ppf4td_ctx_t *ctx)
{
  int           r;
  int           ch;

  char  envname[100];
  char  envval [500];

    if (ParseAName(argv0, ctx,
                   "envvar-name", envname, sizeof(envname)) < 0) return -1;

    /* Skip whitespace and a single optional '=' */
    ppf4td_skip_white(ctx);
    if (ppf4td_peekc(ctx, &ch) > 0  &&  ch == '=') ppf4td_nextc(ctx, &ch);
    ppf4td_skip_white(ctx);

    r = ppf4td_get_string(ctx, PPF4TD_FLAG_HSHTERM | PPF4TD_FLAG_SPCTERM,
                          envval, sizeof(envval));
    if (r < 0)
        return BARK("envvar-value expected; %s\n", ppf4td_strerror(errno));

    ////fprintf(stderr, "<%s>=<%s>\n", envname, envval);
    setenv(envname, envval, 1);

    return 0;
}

static int pid_file_dir_parser(const char *argv0, ppf4td_ctx_t *ctx)
{
    return ParseAPath(argv0, ctx,
                      "pid-file-dir", config_pid_file_dir, sizeof(config_pid_file_dir));
}

static int drivers_path_parser(const char *argv0, ppf4td_ctx_t *ctx)
{
    return ParseAPath(argv0, ctx,
                      "drivers-path", config_drivers_path, sizeof(config_drivers_path));
}

static int frontends_path_parser(const char *argv0, ppf4td_ctx_t *ctx)
{
  int  r;

    r = ParseAPath(argv0, ctx,
                   "frontends-path", config_frontends_path, sizeof(config_frontends_path));
    if (r < 0) return r;
    CxsdSetFndPath(config_frontends_path);
    return 0;
}

static int exts_path_parser(const char *argv0, ppf4td_ctx_t *ctx)
{
  int  r;

    r = ParseAPath(argv0, ctx,
                   "exts-path", config_exts_path, sizeof(config_exts_path));
    if (r < 0) return r;
    CxsdSetExtPath(config_exts_path);
    return 0;
}

static int libs_path_parser(const char *argv0, ppf4td_ctx_t *ctx)
{
  int  r;

    r = ParseAPath(argv0, ctx,
                   "libs-path", config_libs_path, sizeof(config_libs_path));
    if (r < 0) return r;
    CxsdSetLibPath(config_libs_path);
    return 0;
}

static int cxhosts_file_parser(const char *argv0, ppf4td_ctx_t *ctx)
{
  int   r;
  FILE *fp;

    r =    ParseAPath(argv0, ctx,
                      "cxhosts-file", config_cxhosts_file, sizeof(config_cxhosts_file));
    if (r < 0) return r;

    fp = fopen(config_cxhosts_file, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "%s %s: %s:%d: WARNING: fopen(\"%s\"): %s",
                strcurtime(), argv0,
                ppf4td_cur_ref(ctx), ppf4td_cur_line(ctx),
                config_cxhosts_file, strerror(errno));
        fprintf(stderr, "\n");
    }
    else
        fclose(fp);

    return 0;
}

static int load_frontend_parser(const char *argv0, ppf4td_ctx_t *ctx)
{
  char  name[100];

    if (ParseAName(argv0, ctx,
                   "frontend-name", name, sizeof(name)) < 0) return -1;

    return (CxsdLoadFrontend(argv0, name, NULL) < 0)? -1 : 0;
}

static int load_ext_parser(const char *argv0, ppf4td_ctx_t *ctx)
{
  char  name[100];

    if (ParseAName(argv0, ctx,
                   "ext-name", name, sizeof(name)) < 0) return -1;

    return (CxsdLoadExt(argv0, name) < 0)? -1 : 0;
}

static int load_lib_parser(const char *argv0, ppf4td_ctx_t *ctx)
{
  char  name[100];

    if (ParseAName(argv0, ctx,
                   "lib-name", name, sizeof(name)) < 0) return -1;

    return (CxsdLoadLib(argv0, name) < 0)? -1 : 0;
}

static int log_parser(const char *argv0, ppf4td_ctx_t *ctx)
{
  int           r;
  int           ch;

  char  name[100];
  int   category;
  int   cat_mask;
  char  path[PATH_MAX];
  char *setf_rslt;

  const char *categories[] =
  {
      [LOGF_SYSTEM]   = "system",
      [LOGF_ACCESS]   = "access",
      [LOGF_DEBUG]    = "debug",
      [LOGF_MODULES]  = "modules",
      [LOGF_HARDWARE] = "hardware",
  };

    for (cat_mask = 0; ;)
    {
        if (ParseAName(argv0, ctx,
                       "log-category-name", name, sizeof(name)) < 0) return -1;
        if (strcasecmp(name, "all") == 0)
            cat_mask = -1;
        else
        {
            for (category = 0;  category < countof(categories);  category++)
                if (categories[category] != NULL  &&
                    strcasecmp(name, categories[category]) == 0) break;
            if (category >= countof(categories))
                return BARK("unknown log-category-name \"%s\"\n", name);
            cat_mask |= (1 << category);
        }

        r = ppf4td_peekc(ctx, &ch);
        if (r < 0) return -1;
        if (r > 0  &&  ch == ',')
        {
            ppf4td_nextc(ctx, &ch);
        }
        else
            break;
    }

    ppf4td_skip_white(ctx);
    r = ParseAPath(argv0, ctx,
                   "log-file-name", path, sizeof(path));
    if (r < 0) return r;

    setf_rslt = cxsd_log_setf(cat_mask, path);
    if (setf_rslt != NULL)
        return BARK("can't cxsd_log_setf(\"%s\"): %s\n", name, setf_rslt);

    return 0;
}

static keyworddef_t keyword_list[] =
{
    {"setenv",         setenv_parser},
    {"pid-file-dir",   pid_file_dir_parser},
    {"drivers-path",   drivers_path_parser},
    {"frontends-path", frontends_path_parser},
    {"exts-path",      exts_path_parser},
    {"libs-path",      libs_path_parser},
    {"cxhosts-file",   cxhosts_file_parser},
    {"load-frontend",  load_frontend_parser},
    {"load-ext",       load_ext_parser},
    {"load-lib",       load_lib_parser},
    {"log",            log_parser},
    {NULL, NULL}
};

void ReadConfig      (const char *argv0,
                      const char *def_scheme, const char *reference)
{
  ppf4td_ctx_t  ctx;
  int           r;
  int           ch;

  char          keyword[50];
  keyworddef_t *kdp;

    r = ppf4td_open(&ctx, def_scheme, reference);
    if (r != 0)
    {
        fprintf(stderr, "%s %s: failed to open \"%s\": %s\n",
                strcurtime(), argv0, reference, ppf4td_strerror(errno));
        goto FATAL;
    }

    while (1)
    {
        ppf4td_skip_white(&ctx);
        if (ppf4td_is_at_eol(&ctx)) goto SKIP_TO_NEXT_LINE;
        r = ppf4td_peekc(&ctx, &ch);
        ////fprintf(stderr, "!!!: ch='%c'\n", ch);
        if (r <= 0  ||  ch == EOF) goto END_PARSE_FILE;

        if (ch == '#') goto SKIP_TO_NEXT_LINE;

        if (ParseDName(argv0, &ctx, "keyword", keyword, sizeof(keyword)) < 0) goto FATAL;

        for (kdp = keyword_list;  kdp->name != NULL;  kdp++)
            if (strcasecmp(keyword, kdp->name) == 0) break;
        if (kdp->name == NULL)
        {
            fprintf(stderr, "%s %s: %s:%d: unknown keyword \"%s\"\n",
                    strcurtime(), argv0, ppf4td_cur_ref(&ctx), ppf4td_cur_line(&ctx),
                    keyword);
            goto FATAL;
        }

        ppf4td_skip_white(&ctx);
        r = kdp->parser(argv0, &ctx);
        if (r != 0) goto FATAL;

 SKIP_TO_NEXT_LINE:
     ////fprintf(stderr, "SKIP!\n");
        /* First, skip everything till EOL */
        while (ppf4td_peekc(&ctx, &ch) > 0  &&
               ch != '\r'  &&  ch != '\n')
            ppf4td_nextc(&ctx, &ch)/*, fprintf(stderr, "\t(%c)\n", ch)*/;
        /* Than skip as many CR/LFs as possible */
        while (ppf4td_peekc(&ctx, &ch) > 0  &&
               (ch == '\r'  ||  ch == '\n'))
            ppf4td_nextc(&ctx, &ch)/*, fprintf(stderr, "\t{%c}\n", ch)*/;

        /* Check for EOF */
        if (ppf4td_peekc(&ctx, &ch) <= 0) goto END_PARSE_FILE;
    }
 END_PARSE_FILE:

    ppf4td_close(&ctx);
    return;

 FATAL:
    normal_exit = 1;
    exit(2);
}

void UseCmdConfigs   (const char *argv0)
{
  int  x;

    for (x = 0;  x < dash_e_params_used;  x++)
        ReadConfig(argv0, "!mem", dash_e_params[x]);
}
