#include <stdio.h>

#include "ppf4td.h"
#include "ppf4td_pipe.h"
#include "ppf4td_cpp.h"


static int ppf4td_cpp_open (ppf4td_ctx_t *ctx, const char *reference)
{
  static const char *model[] = {"/usr/bin/cpp"};

  const char        *cmdline[countof(model) + 2];
  int                i;

    for (i = 0;  i < countof(model);  i++) cmdline[i] = model[i];
    cmdline[i++] = reference;
    cmdline[i++] = NULL;

    return ppf4td_pipe_open(ctx, cmdline);
}

static int ppf4td_cpp_close(ppf4td_ctx_t *ctx)
{
    return ppf4td_pipe_close(ctx);
}

static int ppf4td_cpp_peekc(ppf4td_ctx_t *ctx, int *ch_p)
{
    return ppf4td_pipe_peekc(ctx, ch_p);
}

static int ppf4td_cpp_nextc(ppf4td_ctx_t *ctx, int *ch_p)
{
    return ppf4td_pipe_nextc(ctx, ch_p);
}

static int  ppf4td_cpp_init_m(void);
static void ppf4td_cpp_term_m(void);

DEFINE_PPF4TD_PLUGIN(cpp, "CPP PPF4TD plugin",
                     ppf4td_cpp_init_m, ppf4td_cpp_term_m,
                     ppf4td_cpp_open,   ppf4td_cpp_close,
                     ppf4td_cpp_peekc,  ppf4td_cpp_nextc,
                     PPF4TD_LINESYNC_HASH);

static int  ppf4td_cpp_init_m(void)
{
    return ppf4td_register_plugin(&(PPF4TD_PLUGIN_MODREC_NAME(cpp)));
}

static void ppf4td_cpp_term_m(void)
{
    ppf4td_deregister_plugin(&(PPF4TD_PLUGIN_MODREC_NAME(cpp)));
}
