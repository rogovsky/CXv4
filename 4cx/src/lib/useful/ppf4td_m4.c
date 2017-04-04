#include <stdio.h>
// Next 2 are for PATH_MAX -- different Linux versions place it in different places
#include <limits.h>
#include <sys/param.h>

#include "ppf4td.h"
#include "ppf4td_pipe.h"
#include "ppf4td_m4.h"


static int ppf4td_m4_open (ppf4td_ctx_t *ctx, const char *reference)
{
  static const char *model[] = {"/usr/bin/m4", "-s"};

  const char        *cmdline[countof(model) + 2 + 2];
  int                i;

  const char        *sl_p;
  size_t             path_len;
  char               Ipath[PATH_MAX];

    /*  */
    for (i = 0;  i < countof(model);  i++) cmdline[i] = model[i];
    /* Optional -I DIR */
    sl_p = strrchr(reference, '/');
    if (sl_p != NULL)
    {
        path_len = (sl_p - reference) + 1 /* "+1" for slash itself */;
        if (path_len < sizeof(Ipath) - 1)
        {
            memcpy(Ipath, reference, path_len); Ipath[path_len] = '\0';
            cmdline[i++] = "-I";
            cmdline[i++] = Ipath;
        }
    }
    /* File name and terminating NULL */
    cmdline[i++] = reference;
    cmdline[i++] = NULL;

    return ppf4td_pipe_open(ctx, cmdline);
}

static int ppf4td_m4_close(ppf4td_ctx_t *ctx)
{
    return ppf4td_pipe_close(ctx);
}

static int ppf4td_m4_peekc(ppf4td_ctx_t *ctx, int *ch_p)
{
    return ppf4td_pipe_peekc(ctx, ch_p);
}

static int ppf4td_m4_nextc(ppf4td_ctx_t *ctx, int *ch_p)
{
    return ppf4td_pipe_nextc(ctx, ch_p);
}

static int  ppf4td_m4_init_m(void);
static void ppf4td_m4_term_m(void);

DEFINE_PPF4TD_PLUGIN(m4, "M4 PPF4TD plugin",
                     ppf4td_m4_init_m, ppf4td_m4_term_m,
                     ppf4td_m4_open,   ppf4td_m4_close,
                     ppf4td_m4_peekc,  ppf4td_m4_nextc,
                     PPF4TD_LINESYNC_HLIN);

static int  ppf4td_m4_init_m(void)
{
    return ppf4td_register_plugin(&(PPF4TD_PLUGIN_MODREC_NAME(m4)));
}

static void ppf4td_m4_term_m(void)
{
    ppf4td_deregister_plugin(&(PPF4TD_PLUGIN_MODREC_NAME(m4)));
}
